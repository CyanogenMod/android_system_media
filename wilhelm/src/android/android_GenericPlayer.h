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


//--------------------------------------------------------------------------------------------------
namespace android {

class GenericPlayer : public AHandler
{
public:

    enum {
        kEventPrepared                = 'prep'
    };

    GenericPlayer(const AudioPlayback_Parameters* params);
    virtual ~GenericPlayer();

    virtual void init(const notif_cbf_t cbf, void* notifUser);

    virtual void setDataSource(const char *uri);
    virtual void setDataSource(int fd, int64_t offset, int64_t length);

    virtual void prepare();
    virtual void play();
    virtual void pause();
    virtual void stop();
    virtual void seek(int64_t timeMsec);
    virtual void loop(bool loop);

protected:

    void resetDataLocator();
    DataLocator2 mDataLocator;
    int          mDataLocatorType;

    enum {
        kWhatPrepare    = 'prep',
        kWhatNotif      = 'noti',
        kWhatPlay       = 'play',
        kWhatPause      = 'paus',
        kWhatSeek       = 'seek',
        kWhatLoop       = 'loop',
    };

    // Send a notification to one of the event listeners
    virtual void notify(const char* event, int data, bool async);

    // AHandler implementation
    virtual void onMessageReceived(const sp<AMessage> &msg);

    // Async event handlers (called from GenericPlayer's event loop)
    virtual void onPrepare();
    virtual void onNotify(const sp<AMessage> &msg);
    virtual void onPlay();
    virtual void onPause();
    virtual void onSeek(const sp<AMessage> &msg);
    virtual void onLoop(const sp<AMessage> &msg);

    // Event notification from GenericPlayer to OpenSL ES / OpenMAX AL framework
    notif_cbf_t mNotifyClient;
    void*       mNotifyUser;

    enum {
        kFlagPrepared  = 1 <<0,
        kFlagPreparing = 1 <<1,
        kFlagPlaying   = 1 <<2,
        kFlagBuffering = 1 <<3,
        kFlagSeeking   = 1 <<4,
        kFlagLooping   = 1 <<5,
    };

    uint32_t mStateFlags;

    sp<ALooper> mLooper;
    int32_t mLooperPriority;

    AudioPlayback_Parameters mPlaybackParams;

private:
    DISALLOW_EVIL_CONSTRUCTORS(GenericPlayer);
};

} // namespace android
