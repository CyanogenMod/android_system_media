/*
 * Copyright (C) 2011 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

//#define USE_LOG SLAndroidLogLevel_Verbose

#include "sles_allinclusive.h"
#include "android/android_AudioSfDecoder.h"

#include <media/stagefright/foundation/ADebug.h>


#define SIZE_CACHED_HIGH_BYTES 1000000
#define SIZE_CACHED_MED_BYTES   700000
#define SIZE_CACHED_LOW_BYTES   400000

namespace android {

//--------------------------------------------------------------------------------------------------
AudioSfDecoder::AudioSfDecoder(const AudioPlayback_Parameters* params) : GenericPlayer(params),
        mDataSource(0),
        mAudioSource(0),
        mBitrate(-1),
        mNumChannels(1),
        mSampleRateHz(0),
        mDurationUsec(-1),
        mDecodeBuffer(NULL),
        mTimeDelta(-1),
        mSeekTimeMsec(0),
        mLastDecodedPositionUs(-1)
{
    SL_LOGV("AudioSfDecoder::AudioSfDecoder()");

}


AudioSfDecoder::~AudioSfDecoder() {
    SL_LOGV("AudioSfDecoder::~AudioSfDecoder()");
    if (mAudioSource != 0) {
        mAudioSource->stop();
    }
}


//--------------------------------------------------
void AudioSfDecoder::play() {
    SL_LOGV("AudioSfDecoder::play");

    GenericPlayer::play();
    (new AMessage(kWhatDecode, id()))->post();
}


void AudioSfDecoder::startPrefetch_async() {
    SL_LOGV("AudioSfDecoder::startPrefetch_async()");

    if (wantPrefetch()) {
        SL_LOGV("AudioSfDecoder::startPrefetch_async(): sending check cache msg");

        mStateFlags |= kFlagPreparing | kFlagBuffering;

        (new AMessage(kWhatCheckCache, id()))->post();
    }
}


//--------------------------------------------------
// Event handlers
void AudioSfDecoder::onPrepare() {
    SL_LOGD("AudioSfDecoder::onPrepare()");

    sp<DataSource> dataSource;

    switch (mDataLocatorType) {

    case kDataLocatorNone:
        SL_LOGE("AudioSfDecoder::onPrepare: no data locator set");
        notifyPrepared(MEDIA_ERROR_BASE);
        return;

    case kDataLocatorUri:
        dataSource = DataSource::CreateFromURI(mDataLocator.uriRef);
        if (dataSource == NULL) {
            SL_LOGE("AudioSfDecoder::onPrepare(): Error opening %s", mDataLocator.uriRef);
            notifyPrepared(MEDIA_ERROR_BASE);
            return;
        }
        break;

    case kDataLocatorFd:
    {
        dataSource = new FileSource(
                mDataLocator.fdi.fd, mDataLocator.fdi.offset, mDataLocator.fdi.length);
        status_t err = dataSource->initCheck();
        if (err != OK) {
            notifyPrepared(err);
            return;
        }
        break;
    }

    default:
        TRESPASS();
    }

    sp<MediaExtractor> extractor = MediaExtractor::Create(dataSource);
    if (extractor == NULL) {
        SL_LOGE("AudioSfDecoder::onPrepare: Could not instantiate extractor.");
        notifyPrepared(ERROR_UNSUPPORTED);
        return;
    }

    ssize_t audioTrackIndex = -1;
    bool isRawAudio = false;
    for (size_t i = 0; i < extractor->countTracks(); ++i) {
        sp<MetaData> meta = extractor->getTrackMetaData(i);

        const char *mime;
        CHECK(meta->findCString(kKeyMIMEType, &mime));

        if (!strncasecmp("audio/", mime, 6)) {
            audioTrackIndex = i;

            if (!strcasecmp(MEDIA_MIMETYPE_AUDIO_RAW, mime)) {
                isRawAudio = true;
            }
            break;
        }
    }

    if (audioTrackIndex < 0) {
        SL_LOGE("AudioSfDecoder::onPrepare: Could not find a supported audio track.");
        notifyPrepared(ERROR_UNSUPPORTED);
        return;
    }

    sp<MediaSource> source = extractor->getTrack(audioTrackIndex);
    sp<MetaData> meta = source->getFormat();

    off64_t size;
    int64_t durationUs;
    if (dataSource->getSize(&size) == OK
            && meta->findInt64(kKeyDuration, &durationUs)) {
        mBitrate = size * 8000000ll / durationUs;  // in bits/sec
        mDurationUsec = durationUs;
    } else {
        mBitrate = -1;
        mDurationUsec = -1;
    }

    if (!isRawAudio) {
        OMXClient client;
        CHECK_EQ(client.connect(), (status_t)OK);

        source = OMXCodec::Create(
                client.interface(), meta, false /* createEncoder */,
                source);

        if (source == NULL) {
            SL_LOGE("AudioSfDecoder::onPrepare: Could not instantiate decoder.");
            notifyPrepared(ERROR_UNSUPPORTED);
            return;
        }

        meta = source->getFormat();
    }


    if (source->start() != OK) {
        SL_LOGE("AudioSfDecoder::onPrepare: Failed to start source/decoder.");
        notifyPrepared(MEDIA_ERROR_BASE);
        return;
    }

    mDataSource = dataSource;
    mAudioSource = source;

    CHECK(meta->findInt32(kKeyChannelCount, &mNumChannels));
    CHECK(meta->findInt32(kKeySampleRate, &mSampleRateHz));

    if (!wantPrefetch()) {
        SL_LOGV("AudioSfDecoder::onPrepare: no need to prefetch");
        // doesn't need prefetching, notify good to go
        mCacheStatus = kStatusHigh;
        mCacheFill = 1000;
        notifyStatus();
        notifyCacheFill();
    }

    // at this point we have enough information about the source to create the sink that
    // will consume the data
    createAudioSink();

    GenericPlayer::onPrepare();
    SL_LOGD("AudioSfDecoder::onPrepare() done, mStateFlags=0x%x", mStateFlags);
}


