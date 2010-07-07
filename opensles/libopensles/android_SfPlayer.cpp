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
      mIsSeeking(false),
      mSeekTimeMsec(0),
      mDataLocatorType(kDataLocatorNone),
      mNotifyClient(NULL),
      mNotifyUser(NULL) {
}


SfPlayer::~SfPlayer() {
    fprintf(stderr, "SfPlayer::~SfPlayer()\n");

    if (mAudioSource != NULL) {
        mAudioSource->stop();
    }

    resetDataLocator();

    // FIXME need to call quit()?
    quit();
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
        fprintf(stderr, "SfPlayer::setDataSource: ERROR not enough memory to allocator URI string\n");
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
        fprintf(stderr, "SfPlayer::setDataSource: ERROR fstat(%d) failed: %d, %s\n", fd, ret, strerror(errno));
        return;
    }

    if (offset >= sb.st_size) {
        fprintf(stderr, "SfPlayer::setDataSource: ERROR invalid offset\n");
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
    //fprintf(stderr, "SfPlayer::prepare_async()\n");
    sp<AMessage> msg = new AMessage(kWhatPrepare, id());
    msg->post();
}

int SfPlayer::prepare_sync() {
    //fprintf(stderr, "SfPlayer::prepare_sync()\n");
    sp<AMessage> msg = new AMessage(kWhatPrepare, id());
    return onPrepare(msg);
}

int SfPlayer::onPrepare(const sp<AMessage> &msg) {
    //fprintf(stderr, "SfPlayer::onPrepare\n");
    sp<DataSource> dataSource;

    switch (mDataLocatorType) {

        case kDataLocatorNone:
            fprintf(stderr, "SfPlayer::onPrepare: ERROR no data locator set\n");
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
        fprintf(stderr, "SfPlayer::onPrepare: ERROR: Could not create datasource.\n");
        quit();
        return ERROR_UNSUPPORTED;
    }

    sp<MediaExtractor> extractor = MediaExtractor::Create(dataSource);
    if (extractor == NULL) {
        fprintf(stderr, "SfPlayer::onPrepare: ERROR: Could not instantiate extractor.\n");
        quit();
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
        fprintf(stderr, "SfPlayer::onPrepare: ERROR: Could not find an audio track.\n");
        quit();
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
            fprintf(stderr, "SfPlayer::onPrepare: ERROR: Could not instantiate decoder.\n");
            quit();
            return ERROR_UNSUPPORTED;
        }

        meta = source->getFormat();
    }

    if (source->start() != OK) {
        fprintf(stderr, "SfPlayer::onPrepare: ERROR: Failed to start source/decoder.\n");
        quit();
        return MEDIA_ERROR_BASE;
    }

    mDataSource = dataSource;
    mAudioSource = source;

    CHECK(meta->findInt32(kKeyChannelCount, &mNumChannels));
    CHECK(meta->findInt32(kKeySampleRate, &mSampleRateHz));

    if (!wantPrefetch()) {
        printf("SfPlayer::onPrepare: no need to prefetch\n");
        // doesn't need prefetching, notify good to go
        sp<AMessage> msg = new AMessage(kWhatNotif, id());
        msg->setInt32(EVENT_PREFETCHSTATUSCHANGE, (int32_t)kStatusEnough);
        msg->setInt32(EVENT_PREFETCHFILLLEVELUPDATE, (int32_t)1000);
        notify(msg, true /*async*/);
    }

    //fprintf(stderr, "SfPlayer::onPrepare: end\n");
    return SFPLAYER_SUCCESS;

}


bool SfPlayer::wantPrefetch() {
    return (mDataSource->flags() & DataSource::kWantsPrefetching);
}


void SfPlayer::startPrefetch_async() {
    //printf("SfPlayer::startPrefetch_async()\n");
    if (mDataSource->flags() & DataSource::kWantsPrefetching) {
        //printf("SfPlayer::startPrefetch_async(): sending check cache msg\n");

        mFlags |= kFlagPreparing;
        mFlags |= kFlagBuffering;

        (new AMessage(kWhatCheckCache, id()))->post(100000);
    }
}

void SfPlayer::play() {
    fprintf(stderr, "SfPlayer::play\n");
    mAudioTrack->start();
    (new AMessage(kWhatPlay, id()))->post();
    (new AMessage(kWhatDecode, id()))->post();
}


void SfPlayer::stop() {
    fprintf(stderr, "SfPlayer::stop\n");
    (new AMessage(kWhatPause, id()))->post();
    mAudioTrack->stop();
}

void SfPlayer::pause() {
    fprintf(stderr, "SfPlayer::pause\n");
    (new AMessage(kWhatPause, id()))->post();
    mAudioTrack->pause();
}

void SfPlayer::seek(int64_t timeMsec) {
    fprintf(stderr, "SfPlayer::seek %d\n", timeMsec);
    sp<AMessage> msg = new AMessage(kWhatSeek, id());
    msg->setInt64("seek", timeMsec);
    msg->post();
}


void SfPlayer::onPlay() {
    mFlags |= kFlagPlaying;
}

void SfPlayer::onPause() {
    mFlags &= ~kFlagPlaying;
}

