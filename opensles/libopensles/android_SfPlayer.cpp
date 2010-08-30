/*
 * Copyright (C) 2010 The Android Open Source Project
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

#include "android_SfPlayer.h"

#include <stdio.h>

// The usual SL_LOGx macros and Android LOGx are not available
#ifdef LOGE
#undef LOGE
#endif
#ifdef LOGV
#undef LOGV
#endif
#define LOGE(fmt, ...) do { fprintf(stderr, "%s:%s:%d:E ", __FUNCTION__, __FILE__, __LINE__); \
    fprintf(stderr, fmt, ## __VA_ARGS__); fputc('\n', stderr); } while(0)
#define LOGV(fmt, ...) do { fprintf(stdout, "%s:%s:%d:V ", __FUNCTION__, __FILE__, __LINE__); \
    fprintf(stdout, fmt, ## __VA_ARGS__); fputc('\n', stdout); } while(0)

namespace android {

SfPlayer::SfPlayer(const sp<ALooper> &renderLooper)
    : mAudioTrack(NULL),
      mRenderLooper(renderLooper),
      mFlags(0),
      mBitrate(-1),
      mNumChannels(1),
      mSampleRateHz(0),
      mTimeDelta(-1),
      mDurationUsec(-1),
      mCacheStatus(kStatusEmpty),
      mSeekTimeMsec(0),
      mBufferInFlight(false),
      mLastDecodedPositionUs(-1),
      mDataLocatorType(kDataLocatorNone),
      mNotifyClient(NULL),
      mNotifyUser(NULL) {
}


SfPlayer::~SfPlayer() {
    LOGV("SfPlayer::~SfPlayer()");

    if (mAudioSource != NULL) {
        if(mBufferInFlight) {
            LOGE("Attempt to stop the MediaSource with an unreleased buffer.");
        }
        mAudioSource->stop();
    }

    resetDataLocator();
}


void SfPlayer::useAudioTrack(AudioTrack* pTrack) {
    mAudioTrack = pTrack;
}

void SfPlayer::setNotifListener(const notif_client_t cbf, void* notifUser) {
    mNotifyClient = cbf;
    mNotifyUser = notifUser;
}


void SfPlayer::notify(const sp<AMessage> &msg, bool async) {
    if (async) {
        msg->post();
    } else {
        onNotify(msg);
    }
}


void SfPlayer::setDataSource(const char *uri) {
    resetDataLocator();

    size_t len = strlen((const char *) uri);
    char* newUri = (char*) malloc(len + 1);
    if (NULL == newUri) {
        // mem issue
        LOGE("SfPlayer::setDataSource: ERROR not enough memory to allocator URI string");
        return;
    }
    memcpy(newUri, uri, len + 1);
    mDataLocator.uri = newUri;

    mDataLocatorType = kDataLocatorUri;
}

void SfPlayer::setDataSource(const int fd, const int64_t offset, const int64_t length) {
    resetDataLocator();

    mDataLocator.fdi.fd = fd;

    struct stat sb;
    int ret = fstat(fd, &sb);
    if (ret != 0) {
        // sockets are not supported
        LOGE("SfPlayer::setDataSource: ERROR fstat(%d) failed: %d, %s", fd, ret, strerror(errno));
        return;
    }

    if (offset >= sb.st_size) {
        LOGE("SfPlayer::setDataSource: ERROR invalid offset");
        return;
    }
    mDataLocator.fdi.offset = offset;

    if (SFPLAYER_FD_FIND_FILE_SIZE == length) {
        mDataLocator.fdi.length = sb.st_size;
    } else if (offset + length > sb.st_size) {
        mDataLocator.fdi.length = sb.st_size - offset;
    } else {
        mDataLocator.fdi.length = length;
    }

    mDataLocatorType = kDataLocatorFd;
}

void SfPlayer::prepare_async() {
    //LOGV("SfPlayer::prepare_async()");
    sp<AMessage> msg = new AMessage(kWhatPrepare, id());
    msg->post();
}

int SfPlayer::prepare_sync() {
    //LOGV("SfPlayer::prepare_sync()");
    sp<AMessage> msg = new AMessage(kWhatPrepare, id());
    return onPrepare(msg);
}

int SfPlayer::onPrepare(const sp<AMessage> &msg) {
    //LOGV("SfPlayer::onPrepare");
    sp<DataSource> dataSource;

    switch (mDataLocatorType) {

        case kDataLocatorNone:
            LOGE("SfPlayer::onPrepare: ERROR no data locator set");
            return MEDIA_ERROR_BASE;
            break;

        case kDataLocatorUri:
            if (!strncasecmp(mDataLocator.uri, "http://", 7)) {
                sp<NuHTTPDataSource> http = new NuHTTPDataSource;
                if (http->connect(mDataLocator.uri) == OK) {
                    dataSource =
                        new NuCachedSource2(
                                new ThrottledSource(
                                        http, 5 * 1024 /* bytes/sec */));
                }
            } else {
                dataSource = DataSource::CreateFromURI(mDataLocator.uri);
            }
            break;

        case kDataLocatorFd: {
            dataSource = new FileSource(
                    mDataLocator.fdi.fd, mDataLocator.fdi.offset, mDataLocator.fdi.length);
            status_t err = dataSource->initCheck();
            if (err != OK) {
                return err;
            }
            }
            break;

        default:
            TRESPASS();
    }

    if (dataSource == NULL) {
        LOGE("SfPlayer::onPrepare: ERROR: Could not create datasource.");
        return ERROR_UNSUPPORTED;
    }

    sp<MediaExtractor> extractor = MediaExtractor::Create(dataSource);
    if (extractor == NULL) {
        LOGE("SfPlayer::onPrepare: ERROR: Could not instantiate extractor.");
        return ERROR_UNSUPPORTED;
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
        LOGE("SfPlayer::onPrepare: ERROR: Could not find an audio track.");
        return ERROR_UNSUPPORTED;
    }

    sp<MediaSource> source = extractor->getTrack(audioTrackIndex);
    sp<MetaData> meta = source->getFormat();

    off_t size;
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
            LOGE("SfPlayer::onPrepare: ERROR: Could not instantiate decoder.");
            return ERROR_UNSUPPORTED;
        }

        meta = source->getFormat();
    }

    if (source->start() != OK) {
        LOGE("SfPlayer::onPrepare: ERROR: Failed to start source/decoder.");
        return MEDIA_ERROR_BASE;
    }

    mDataSource = dataSource;
    mAudioSource = source;

    CHECK(meta->findInt32(kKeyChannelCount, &mNumChannels));
    CHECK(meta->findInt32(kKeySampleRate, &mSampleRateHz));

    if (!wantPrefetch()) {
        LOGV("SfPlayer::onPrepare: no need to prefetch");
        // doesn't need prefetching, notify good to go
        sp<AMessage> msg = new AMessage(kWhatNotif, id());
        msg->setInt32(EVENT_PREFETCHSTATUSCHANGE, (int32_t)kStatusEnough);
        msg->setInt32(EVENT_PREFETCHFILLLEVELUPDATE, (int32_t)1000);
        notify(msg, true /*async*/);
    }

    //LOGV("SfPlayer::onPrepare: end");
    return SFPLAYER_SUCCESS;

}


