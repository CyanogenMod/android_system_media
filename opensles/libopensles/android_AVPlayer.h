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
    AVPlayer(AudioPlayback_Parameters* params);
    virtual ~AVPlayer();

    virtual void init();

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

    // AHandler implementation
    virtual void onMessageReceived(const sp<AMessage> &msg);

    sp<ALooper> mLooper;

    AudioPlayback_Parameters mPlaybackParams;

    sp<IMediaPlayer> mPlayer;
    sp<MediaPlayerNotificationClient> mPlayerClient; // receives events from mPlayer

    sp<IServiceManager> mServiceManager;
    sp<IBinder> mBinder;
    sp<IMediaPlayerService> mMediaPlayerService;

    Mutex mLock;

    // Event handlers
    virtual void onPrepare();
    //virtual void onNotif(const sp<AMessage> &msg);
    virtual void onPlay();
    virtual void onPause();
    virtual void onStop();
};

} // namespace android