void AudioSfDecoder::onPause() {
    SL_LOGD("AudioSfDecoder::onPause()");
    GenericPlayer::onPause();
    pauseAudioSink();
}


void AudioSfDecoder::onPlay() {
    SL_LOGD("AudioSfDecoder::onPlay()");
    GenericPlayer::onPlay();
    startAudioSink();
}


void AudioSfDecoder::onSeek(const sp<AMessage> &msg) {
    SL_LOGV("AudioSfDecoder::onSeek");
    int64_t timeMsec;
    CHECK(msg->findInt64(WHATPARAM_SEEK_SEEKTIME_MS, &timeMsec));

    Mutex::Autolock _l(mSeekLock);
    mStateFlags |= kFlagSeeking;
    mSeekTimeMsec = timeMsec;
    mTimeDelta = -1;
    mLastDecodedPositionUs = -1;
}


void AudioSfDecoder::onLoop(const sp<AMessage> &msg) {
    SL_LOGV("AudioSfDecoder::onLoop");
    int32_t loop;
    CHECK(msg->findInt32(WHATPARAM_LOOP_LOOPING, &loop));

    if (loop) {
        //SL_LOGV("AudioSfDecoder::onLoop start looping");
        mStateFlags |= kFlagLooping;
    } else {
        //SL_LOGV("AudioSfDecoder::onLoop stop looping");
        mStateFlags &= ~kFlagLooping;
    }
}


void AudioSfDecoder::onCheckCache(const sp<AMessage> &msg) {
    //SL_LOGV("AudioSfDecoder::onCheckCache");
    bool eos;
    CacheStatus_t status = getCacheRemaining(&eos);

    if (eos || status == kStatusHigh
            || ((mStateFlags & kFlagPreparing) && (status >= kStatusEnough))) {
        if (mStateFlags & kFlagPlaying) {
            startAudioSink();
        }
        mStateFlags &= ~kFlagBuffering;

        SL_LOGV("AudioSfDecoder::onCheckCache: buffering done.");

        if (mStateFlags & kFlagPreparing) {
            //SL_LOGV("AudioSfDecoder::onCheckCache: preparation done.");
            mStateFlags &= ~kFlagPreparing;
        }

        mTimeDelta = -1;
        if (mStateFlags & kFlagPlaying) {
            (new AMessage(kWhatDecode, id()))->post();
        }
        return;
    }

    msg->post(100000);
}


