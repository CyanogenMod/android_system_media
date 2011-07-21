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

#ifndef __ANDROID_GENERICMEDIAPLAYER_H__
#define __ANDROID_GENERICMEDIAPLAYER_H__

#include "android_GenericPlayer.h"

#include <binder/IServiceManager.h>
#include <surfaceflinger/Surface.h>
#include <gui/ISurfaceTexture.h>


//--------------------------------------------------------------------------------------------------
namespace android {

class GenericMediaPlayer;
class MediaPlayerNotificationClient : public BnMediaPlayerClient
{
public:
    MediaPlayerNotificationClient(GenericMediaPlayer* gmp);
    virtual ~MediaPlayerNotificationClient();

    // IMediaPlayerClient implementation
    virtual void notify(int msg, int ext1, int ext2, const Parcel *obj);

    // Call before enqueuing a prepare event
    void beforePrepare();

    // Call after enqueueing the prepare event; returns true if the prepare
    // completed successfully, or false if it completed unsuccessfully
    bool blockUntilPlayerPrepared();

private:
    Mutex mLock;
    GenericMediaPlayer* mGenericMediaPlayer;
    Condition mPlayerPreparedCondition;
    enum {
        PREPARE_NOT_STARTED,
        PREPARE_IN_PROGRESS,
        PREPARE_COMPLETED_SUCCESSFULLY,
        PREPARE_COMPLETED_UNSUCCESSFULLY
    } mPlayerPrepared;
};


//--------------------------------------------------------------------------------------------------
class GenericMediaPlayer : public GenericPlayer
{
public:

    GenericMediaPlayer(const AudioPlayback_Parameters* params, bool hasVideo);
    virtual ~GenericMediaPlayer();

    virtual void preDestroy();

    // overridden from GenericPlayer
    virtual void getPositionMsec(int* msec); // ANDROID_UNKNOWN_TIME if unknown

    virtual void setVideoSurface(const sp<Surface> &surface);
    virtual void setVideoSurfaceTexture(const sp<ISurfaceTexture> &surfaceTexture);

protected:
    friend class MediaPlayerNotificationClient;

    // overridden from GenericPlayer
    virtual void onMessageReceived(const sp<AMessage> &msg);

    // Async event handlers (called from GenericPlayer's event loop)
    virtual void onPrepare();
    virtual void onPlay();
    virtual void onPause();
    virtual void onSeek(const sp<AMessage> &msg);
    virtual void onLoop(const sp<AMessage> &msg);
    virtual void onVolumeUpdate();
    virtual void onBufferingUpdate(const sp<AMessage> &msg);
    virtual void onGetMediaPlayerInfo();

    bool mHasVideo;
    int32_t mSeekTimeMsec;

    sp<Surface> mVideoSurface;
    sp<ISurfaceTexture> mVideoSurfaceTexture;

    sp<IMediaPlayer> mPlayer;
    // Receives Android MediaPlayer events from mPlayer
    sp<MediaPlayerNotificationClient> mPlayerClient;

    sp<IServiceManager> mServiceManager;
    sp<IBinder> mBinder;
    sp<IMediaPlayerService> mMediaPlayerService;

    Parcel metadatafilter;

    // for synchronous "getXXX" calls on misc MediaPlayer settings: currently querying:
    //   - position (time): protects GenericPlayer::mPositionMsec
    Mutex       mGetMediaPlayerInfoLock;
    Condition   mGetMediaPlayerInfoCondition;
    //  marks the current "generation" of MediaPlayer info,any change denotes more recent info,
    //    protected by mGetMediaPlayerInfoLock
    uint32_t    mGetMediaPlayerInfoGenCount;

private:
    DISALLOW_EVIL_CONSTRUCTORS(GenericMediaPlayer);
    void onAfterMediaPlayerPrepared();
};

} // namespace android

#endif /* __ANDROID_GENERICMEDIAPLAYER_H__ */
