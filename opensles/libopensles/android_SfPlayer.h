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
#include <sys/stat.h>
#include <media/AudioTrack.h>
#include <media/stagefright/foundation/ADebug.h>
#include <media/stagefright/foundation/AHandler.h>
#include <media/stagefright/foundation/ALooper.h>
#include <media/stagefright/foundation/AMessage.h>
#include <media/stagefright/DataSource.h>
#include <media/stagefright/FileSource.h>
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

#define SFPLAYER_SUCCESS 1
#define SFPLAYER_FD_FIND_FILE_SIZE ((int64_t)0xFFFFFFFFFFFFFFFFll)

namespace android {

    typedef void (*notif_client_t)(int event, const int data1, void* notifUser);

struct SfPlayer : public AHandler {
    SfPlayer(const sp<ALooper> &renderLooper);

    enum CacheStatus {
        kStatusEmpty = 0,
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

    void useAudioTrack(AudioTrack* pTrack);
    void setNotifListener(const notif_client_t cbf, void* notifUser);

    void setDataSource(const char *uri);
    void setDataSource(const int fd, const int64_t offset, const int64_t length);

    void prepare_async();
    int  prepare_sync();
    void play();
    void pause();
    void stop();
    bool wantPrefetch();
    void startPrefetch_async();

    /**
     * returns the duration in microseconds, -1 if unknown
     */
    int64_t getDurationUsec() { return mDurationUsec; }
    int32_t getNumChannels()  { return mNumChannels; }
    int32_t getSampleRateHz() { return mSampleRateHz; }

protected:
    virtual ~SfPlayer();
    virtual void onMessageReceived(const sp<AMessage> &msg);

private:

    enum {
        kDataLocatorNone = 'none',
        kDataLocatorUri  = 'uri',
        kDataLocatorFd   = 'fd',
    };

    enum {
        kWhatPrepare    = 'prep',
        kWhatDecode     = 'deco',
        kWhatRender     = 'rend',
        kWhatCheckCache = 'cach',
        kWhatNotif      = 'noti',
        kWhatPlay       = 'play',
        kWhatPause      = 'paus',
    };

    enum {
        kFlagPlaying   = 1,
        kFlagPreparing = 2,
        kFlagBuffering = 4,
    };

    struct FdInfo {
        int fd;
        int64_t offset;
        int64_t length;
    };

    union DataLocator {
        char* uri;
        FdInfo fdi;
    };

    AudioTrack *mAudioTrack;

    wp<ALooper> mRenderLooper;
    sp<DataSource> mDataSource;
    sp<MediaSource> mAudioSource;
    uint32_t mFlags;
    int64_t mBitrate;  // in bits/sec
    int32_t mNumChannels;
    int32_t mSampleRateHz;
    int64_t mTimeDelta;
    int64_t mDurationUsec;
    CacheStatus mCacheStatus;

    DataLocator mDataLocator;
    int         mDataLocatorType;

    notif_client_t mNotifyClient;
    void*          mNotifyUser;

    int onPrepare(const sp<AMessage> &msg);
    void onDecode();
    void onRender(const sp<AMessage> &msg);
    void onCheckCache(const sp<AMessage> &msg);
    void onNotify(const sp<AMessage> &msg);
    void onPlay();
    void onPause();

    CacheStatus getCacheRemaining(bool *eos);
    void notify(const sp<AMessage> &msg, bool async);

    void quit();
    void resetDataLocator();

    DISALLOW_EVIL_CONSTRUCTORS(SfPlayer);
};

} // namespace android