void AudioSfDecoder::onDecode() {
    SL_LOGV("AudioSfDecoder::onDecode");

    //-------------------------------- Need to buffer some more before decoding?
    bool eos;
    if (mDataSource == 0) {
        // application set play state to paused which failed, then set play state to playing
        return;
    }
    if (wantPrefetch()
            && (getCacheRemaining(&eos) == kStatusLow)
            && !eos) {
        SL_LOGV("buffering more.");

        if (mStateFlags & kFlagPlaying) {
            pauseAudioSink();
        }
        mStateFlags |= kFlagBuffering;
        (new AMessage(kWhatCheckCache, id()))->post(100000);
        return;
    }

    if (!(mStateFlags & (kFlagPlaying | kFlagBuffering | kFlagPreparing))) {
        // don't decode if we're not buffering, prefetching or playing
        //SL_LOGV("don't decode: not buffering, prefetching or playing");
        return;
    }

    //-------------------------------- Decode
    status_t err;
    MediaSource::ReadOptions readOptions;
    if (mStateFlags & kFlagSeeking) {
        readOptions.setSeekTo(mSeekTimeMsec * 1000);
    }

    {
        Mutex::Autolock _l(mDecodeBufferLock);
        if (NULL != mDecodeBuffer) {
            // the current decoded buffer hasn't been rendered, drop it
            mDecodeBuffer->release();
            mDecodeBuffer = NULL;
        }
        err = mAudioSource->read(&mDecodeBuffer, &readOptions);
        if (err == OK) {
            CHECK(mDecodeBuffer->meta_data()->findInt64(kKeyTime, &mLastDecodedPositionUs));
        }
    }

    {
        Mutex::Autolock _l(mSeekLock);
        if (mStateFlags & kFlagSeeking) {
            mStateFlags &= ~kFlagSeeking;
        }
    }

    //-------------------------------- Handle return of decode
    if (err != OK) {
        bool continueDecoding = false;
        switch(err) {
            case ERROR_END_OF_STREAM:
                if (0 < mDurationUsec) {
                    mLastDecodedPositionUs = mDurationUsec;
                }
                // handle notification and looping at end of stream
                if (mStateFlags & kFlagPlaying) {
                    notify(PLAYEREVENT_ENDOFSTREAM, 1, true);
                }
                if (mStateFlags & kFlagLooping) {
                    seek(0);
                    // kick-off decoding again
                    continueDecoding = true;
                }
                break;
            case INFO_FORMAT_CHANGED:
                SL_LOGD("MediaSource::read encountered INFO_FORMAT_CHANGED");
                // reconfigure output
                updateAudioSink();
                continueDecoding = true;
                break;
            case INFO_DISCONTINUITY:
                SL_LOGD("MediaSource::read encountered INFO_DISCONTINUITY");
                continueDecoding = true;
                break;
            default:
                SL_LOGE("MediaSource::read returned error %d", err);
                break;
        }
        if (continueDecoding) {
            if (NULL == mDecodeBuffer) {
                (new AMessage(kWhatDecode, id()))->post();
                return;
            }
        } else {
            return;
        }
    }

    //-------------------------------- Render
    sp<AMessage> msg = new AMessage(kWhatRender, id());
    msg->post();
}


void AudioSfDecoder::onRender() {
    //SL_LOGV("AudioSfDecoder::onRender");

    Mutex::Autolock _l(mDecodeBufferLock);

    if (NULL == mDecodeBuffer) {
        // nothing to render, move along
        SL_LOGV("AudioSfDecoder::onRender NULL buffer, exiting");
        return;
    }

    mDecodeBuffer->release();
    mDecodeBuffer = NULL;

}


void AudioSfDecoder::onMessageReceived(const sp<AMessage> &msg) {
    switch (msg->what()) {
        case kWhatPrepare:
            onPrepare();
            break;

        case kWhatDecode:
            onDecode();
            break;

        case kWhatRender:
            onRender();
            break;

        case kWhatCheckCache:
            onCheckCache(msg);
            break;

        case kWhatNotif:
            onNotify(msg);
            break;

        case kWhatPlay:
            onPlay();
            break;

        case kWhatPause:
            onPause();
            break;
/*
        case kWhatSeek:
            onSeek(msg);
            break;

        case kWhatLoop:
            onLoop(msg);
            break;
*/
        default:
            GenericPlayer::onMessageReceived(msg);
            break;
    }
}

//--------------------------------------------------
// Prepared state, prefetch status notifications
void AudioSfDecoder::notifyPrepared(status_t prepareRes) {
    notify(PLAYEREVENT_PREPARED, (int32_t)prepareRes, true);

}


