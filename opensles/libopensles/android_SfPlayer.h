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
#include <binder/ProcessState.h>

#include <media/AudioTrack.h>
#include <media/stagefright/foundation/ADebug.h>
#include <media/stagefright/foundation/AHandler.h>
#include <media/stagefright/foundation/ALooper.h>
#include <media/stagefright/foundation/AMessage.h>
#include <media/stagefright/DataSource.h>
#include <media/stagefright/MediaDefs.h>
#include <media/stagefright/MediaExtractor.h>
#include <media/stagefright/MetaData.h>
#include <media/stagefright/OMXClient.h>
#include <media/stagefright/OMXCodec.h>

#include "NuCachedSource2.h"
#include "NuHTTPDataSource.h"
#include "ThrottledSource.h"

#define EVENT_PREFETCHSTATUSCHANGE    "prsc"
#define EVENT_PREFETCHFILLLEVELUPDATE "pflu"
#define EVENT_ENDOFSTREAM             "eos"

namespace android {

    typedef void (*notif_client_t)(int event, const int data1, void* notifUser);

struct SfPlayer : public AHandler {
    SfPlayer(const sp<ALooper> &renderLooper);

    enum CacheStatus {
        kStatusEmpty,
        kStatusLow,
        kStatusIntermediate,
        kStatusEnough,
        kStatusHigh
    };

    enum {
        kEventPrefetchStatusChange    = 'prsc',
        kEventPrefetchFillLevelUpdate = 'pflu',
        kEventEndOfStream             = 'eos',
    };

    /**
     * temporary accessor until AudioTrack is not owned by SfPlayer
     */
    AudioTrack* audioTrack();

    void setNotifListener(const notif_client_t cbf, void* notifUser);

    void prepare_async(const char *uri);
    void prepare_sync(const char *uri);
    void play();
    bool wantPrefetch();
    void startPrefetch_async();

    /**
     * returns the duration in microseconds, -1 if unknown
     */
    int64_t getDurationUsec() { return mDurationUsec; }

protected:
    virtual ~SfPlayer();
    virtual void onMessageReceived(const sp<AMessage> &msg);

private:

    enum {
        kWhatPrepare    = 'prep',
        kWhatDecode     = 'deco',
        kWhatRender     = 'rend',
        kWhatCheckCache = 'cach',
        kWhatNotif      = 'notif',
    };

    enum {
        kFlagPlaying   = 1,
        kFlagPreparing = 2,
        kFlagBuffering = 4,
    };

    wp<ALooper> mRenderLooper;
    sp<DataSource> mDataSource;
    sp<MediaSource> mAudioSource;
    AudioTrack *mAudioTrack;
    uint32_t mFlags;
    int64_t mBitrate;  // in bits/sec
    int64_t mTimeDelta;
    int64_t mDurationUsec;
    CacheStatus mCacheStatus;

    notif_client_t mNotifyClient;
    void*          mNotifyUser;

    void onPrepare(const sp<AMessage> &msg);
    void onDecode();
    void onRender(const sp<AMessage> &msg);
    void onCheckCache(const sp<AMessage> &msg);
    void onNotify(const sp<AMessage> &msg);

    CacheStatus getCacheRemaining(bool *eos);
    void notify(const sp<AMessage> &msg, bool async);

    void quit();

    DISALLOW_EVIL_CONSTRUCTORS(SfPlayer);
};

} // namespace android