void SfPlayer::onSeek(const sp<AMessage> &msg) {
    fprintf(stderr, "SfPlayer::onSeek\n");
    int64_t timeMsec;
    CHECK(msg->findInt64("seek", &timeMsec));
    mIsSeeking = true;
    mSeekTimeMsec = timeMsec;
}


void SfPlayer::onDecode() {
    //fprintf(stderr, "SfPlayer::onDecode\n");
    bool eos;
    if ((mDataSource->flags() & DataSource::kWantsPrefetching)
            && (getCacheRemaining(&eos) == kStatusLow)
            && !eos) {
        printf("buffering more.\n");

        if (mFlags & kFlagPlaying) {
            mAudioTrack->pause();
        }
        mFlags |= kFlagBuffering;
        (new AMessage(kWhatCheckCache, id()))->post(100000);
        return;
    }

    if (!(mFlags & (kFlagPlaying | kFlagBuffering | kFlagPreparing))) {
        // don't decode if we're not buffering, prefetching or playing
        return;
    }

    MediaBuffer *buffer;
    MediaSource::ReadOptions readOptions;
    if (mIsSeeking) {
        readOptions.setSeekTo(mSeekTimeMsec * 1000);
        mIsSeeking = false;
    }
    status_t err = mAudioSource->read(&buffer, &readOptions);

    if (err != OK) {
        if (err != ERROR_END_OF_STREAM) {
            fprintf(stderr, "MediaSource::read returned error %d", err);
            // FIXME handle error
        } else {
            //fprintf(stderr, "SfPlayer::onDecode hit ERROR_END_OF_STREAM\n");
            // async notification of end of stream reached
            sp<AMessage> msg = new AMessage(kWhatNotif, id());
            msg->setInt32(EVENT_ENDOFSTREAM, 1);
            notify(msg, true /*async*/);
        }
        //quit();
        return;
    }

    int64_t timeUs;
    CHECK(buffer->meta_data()->findInt64(kKeyTime, &timeUs));

    sp<AMessage> msg = new AMessage(kWhatRender, id());
    msg->setPointer("mbuffer", buffer);

    if (mTimeDelta < 0) {
        mTimeDelta = ALooper::GetNowUs() - timeUs;
    }

    int64_t delayUs = timeUs + mTimeDelta - ALooper::GetNowUs();
    msg->post(delayUs);
}

void SfPlayer::onRender(const sp<AMessage> &msg) {
    //fprintf(stderr, "SfPlayer::onRender\n");

    MediaBuffer *buffer;
    CHECK(msg->findPointer("mbuffer", (void **)&buffer));

    if (mFlags & kFlagPlaying) {
        mAudioTrack->write(
                (const uint8_t *)buffer->data() + buffer->range_offset(),
                buffer->range_length());
        (new AMessage(kWhatDecode, id()))->post();
    }
    buffer->release();
    buffer = NULL;


}

void SfPlayer::onCheckCache(const sp<AMessage> &msg) {
    //fprintf(stderr, "SfPlayer::onCheckCache\n");
    bool eos;
    CacheStatus status = getCacheRemaining(&eos);

    if (eos || status == kStatusHigh
            || ((mFlags & kFlagPreparing) && (status >= kStatusEnough))) {
        if (mFlags & kFlagPlaying) {
            mAudioTrack->start();
        }
        mFlags &= ~kFlagBuffering;

        printf("SfPlayer::onCheckCache: buffering done.\n");

        if (mFlags & kFlagPreparing) {
            //printf("SfPlayer::onCheckCache: preparation done.\n");
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
        fprintf(stdout, "\tSfPlayer notifying %s = %d\n", EVENT_PREFETCHSTATUSCHANGE, cacheInfo);
        mNotifyClient(kEventPrefetchStatusChange, cacheInfo, mNotifyUser);
    }
    if (msg->findInt32(EVENT_PREFETCHFILLLEVELUPDATE, &cacheInfo)) {
        fprintf(stdout, "\tSfPlayer notifying %s = %d\n", EVENT_PREFETCHFILLLEVELUPDATE, cacheInfo);
        mNotifyClient(kEventPrefetchFillLevelUpdate, cacheInfo, mNotifyUser);
    }
    if (msg->findInt32(EVENT_ENDOFSTREAM, &cacheInfo)) {
        fprintf(stdout, "\tSfPlayer notifying %s = %d\n", EVENT_ENDOFSTREAM, cacheInfo);
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

    printf("SfPlayer::getCacheRemaining: approx %.2f secs remaining (eos=%d)\n",
           dataRemainingUs / 1E6, *eos);

    // FIXME improve
    if (dataRemainingUs >= mDurationUsec*0.95) {
        mCacheStatus = kStatusEnough;
        //printf("SfPlayer::getCacheRemaining: cached most of the content\n");
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
        //fprintf(stdout, "SfPlayer::getCacheRemaining: Status change to %d\n", mCacheStatus);
        sp<AMessage> msg = new AMessage(kWhatNotif, id());
        msg->setInt32(EVENT_PREFETCHSTATUSCHANGE, mCacheStatus);
        notify(msg, true /*async*/);
        // FIXME update cache level
        fprintf(stderr, "[ FIXME update cache level in SfPlayer::getCacheRemaining() ]\n");
    }

    return mCacheStatus;
}

void SfPlayer::quit() {
    sp<ALooper> looper = mRenderLooper.promote();
    CHECK(looper != NULL);
    looper->stop();
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