bool SfPlayer::wantPrefetch() {
    return (mDataSource->flags() & DataSource::kWantsPrefetching);
}


void SfPlayer::startPrefetch_async() {
    //LOGV("SfPlayer::startPrefetch_async()");
    if (wantPrefetch()) {
        //LOGV("SfPlayer::startPrefetch_async(): sending check cache msg");

        mFlags |= kFlagPreparing;
        mFlags |= kFlagBuffering;

        (new AMessage(kWhatCheckCache, id()))->post(100000);
    }
}


void SfPlayer::play() {
    LOGV("SfPlayer::play");
    if (NULL == mAudioTrack) {
        return;
    }

    mAudioTrack->start();

    (new AMessage(kWhatPlay, id()))->post();
    (new AMessage(kWhatDecode, id()))->post();
}


void SfPlayer::stop() {
    LOGV("SfPlayer::stop");

    if (NULL == mAudioTrack) {
        return;
    }
    mAudioTrack->stop();

    (new AMessage(kWhatPause, id()))->post();

    // after a stop, playback should resume from the start.
    seek(0);
}

void SfPlayer::pause() {
    LOGV("SfPlayer::pause");
    if (NULL == mAudioTrack) {
        return;
    }
    (new AMessage(kWhatPause, id()))->post();
    mAudioTrack->pause();
}

void SfPlayer::seek(int64_t timeMsec) {
    LOGV("SfPlayer::seek %lld", timeMsec);
    sp<AMessage> msg = new AMessage(kWhatSeek, id());
    msg->setInt64("seek", timeMsec);
    msg->post();
}


