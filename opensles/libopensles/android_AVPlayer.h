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


#include <binder/IServiceManager.h>


//--------------------------------------------------------------------------------------------------
namespace android {

class MediaPlayerNotificationClient : public BnMediaPlayerClient
{
public:
    MediaPlayerNotificationClient();
    virtual ~MediaPlayerNotificationClient();

    // IMediaPlayerClient implementation
    virtual void notify(int msg, int ext1, int ext2);

    void blockUntilPlayerPrepared();

private:
    Mutex mLock;
    Condition mPlayerPreparedCondition;
    bool mPlayerPrepared;
};

//--------------------------------------------------------------------------------------------------
class AVPlayer : public AHandler
{
public:

    enum {
        kEventPrepared                = 'prep'
    };

    AVPlayer(AudioPlayback_Parameters* params);
    virtual ~AVPlayer();

    virtual void init(const notif_client_t cbf, void* notifUser);
    virtual void setVideoSurface(void* surface);

    virtual void prepare();
    virtual void play();
    virtual void pause();
    virtual void stop();

protected:

    enum {
        kWhatPrepare    = 'prep',
        kWhatNotif      = 'noti',
        kWhatPlay       = 'play',
        kWhatPause      = 'paus',
        kWhatStop       = 'stop'
    };

    // Send a notification to one of the event listeners
    virtual void notify(const char* event, int data, bool async);

    // AHandler implementation
    virtual void onMessageReceived(const sp<AMessage> &msg);

    // Async event handlers (called from AVPlayer's event loop)
    virtual void onPrepare();
    virtual void onNotify(const sp<AMessage> &msg);
    virtual void onPlay();
    virtual void onPause();
    virtual void onStop();

    // Event notification from AVPlayer to OpenSL ES / OpenMAX AL framework
    notif_client_t mNotifyClient;
    void*          mNotifyUser;

    enum {
        kFlagPrepared  = 1 <<0,
        kFlagPlaying   = 1 <<1,
        /*kFlagBuffering = 1 <<2,
        kFlagSeeking   = 1 <<3,
        kFlagLooping   = 1 <<4,*/
    };

    uint32_t mStateFlags;

    sp<ALooper> mLooper;

    AudioPlayback_Parameters mPlaybackParams;
    sp<Surface> mVideoSurface;

    sp<IMediaPlayer> mPlayer;
    // Receives Android MediaPlayer events from mPlayer
    sp<MediaPlayerNotificationClient> mPlayerClient;

    sp<IServiceManager> mServiceManager;
    sp<IBinder> mBinder;
    sp<IMediaPlayerService> mMediaPlayerService;

    Mutex mLock;

};

} // namespace android
