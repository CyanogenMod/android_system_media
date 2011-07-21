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

#ifndef __ANDROID_GENERICPLAYER_H__
#define __ANDROID_GENERICPLAYER_H__

#include <media/stagefright/foundation/AHandler.h>
#include <media/stagefright/foundation/ALooper.h>
#include <media/stagefright/foundation/AMessage.h>

//--------------------------------------------------------------------------------------------------
/**
 * Message parameters for AHandler messages, see list in GenericPlayer::kWhatxxx
 */
#define WHATPARAM_SEEK_SEEKTIME_MS                  "seekTimeMs"
#define WHATPARAM_LOOP_LOOPING                      "looping"
#define WHATPARAM_BUFFERING_UPDATE                  "bufferingUpdate"
#define WHATPARAM_BUFFERING_UPDATETHRESHOLD_PERCENT "buffUpdateThreshold"

namespace android {

class GenericPlayer : public AHandler
{
public:

    enum {
        kEventPrepared                = 'prep',
        kEventHasVideoSize            = 'vsiz',
        kEventPrefetchStatusChange    = 'pfsc',
        kEventPrefetchFillLevelUpdate = 'pflu',
        kEventEndOfStream             = 'eos'
    };


    GenericPlayer(const AudioPlayback_Parameters* params);
    virtual ~GenericPlayer();

    virtual void init(const notif_cbf_t cbf, void* notifUser);
    virtual void preDestroy();

    virtual void setDataSource(const char *uri);
    virtual void setDataSource(int fd, int64_t offset, int64_t length);

    virtual void prepare();
    virtual void play();
    virtual void pause();
    virtual void stop();
    virtual void seek(int64_t timeMsec);
    virtual void loop(bool loop);
    virtual void setBufferingUpdateThreshold(int16_t thresholdPercent);

    virtual void getDurationMsec(int* msec); //msec != NULL, ANDROID_UNKNOWN_TIME if unknown
    virtual void getPositionMsec(int* msec); //msec != NULL, ANDROID_UNKNOWN_TIME if unknown
    virtual void getSampleRate(uint32_t* hz);// hz  != NULL, UNKNOWN_SAMPLERATE if unknown

    void setVolume(bool mute, bool useStereoPos, XApermille stereoPos, XAmillibel volume);

protected:
    // mutex used for set vs use of volume and cache (fill, threshold) settings
    Mutex mSettingsLock;

    void resetDataLocator();
    DataLocator2 mDataLocator;
    int          mDataLocatorType;

    // Constants used to identify the messages in this player's AHandler message loop
    //   in onMessageReceived()
    enum {
        kWhatPrepare         = 'prep',
        kWhatNotif           = 'noti',
        kWhatPlay            = 'play',
        kWhatPause           = 'paus',
        kWhatSeek            = 'seek',
        kWhatSeekComplete    = 'skcp',
        kWhatLoop            = 'loop',
        kWhatVolumeUpdate    = 'volu',
        kWhatBufferingUpdate = 'bufu',
        kWhatBuffUpdateThres = 'buut',
        kWhatMediaPlayerInfo = 'mpin'
    };

    // Send a notification to one of the event listeners
    virtual void notify(const char* event, int data1, bool async);
    virtual void notify(const char* event, int data1, int data2, bool async);

    // AHandler implementation
    virtual void onMessageReceived(const sp<AMessage> &msg);

    // Async event handlers (called from GenericPlayer's event loop)
    virtual void onPrepare();
    virtual void onNotify(const sp<AMessage> &msg);
    virtual void onPlay();
    virtual void onPause();
    virtual void onSeek(const sp<AMessage> &msg);
    virtual void onLoop(const sp<AMessage> &msg);
    virtual void onVolumeUpdate();
    virtual void onSeekComplete();
    virtual void onBufferingUpdate(const sp<AMessage> &msg);
    virtual void onSetBufferingUpdateThreshold(const sp<AMessage> &msg);

    // Convenience methods
    //   for async notifications of prefetch status and cache fill level, needs to be called
    //     with mSettingsLock locked
    void notifyStatus();
    void notifyCacheFill();
    //   for internal async notification to update state that the player is no longer seeking
    void seekComplete();
    void bufferingUpdate(int16_t fillLevelPerMille);

    // Event notification from GenericPlayer to OpenSL ES / OpenMAX AL framework
    notif_cbf_t mNotifyClient;
    void*       mNotifyUser;
    // lock to protect mNotifyClient and mNotifyUser updates
    Mutex       mNotifyClientLock;

    // Bits for mStateFlags
    enum {
        kFlagPrepared               = 1 << 0,   // use only for successful preparation
        kFlagPreparing              = 1 << 1,
        kFlagPlaying                = 1 << 2,
        kFlagBuffering              = 1 << 3,
        kFlagSeeking                = 1 << 4,
        kFlagLooping                = 1 << 5,
        kFlagPreparedUnsuccessfully = 1 << 6,
    };

    uint32_t mStateFlags;

    sp<ALooper> mLooper;
    int32_t mLooperPriority;

    AudioPlayback_Parameters mPlaybackParams;

    AndroidAudioLevels mAndroidAudioLevels;
    int mChannelCount; // this is used for the panning law, and is not exposed outside of the object
    int32_t mDurationMsec;
    // position is not protected by any lock in this generic class, may be different in subclasses
    int32_t mPositionMsec;
    uint32_t mSampleRateHz;

    CacheStatus_t mCacheStatus;
    int16_t mCacheFill; // cache fill level + played back level in permille
    int16_t mLastNotifiedCacheFill; // last cache fill level communicated to the listener
    int16_t mCacheFillNotifThreshold; // threshold in cache fill level for cache fill to be reported

private:
    DISALLOW_EVIL_CONSTRUCTORS(GenericPlayer);
};

} // namespace android

#endif /* __ANDROID_GENERICPLAYER_H__ */