uint32_t SfPlayer::getPositionMsec() {
    Mutex::Autolock _l(mSeekLock);
    if (mFlags & kFlagSeeking) {
        return (uint32_t) mSeekTimeMsec;
    } else {
        if (mLastDecodedPositionUs < 0) {
            return 0;
        } else {
            return (uint32_t) (mLastDecodedPositionUs / 1000);
        }
    }
}

/*
 * Message handlers
 */

void SfPlayer::onPlay() {
    LOGV("SfPlayer::onPlay");
    mFlags |= kFlagPlaying;
}

void SfPlayer::onPause() {
    LOGV("SfPlayer::onPause");
    mFlags &= ~kFlagPlaying;
}

void SfPlayer::onSeek(const sp<AMessage> &msg) {
    LOGV("SfPlayer::onSeek");
    int64_t timeMsec;
    CHECK(msg->findInt64("seek", &timeMsec));

    Mutex::Autolock _l(mSeekLock);
    mFlags |= kFlagSeeking;
    mSeekTimeMsec = timeMsec;
    mTimeDelta = -1;
    mLastDecodedPositionUs = -1;
}


void SfPlayer::onDecode() {
    //LOGV("SfPlayer::onDecode");
    bool eos;
    if ((mDataSource->flags() & DataSource::kWantsPrefetching)
            && (getCacheRemaining(&eos) == kStatusLow)
            && !eos) {
        LOGV("buffering more.");

        if (mFlags & kFlagPlaying) {
            mAudioTrack->pause();
        }
        mFlags |= kFlagBuffering;
        (new AMessage(kWhatCheckCache, id()))->post(100000);
        return;
    }

    if (!(mFlags & (kFlagPlaying | kFlagBuffering | kFlagPreparing))) {
        // don't decode if we're not buffering, prefetching or playing
        //LOGV("don't decode: not buffering, prefetching or playing");
        return;
    }

    if (mBufferInFlight) {
        // the current decoded buffer hasn't been rendered, drop this decode until it is
        //LOGV("don't decode: buffer in flight");
        return;
    }

    MediaBuffer *buffer;
    MediaSource::ReadOptions readOptions;
    if (mFlags & kFlagSeeking) {
        readOptions.setSeekTo(mSeekTimeMsec * 1000);
    }

    status_t err = mAudioSource->read(&buffer, &readOptions);

    if (err != OK) {
        if (err != ERROR_END_OF_STREAM) {
            LOGE("MediaSource::read returned error %d", err);
            // FIXME handle error
        } else {
            //LOGV("SfPlayer::onDecode hit ERROR_END_OF_STREAM");
            // async notification of end of stream reached
            sp<AMessage> msg = new AMessage(kWhatNotif, id());
            msg->setInt32(EVENT_ENDOFSTREAM, 1);
            notify(msg, true /*async*/);
        }
        return;
    }

    {
        Mutex::Autolock _l(mSeekLock);
        CHECK(buffer->meta_data()->findInt64(kKeyTime, &mLastDecodedPositionUs));
        if (mFlags & kFlagSeeking) {
            mFlags &= ~kFlagSeeking;
        }
    }

    sp<AMessage> msg = new AMessage(kWhatRender, id());
    msg->setPointer("mbuffer", buffer);

    if (mTimeDelta < 0) {
        mTimeDelta = ALooper::GetNowUs() - mLastDecodedPositionUs;
    }

    int64_t delayUs = mLastDecodedPositionUs + mTimeDelta - ALooper::GetNowUs();

    mBufferInFlight = true;
    msg->post(delayUs); // negative delays are ignored
    //LOGV("timeUs=%lld, mTimeDelta=%lld, delayUs=%lld",
    //mLastDecodedPositionUs, mTimeDelta, delayUs);
}

void SfPlayer::onRender(const sp<AMessage> &msg) {
    //LOGV("SfPlayer::onRender");

    MediaBuffer *buffer;
    CHECK(msg->findPointer("mbuffer", (void **)&buffer));

    if (mFlags & kFlagPlaying) {
        mAudioTrack->write( (const uint8_t *)buffer->data() + buffer->range_offset(),
                buffer->range_length());
        (new AMessage(kWhatDecode, id()))->post();
    }
    buffer->release();
    mBufferInFlight = false;
    buffer = NULL;
}

