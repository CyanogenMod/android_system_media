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

//#define USE_LOG SLAndroidLogLevel_Verbose

#include "sles_allinclusive.h"

#include <media/stagefright/foundation/ADebug.h>
#include <sys/stat.h>

namespace android {

//--------------------------------------------------------------------------------------------------
GenericPlayer::GenericPlayer(const AudioPlayback_Parameters* params) :
        mDataLocatorType(kDataLocatorNone),
        mNotifyClient(NULL),
        mNotifyUser(NULL),
        mStateFlags(0),
        mLooperPriority(PRIORITY_DEFAULT),
        mPlaybackParams(*params),
        mChannelCount(1),
        mDurationMsec(ANDROID_UNKNOWN_TIME),
        mPositionMsec(ANDROID_UNKNOWN_TIME),
        mCacheStatus(kStatusEmpty),
        mCacheFill(0),
        mLastNotifiedCacheFill(0),
        mCacheFillNotifThreshold(100)
{
    SL_LOGD("GenericPlayer::GenericPlayer()");

    mLooper = new android::ALooper();

    mAndroidAudioLevels.mMute = false;
    mAndroidAudioLevels.mFinalVolume[0] = 1.0f;
    mAndroidAudioLevels.mFinalVolume[1] = 1.0f;
}


GenericPlayer::~GenericPlayer() {
    SL_LOGI("GenericPlayer::~GenericPlayer()");

    mLooper->stop();
    mLooper->unregisterHandler(id());
    mLooper.clear();

}


void GenericPlayer::init(const notif_cbf_t cbf, void* notifUser) {
    SL_LOGD("GenericPlayer::init()");

    mNotifyClient = cbf;
    mNotifyUser = notifUser;

    mLooper->registerHandler(this);
    mLooper->start(false /*runOnCallingThread*/, false /*canCallJava*/, mLooperPriority);
}


void GenericPlayer::setDataSource(const char *uri) {
    resetDataLocator();

    mDataLocator.uriRef = uri;

    mDataLocatorType = kDataLocatorUri;
}


void GenericPlayer::setDataSource(int fd, int64_t offset, int64_t length) {
    resetDataLocator();

    mDataLocator.fdi.fd = fd;

    struct stat sb;
    int ret = fstat(fd, &sb);
    if (ret != 0) {
        SL_LOGE("GenericPlayer::setDataSource: fstat(%d) failed: %d, %s", fd, ret, strerror(errno));
        return;
    }

    if (offset >= sb.st_size) {
        SL_LOGE("SfPlayer::setDataSource: invalid offset");
        return;
    }
    mDataLocator.fdi.offset = offset;

    if (PLAYER_FD_FIND_FILE_SIZE == length) {
        mDataLocator.fdi.length = sb.st_size;
    } else if (offset + length > sb.st_size) {
        mDataLocator.fdi.length = sb.st_size - offset;
    } else {
        mDataLocator.fdi.length = length;
    }

    mDataLocatorType = kDataLocatorFd;
}


void GenericPlayer::prepare() {
    SL_LOGD("GenericPlayer::prepare()");
    sp<AMessage> msg = new AMessage(kWhatPrepare, id());
    msg->post();
}


void GenericPlayer::play() {
    SL_LOGD("GenericPlayer::play()");
    sp<AMessage> msg = new AMessage(kWhatPlay, id());
    msg->post();
}


void GenericPlayer::pause() {
    SL_LOGD("GenericPlayer::pause()");
    sp<AMessage> msg = new AMessage(kWhatPause, id());
    msg->post();
}


void GenericPlayer::stop() {
    SL_LOGD("GenericPlayer::stop()");
    (new AMessage(kWhatPause, id()))->post();

    // after a stop, playback should resume from the start.
    seek(0);
}


void GenericPlayer::seek(int64_t timeMsec) {
    SL_LOGV("GenericPlayer::seek %lld", timeMsec);
    sp<AMessage> msg = new AMessage(kWhatSeek, id());
    msg->setInt64(WHATPARAM_SEEK_SEEKTIME_MS, timeMsec);
    msg->post();
}


void GenericPlayer::loop(bool loop) {
    sp<AMessage> msg = new AMessage(kWhatLoop, id());
    msg->setInt32(WHATPARAM_LOOP_LOOPING, (int32_t)loop);
    msg->post();
}


void GenericPlayer::setBufferingUpdateThreshold(int16_t thresholdPercent) {
    sp<AMessage> msg = new AMessage(kWhatBuffUpdateThres, id());
    msg->setInt32(WHATPARAM_BUFFERING_UPDATETHRESHOLD_PERCENT, (int32_t)thresholdPercent);
    msg->post();
}


//--------------------------------------------------
void GenericPlayer::getDurationMsec(int* msec) {
    *msec = mDurationMsec;
}

void GenericPlayer::getPositionMsec(int* msec) {
    *msec = mPositionMsec;
}

//--------------------------------------------------
void GenericPlayer::setVolume(bool mute, bool useStereoPos,
        XApermille stereoPos, XAmillibel volume) {

    // compute amplification as the combination of volume level and stereo position
    float leftVol = 1.0f, rightVol = 1.0f;
    //   amplification from volume level
    leftVol  *= sles_to_android_amplification(volume);
    rightVol = leftVol;

    //   amplification from direct level (changed in SLEffectSendtItf and SLAndroidEffectSendItf)
    // FIXME use calculation below when supporting effects
    //leftVol  *= mAndroidAudioLevels.mAmplFromDirectLevel;
    //rightVol *= mAndroidAudioLevels.mAmplFromDirectLevel;

    // amplification from stereo position
    if (useStereoPos) {
        // panning law depends on number of channels of content: stereo panning vs 2ch. balance
        if (1 == mChannelCount) {
            // stereo panning
            double theta = (1000 + stereoPos) * M_PI_4 / 1000.0f; // 0 <= theta <= Pi/2
            leftVol  *= cos(theta);
            rightVol *= sin(theta);
        } else {
            // stereo balance
            if (stereoPos > 0) {
                leftVol  *= (1000 - stereoPos) / 1000.0f;
                rightVol *= 1.0f;
            } else {
                leftVol  *= 1.0f;
                rightVol *= (1000 + stereoPos) / 1000.0f;
            }
        }
    }

    {
        Mutex::Autolock _l(mSettingsLock);
        mAndroidAudioLevels.mMute = mute;
        mAndroidAudioLevels.mFinalVolume[0] = leftVol;
        mAndroidAudioLevels.mFinalVolume[1] = rightVol;
    }

    // send a message for the volume to be updated by the object which implements the volume
    (new AMessage(kWhatVolumeUpdate, id()))->post();
}


//--------------------------------------------------
/*
 * post-condition: mDataLocatorType == kDataLocatorNone
 *
 */
void GenericPlayer::resetDataLocator() {
    mDataLocatorType = kDataLocatorNone;
}


void GenericPlayer::notify(const char* event, int data, bool async) {
    sp<AMessage> msg = new AMessage(kWhatNotif, id());
    msg->setInt32(event, (int32_t)data);
    if (async) {
        msg->post();
    } else {
        this->onNotify(msg);
    }
}


void GenericPlayer::notify(const char* event, int data1, int data2, bool async) {
    sp<AMessage> msg = new AMessage(kWhatNotif, id());
    msg->setRect(event, 0, 0, (int32_t)data1, (int32_t)data2);
    if (async) {
        msg->post();
    } else {
        this->onNotify(msg);
    }
}


//--------------------------------------------------
// AHandler implementation
void GenericPlayer::onMessageReceived(const sp<AMessage> &msg) {
    switch (msg->what()) {
        case kWhatPrepare:
            onPrepare();
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

        case kWhatLoop:
            onLoop(msg);
            break;

        case kWhatVolumeUpdate:
            onVolumeUpdate();
            break;

        case kWhatSeekComplete:
            onSeekComplete();
            break;

        case kWhatBufferingUpdate:
            onBufferingUpdate(msg);
            break;

        case kWhatBuffUpdateThres:
            onSetBufferingUpdateThreshold(msg);
            break;

        default:
            TRESPASS();
    }
}


//--------------------------------------------------
// Event handlers
//  it is strictly verboten to call those methods outside of the event loop

void GenericPlayer::onPrepare() {
    SL_LOGD("GenericPlayer::onPrepare()");
    if (!(mStateFlags & kFlagPrepared)) {
        mStateFlags |= kFlagPrepared;
        notify(PLAYEREVENT_PREPARED, PLAYER_SUCCESS, false /*async*/);
    }
    SL_LOGD("GenericPlayer::onPrepare() done, mStateFlags=0x%x", mStateFlags);
}


void GenericPlayer::onNotify(const sp<AMessage> &msg) {
    if (NULL == mNotifyClient) {
        return;
    }

    int32_t val1, val2;
    if (msg->findInt32(PLAYEREVENT_PREFETCHSTATUSCHANGE, &val1)) {
        SL_LOGV("GenericPlayer notifying %s = %d", PLAYEREVENT_PREFETCHSTATUSCHANGE, val1);
        mNotifyClient(kEventPrefetchStatusChange, val1, 0, mNotifyUser);
    } else if (msg->findInt32(PLAYEREVENT_PREFETCHFILLLEVELUPDATE, &val1)) {
        SL_LOGV("GenericPlayer notifying %s = %d", PLAYEREVENT_PREFETCHFILLLEVELUPDATE, val1);
        mNotifyClient(kEventPrefetchFillLevelUpdate, val1, 0, mNotifyUser);
    } else if (msg->findInt32(PLAYEREVENT_ENDOFSTREAM, &val1)) {
        SL_LOGV("GenericPlayer notifying %s = %d", PLAYEREVENT_ENDOFSTREAM, val1);
        mNotifyClient(kEventEndOfStream, val1, 0, mNotifyUser);
    } else if (msg->findInt32(PLAYEREVENT_PREPARED, &val1)) {
        SL_LOGV("GenericPlayer notifying %s = %d", PLAYEREVENT_PREPARED, val1);
        mNotifyClient(kEventPrepared, val1, 0, mNotifyUser);
    } else if (msg->findRect(PLAYEREVENT_VIDEO_SIZE_UPDATE, &val1, &val2, &val1, &val2)) {
        SL_LOGV("GenericPlayer notifying %s = %d, %d", PLAYEREVENT_VIDEO_SIZE_UPDATE, val1, val2);
        mNotifyClient(kEventHasVideoSize, val1, val2, mNotifyUser);
    }
}


void GenericPlayer::onPlay() {
    SL_LOGD("GenericPlayer::onPlay()");
    if ((mStateFlags & kFlagPrepared)) {
        SL_LOGD("starting player");
        mStateFlags |= kFlagPlaying;
    } else {
        SL_LOGV("NOT starting player mStateFlags=0x%x", mStateFlags);
    }
}


void GenericPlayer::onPause() {
    SL_LOGD("GenericPlayer::onPause()");
    if ((mStateFlags & kFlagPrepared)) {
        mStateFlags &= ~kFlagPlaying;
    }
}


void GenericPlayer::onSeek(const sp<AMessage> &msg) {
    SL_LOGV("GenericPlayer::onSeek");
}


void GenericPlayer::onLoop(const sp<AMessage> &msg) {
    SL_LOGV("GenericPlayer::onLoop");
}


void GenericPlayer::onVolumeUpdate() {

}


void GenericPlayer::onSeekComplete() {
    SL_LOGD("GenericPlayer::onSeekComplete()");
    mStateFlags &= ~kFlagSeeking;
}


void GenericPlayer::onBufferingUpdate(const sp<AMessage> &msg) {

}


void GenericPlayer::onSetBufferingUpdateThreshold(const sp<AMessage> &msg) {
    int32_t thresholdPercent = 0;
    if (msg->findInt32(WHATPARAM_BUFFERING_UPDATETHRESHOLD_PERCENT, &thresholdPercent)) {
        Mutex::Autolock _l(mSettingsLock);
        mCacheFillNotifThreshold = (int16_t)thresholdPercent;
    }
}


//-------------------------------------------------
void GenericPlayer::notifyStatus() {
    notify(PLAYEREVENT_PREFETCHSTATUSCHANGE, (int32_t)mCacheStatus, true /*async*/);
}


void GenericPlayer::notifyCacheFill() {
    mLastNotifiedCacheFill = mCacheFill;
    notify(PLAYEREVENT_PREFETCHFILLLEVELUPDATE, (int32_t)mLastNotifiedCacheFill, true/*async*/);
}


void GenericPlayer::seekComplete() {
    sp<AMessage> msg = new AMessage(kWhatSeekComplete, id());
    msg->post();
}


void GenericPlayer::bufferingUpdate(int16_t fillLevelPerMille) {
    sp<AMessage> msg = new AMessage(kWhatBufferingUpdate, id());
    msg->setInt32(WHATPARAM_BUFFERING_UPDATE, fillLevelPerMille);
    msg->post();
}

} // namespace android
