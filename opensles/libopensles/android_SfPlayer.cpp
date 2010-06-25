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
      mNotifyClient(NULL),
      mNotifyUser(NULL) {
}


SfPlayer::~SfPlayer() {
    fprintf(stderr, "\t\t~SfPlayer called\n");

    if (mAudioSource != NULL) {
        mAudioSource->stop();
    }

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


void SfPlayer::prepare_async(const char *uri) {
    //fprintf(stderr, "SfPlayer::prepare_async(%s)\n", uri);
    sp<AMessage> msg = new AMessage(kWhatPrepare, id());
    msg->setString("uri", uri);
    msg->post();
}

int SfPlayer::prepare_sync(const char *uri) {
    //fprintf(stderr, "SfPlayer::prepare_sync(%s)\n", uri);
    sp<AMessage> msg = new AMessage(kWhatPrepare, id());
    msg->setString("uri", uri);
    return onPrepare(msg);
}

int SfPlayer::onPrepare(const sp<AMessage> &msg) {
    //fprintf(stderr, "SfPlayer::onPrepare\n");
    AString uri;
    CHECK(msg->findString("uri", &uri));

    sp<DataSource> dataSource;

    if (!strncasecmp(uri.c_str(), "http://", 7)) {
        sp<NuHTTPDataSource> http = new NuHTTPDataSource;
        if (http->connect(uri.c_str()) == OK) {
            dataSource =
                new NuCachedSource2(
                        new ThrottledSource(
                            http, 5 * 1024 /* bytes/sec */));
        }
    } else {
        dataSource = DataSource::CreateFromURI(uri.c_str());
    }

    if (dataSource == NULL) {
        fprintf(stderr, "Could not create datasource.\n");
        quit();
        return ERROR_UNSUPPORTED;
    }

    sp<MediaExtractor> extractor = MediaExtractor::Create(dataSource);

    if (extractor == NULL) {
        fprintf(stderr, "Could not instantiate extractor.\n");
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
        fprintf(stderr, "Could not find an audio track.\n");
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
            fprintf(stderr, "Could not instantiate decoder.\n");
            quit();
            return ERROR_UNSUPPORTED;
        }

        meta = source->getFormat();
    }

    if (source->start() != OK) {
        fprintf(stderr, "Failed to start source/decoder.\n");
        quit();
        return MEDIA_ERROR_BASE;
    }

    mDataSource = dataSource;
    mAudioSource = source;

    CHECK(meta->findInt32(kKeyChannelCount, &mNumChannels));
    CHECK(meta->findInt32(kKeySampleRate, &mSampleRateHz));

    if (!wantPrefetch()) {
        printf("no need to prefetch\n");
        // doesn't need prefetching, notify good to go
        sp<AMessage> msg = new AMessage(kWhatNotif, id());
        msg->setInt32(EVENT_PREFETCHSTATUSCHANGE, (int32_t)kStatusEnough);
        msg->setInt32(EVENT_PREFETCHFILLLEVELUPDATE, (int32_t)1000);
        notify(msg, true /*async*/);
    }

    return SFPLAYER_SUCCESS;

}


bool SfPlayer::wantPrefetch() {
    return (mDataSource->flags() & DataSource::kWantsPrefetching);
}


void SfPlayer::startPrefetch_async() {
    if (mDataSource->flags() & DataSource::kWantsPrefetching) {
        printf("preparing.\n");

        mFlags |= kFlagPreparing;
        mFlags |= kFlagBuffering;

        (new AMessage(kWhatCheckCache, id()))->post(100000);
    }
}

void SfPlayer::play() {
    // FIXME start based on play state
    fprintf(stderr, "SfPlayer::play\n");
    mFlags |= kFlagPlaying;
    //mAudioTrack->start();
    (new AMessage(kWhatDecode, id()))->post();
}


void SfPlayer::stop() {
    fprintf(stderr, "SfPlayer::stop\n");
    mFlags &= ~kFlagPlaying;
}


void SfPlayer::onDecode() {
    //fprintf(stderr, "SfPlayer::onDecode\n");
    bool eos;
    if ((mDataSource->flags() & DataSource::kWantsPrefetching)
            && (getCacheRemaining(&eos) == kStatusLow)
            && !eos) {
        printf("buffering more.\n");

        //mAudioTrack->pause();
        mFlags |= kFlagBuffering;
        (new AMessage(kWhatCheckCache, id()))->post(100000);
        return;
    }

    MediaBuffer *buffer;
    status_t err = mAudioSource->read(&buffer);

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
    if (!(mFlags & kFlagPlaying)) {
        return;
    }
    //fprintf(stderr, "SfPlayer::onRender\n");

    MediaBuffer *buffer;
    CHECK(msg->findPointer("mbuffer", (void **)&buffer));

    mAudioTrack->write(
            (const uint8_t *)buffer->data() + buffer->range_offset(),
            buffer->range_length());

    buffer->release();
    buffer = NULL;

    (new AMessage(kWhatDecode, id()))->post();
}

void SfPlayer::onCheckCache(const sp<AMessage> &msg) {
    bool eos;
    CacheStatus status = getCacheRemaining(&eos);

    if (eos || status == kStatusHigh
            || ((mFlags & kFlagPreparing) && (status >= kStatusEnough))) {
        // FIXME restart based on play state
        //mAudioTrack->start();
        mFlags &= ~kFlagBuffering;

        printf("buffering done.\n");

        if (mFlags & kFlagPreparing) {
            printf("preparation done.\n");
            mFlags &= ~kFlagPreparing;
        }

        mTimeDelta = -1;
        (new AMessage(kWhatDecode, id()))->post();
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

    printf("approx %.2f secs remaining (eos=%d)\n",
           dataRemainingUs / 1E6, *eos);

    // FIXME evaluate also against the sound duration if the sound duration is < 3s
    if (dataRemainingUs > 30000000) {
        mCacheStatus = kStatusHigh;
    } else if (dataRemainingUs > 10000000) {
        mCacheStatus = kStatusEnough;
    } else if (dataRemainingUs < 2000000) {
        mCacheStatus = kStatusLow;
    } else {
        mCacheStatus = kStatusIntermediate;
    }

    if (oldStatus != mCacheStatus) {
        fprintf(stdout, "Status change to %d\n", mCacheStatus);
        sp<AMessage> msg = new AMessage(kWhatNotif, id());
        msg->setInt32(EVENT_PREFETCHSTATUSCHANGE, mCacheStatus);
        notify(msg, true /*async*/);
        // FIXME update cache level
        fprintf(stderr, "[ FIXME update cache level ]\n");
    }

    return mCacheStatus;
}

void SfPlayer::quit() {
    sp<ALooper> looper = mRenderLooper.promote();
    CHECK(looper != NULL);
    looper->stop();
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

        default:
            TRESPASS();
    }
}

}  // namespace android