void SfPlayer::onCheckCache(const sp<AMessage> &msg) {
    //LOGV("SfPlayer::onCheckCache");
    bool eos;
    CacheStatus status = getCacheRemaining(&eos);

    if (eos || status == kStatusHigh
            || ((mFlags & kFlagPreparing) && (status >= kStatusEnough))) {
        if (mFlags & kFlagPlaying) {
            mAudioTrack->start();
        }
        mFlags &= ~kFlagBuffering;

        LOGV("SfPlayer::onCheckCache: buffering done.");

        if (mFlags & kFlagPreparing) {
            //LOGV("SfPlayer::onCheckCache: preparation done.");
            mFlags &= ~kFlagPreparing;
        }

        mTimeDelta = -1;
        if (mFlags & kFlagPlaying) {
            (new AMessage(kWhatDecode, id()))->post();
        }
        return;
    }

    msg->post(100000);
}

void SfPlayer::onNotify(const sp<AMessage> &msg) {
    if (NULL == mNotifyClient) {
        return;
    }
    int32_t cacheInfo;
    if (msg->findInt32(EVENT_PREFETCHSTATUSCHANGE, &cacheInfo)) {
        LOGV("\tSfPlayer notifying %s = %d", EVENT_PREFETCHSTATUSCHANGE, cacheInfo);
        mNotifyClient(kEventPrefetchStatusChange, cacheInfo, mNotifyUser);
    }
    if (msg->findInt32(EVENT_PREFETCHFILLLEVELUPDATE, &cacheInfo)) {
        LOGV("\tSfPlayer notifying %s = %d", EVENT_PREFETCHFILLLEVELUPDATE, cacheInfo);
        mNotifyClient(kEventPrefetchFillLevelUpdate, cacheInfo, mNotifyUser);
    }
    if (msg->findInt32(EVENT_ENDOFSTREAM, &cacheInfo)) {
        LOGV("\tSfPlayer notifying %s = %d", EVENT_ENDOFSTREAM, cacheInfo);
        mNotifyClient(kEventEndOfStream, cacheInfo, mNotifyUser);
    }
}

SfPlayer::CacheStatus SfPlayer::getCacheRemaining(bool *eos) {
    sp<NuCachedSource2> cachedSource =
        static_cast<NuCachedSource2 *>(mDataSource.get());

    CacheStatus oldStatus = mCacheStatus;

    size_t dataRemaining = cachedSource->approxDataRemaining(eos);

    CHECK_GE(mBitrate, 0);

    int64_t dataRemainingUs = dataRemaining * 8000000ll / mBitrate;

    LOGV("SfPlayer::getCacheRemaining: approx %.2f secs remaining (eos=%d)",
           dataRemainingUs / 1E6, *eos);

    // FIXME improve
    if (dataRemainingUs >= mDurationUsec*0.95) {
        mCacheStatus = kStatusEnough;
        LOGV("SfPlayer::getCacheRemaining: cached most of the content");
    } else {
        // FIXME evaluate also against the sound duration
        if (dataRemainingUs > 30000000) {
            mCacheStatus = kStatusHigh;
        } else if (dataRemainingUs > 10000000) {
            mCacheStatus = kStatusEnough;
        } else if (dataRemainingUs < 2000000) {
            mCacheStatus = kStatusLow;
        } else {
            mCacheStatus = kStatusIntermediate;
        }
    }

    if (oldStatus != mCacheStatus) {
        //LOGV("SfPlayer::getCacheRemaining: Status change to %d", mCacheStatus);
        sp<AMessage> msg = new AMessage(kWhatNotif, id());
        msg->setInt32(EVENT_PREFETCHSTATUSCHANGE, mCacheStatus);
        notify(msg, true /*async*/);
        // FIXME update cache level
        LOGE("[ FIXME update cache level in SfPlayer::getCacheRemaining() ]");
    }

    return mCacheStatus;
}


/*
 * post-condition: mDataLocatorType == kDataLocatorNone
 *
 */
void SfPlayer::resetDataLocator() {
    if (kDataLocatorUri == mDataLocatorType) {
        if (NULL != mDataLocator.uri) {
            free(mDataLocator.uri);
            mDataLocator.uri = NULL;
        }
    }
    mDataLocatorType = kDataLocatorNone;
}


void SfPlayer::onMessageReceived(const sp<AMessage> &msg) {
    switch (msg->what()) {
        case kWhatPrepare:
            onPrepare(msg);
            break;

        case kWhatDecode:
            onDecode();
            break;

        case kWhatRender:
            onRender(msg);
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

        case kWhatSeek:
            onSeek(msg);
            break;

        default:
            TRESPASS();
    }
}

}  // namespace android