void AudioSfDecoder::onNotify(const sp<AMessage> &msg) {
    if (NULL == mNotifyClient) {
        return;
    }
    int32_t val;
    if (msg->findInt32(PLAYEREVENT_PREFETCHSTATUSCHANGE, &val)) {
        SL_LOGV("\tASfPlayer notifying %s = %d", PLAYEREVENT_PREFETCHSTATUSCHANGE, val);
        mNotifyClient(kEventPrefetchStatusChange, val, 0, mNotifyUser);
    }
    else if (msg->findInt32(PLAYEREVENT_PREFETCHFILLLEVELUPDATE, &val)) {
        SL_LOGV("\tASfPlayer notifying %s = %d", PLAYEREVENT_PREFETCHFILLLEVELUPDATE, val);
        mNotifyClient(kEventPrefetchFillLevelUpdate, val, 0, mNotifyUser);
    }
    else if (msg->findInt32(PLAYEREVENT_ENDOFSTREAM, &val)) {
        SL_LOGV("\tASfPlayer notifying %s = %d", PLAYEREVENT_ENDOFSTREAM, val);
        mNotifyClient(kEventEndOfStream, val, 0, mNotifyUser);
    }
    else {
        GenericPlayer::onNotify(msg);
    }
}


//--------------------------------------------------
// Private utility functions

bool AudioSfDecoder::wantPrefetch() {
    return (mDataSource->flags() & DataSource::kWantsPrefetching);
}


int64_t AudioSfDecoder::getPositionUsec() {
    Mutex::Autolock _l(mSeekLock);
    if (mStateFlags & kFlagSeeking) {
        return mSeekTimeMsec * 1000;
    } else {
        if (mLastDecodedPositionUs < 0) {
            return 0;
        } else {
            return mLastDecodedPositionUs;
        }
    }
}


CacheStatus_t AudioSfDecoder::getCacheRemaining(bool *eos) {
    sp<NuCachedSource2> cachedSource =
        static_cast<NuCachedSource2 *>(mDataSource.get());

    CacheStatus_t oldStatus = mCacheStatus;

    status_t finalStatus;
    size_t dataRemaining = cachedSource->approxDataRemaining(&finalStatus);
    *eos = (finalStatus != OK);

    CHECK_GE(mBitrate, 0);

    int64_t dataRemainingUs = dataRemaining * 8000000ll / mBitrate;
    //SL_LOGV("AudioSfDecoder::getCacheRemaining: approx %.2f secs remaining (eos=%d)",
    //       dataRemainingUs / 1E6, *eos);

    if (*eos) {
        // data is buffered up to the end of the stream, it can't get any better than this
        mCacheStatus = kStatusHigh;
        mCacheFill = 1000;

    } else {
        if (mDurationUsec > 0) {
            // known duration:

            //   fill level is ratio of how much has been played + how much is
            //   cached, divided by total duration
            uint32_t currentPositionUsec = getPositionUsec();
            mCacheFill = (int16_t) ((1000.0
                    * (double)(currentPositionUsec + dataRemainingUs) / mDurationUsec));
            //SL_LOGV("cacheFill = %d", mCacheFill);

            //   cache status is evaluated against duration thresholds
            if (dataRemainingUs > DURATION_CACHED_HIGH_MS*1000) {
                mCacheStatus = kStatusHigh;
                //LOGV("high");
            } else if (dataRemainingUs > DURATION_CACHED_MED_MS*1000) {
                //LOGV("enough");
                mCacheStatus = kStatusEnough;
            } else if (dataRemainingUs < DURATION_CACHED_LOW_MS*1000) {
                //LOGV("low");
                mCacheStatus = kStatusLow;
            } else {
                mCacheStatus = kStatusIntermediate;
            }

        } else {
            // unknown duration:

            //   cache status is evaluated against cache amount thresholds
            //   (no duration so we don't have the bitrate either, could be derived from format?)
            if (dataRemaining > SIZE_CACHED_HIGH_BYTES) {
                mCacheStatus = kStatusHigh;
            } else if (dataRemaining > SIZE_CACHED_MED_BYTES) {
                mCacheStatus = kStatusEnough;
            } else if (dataRemaining < SIZE_CACHED_LOW_BYTES) {
                mCacheStatus = kStatusLow;
            } else {
                mCacheStatus = kStatusIntermediate;
            }
        }

    }

    if (oldStatus != mCacheStatus) {
        notifyStatus();
    }

    if (abs(mCacheFill - mLastNotifiedCacheFill) > mCacheFillNotifThreshold) {
        notifyCacheFill();
    }

    return mCacheStatus;
}

} // namespace android
