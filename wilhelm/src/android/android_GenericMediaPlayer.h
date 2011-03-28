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
    virtual void notify(int msg, int ext1, int ext2);

    void blockUntilPlayerPrepared();

private:
    Mutex mLock;
    GenericMediaPlayer* mGenericMediaPlayer;
    Condition mPlayerPreparedCondition;
    bool mPlayerPrepared;
};


//--------------------------------------------------------------------------------------------------
class GenericMediaPlayer : public GenericPlayer
{
public:

    GenericMediaPlayer(const AudioPlayback_Parameters* params, bool hasVideo);
    virtual ~GenericMediaPlayer();

    virtual void setVideoSurface(const sp<Surface> &surface);
    virtual void setVideoSurfaceTexture(const sp<ISurfaceTexture> &surfaceTexture);

protected:
    friend class MediaPlayerNotificationClient;

    // Async event handlers (called from GenericPlayer's event loop)
    virtual void onPrepare();
    virtual void onPlay();
    virtual void onPause();
    virtual void onVolumeUpdate();

    bool mHasVideo;

    sp<Surface> mVideoSurface;
    sp<ISurfaceTexture> mVideoSurfaceTexture;

    sp<IMediaPlayer> mPlayer;
    // Receives Android MediaPlayer events from mPlayer
    sp<MediaPlayerNotificationClient> mPlayerClient;

    sp<IServiceManager> mServiceManager;
    sp<IBinder> mBinder;
    sp<IMediaPlayerService> mMediaPlayerService;

private:
    DISALLOW_EVIL_CONSTRUCTORS(GenericMediaPlayer);
};

} // namespace android

#endif /* __ANDROID_GENERICMEDIAPLAYER_H__ */
