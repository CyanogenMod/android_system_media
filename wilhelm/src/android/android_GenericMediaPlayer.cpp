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
#include "android_GenericMediaPlayer.h"

#include <media/IMediaPlayerService.h>
#include <surfaceflinger/ISurfaceComposer.h>
#include <surfaceflinger/SurfaceComposerClient.h>
#include <media/stagefright/foundation/ADebug.h>

namespace android {

// default delay in Us used when reposting an event when the player is not ready to accept
// the command yet. This is for instance used when seeking on a MediaPlayer that's still preparing
#define DEFAULT_COMMAND_DELAY_FOR_REPOST_US (100*1000) // 100ms

static const char* const kDistantProtocolPrefix[] = { "http:", "https:", "ftp:", "rtp:", "rtsp:"};
#define NB_DISTANT_PROTOCOLS (sizeof(kDistantProtocolPrefix)/sizeof(kDistantProtocolPrefix[0]))

//--------------------------------------------------------------------------------------------------
MediaPlayerNotificationClient::MediaPlayerNotificationClient(GenericMediaPlayer* gmp) :
    mGenericMediaPlayer(gmp),
    mPlayerPrepared(PREPARE_NOT_STARTED)
{
    SL_LOGV("MediaPlayerNotificationClient::MediaPlayerNotificationClient()");
}

MediaPlayerNotificationClient::~MediaPlayerNotificationClient() {
    SL_LOGV("MediaPlayerNotificationClient::~MediaPlayerNotificationClient()");
}

// Map a MEDIA_* enum to a string
static const char *media_to_string(int msg)
{
    switch (msg) {
#define _(x) case MEDIA_##x: return "MEDIA_" #x;
      _(PREPARED)
      _(SET_VIDEO_SIZE)
      _(SEEK_COMPLETE)
      _(PLAYBACK_COMPLETE)
      _(BUFFERING_UPDATE)
      _(ERROR)
      _(NOP)
      _(TIMED_TEXT)
      _(INFO)
#undef _
    default:
        return NULL;
    }
}

//--------------------------------------------------
// IMediaPlayerClient implementation
void MediaPlayerNotificationClient::notify(int msg, int ext1, int ext2, const Parcel *obj) {
    SL_LOGV("MediaPlayerNotificationClient::notify(msg=%s (%d), ext1=%d, ext2=%d)",
            media_to_string(msg), msg, ext1, ext2);

    switch (msg) {
      case MEDIA_PREPARED:
        mPlayerPrepared = PREPARE_COMPLETED_SUCCESSFULLY;
        mPlayerPreparedCondition.signal();
        break;

      case MEDIA_SET_VIDEO_SIZE:
        // only send video size updates if the player was flagged as having video, to avoid
        // sending video size updates of (0,0)
        if (mGenericMediaPlayer->mHasVideo) {
            mGenericMediaPlayer->notify(PLAYEREVENT_VIDEO_SIZE_UPDATE,
                    (int32_t)ext1, (int32_t)ext2, true /*async*/);
        }
        break;

      case MEDIA_SEEK_COMPLETE:
          mGenericMediaPlayer->seekComplete();
        break;

      case MEDIA_PLAYBACK_COMPLETE:
        mGenericMediaPlayer->notify(PLAYEREVENT_ENDOFSTREAM, 1, true /*async*/);
        break;

      case MEDIA_BUFFERING_UPDATE:
        // values received from Android framework for buffer fill level use percent,
        //   while SL/XA use permille, so does GenericPlayer
        mGenericMediaPlayer->bufferingUpdate(ext1 * 10 /*fillLevelPerMille*/);
        break;

      case MEDIA_ERROR:
        mPlayerPrepared = PREPARE_COMPLETED_UNSUCCESSFULLY;
        mPlayerPreparedCondition.signal();
        break;

      case MEDIA_NOP:
      case MEDIA_TIMED_TEXT:
      case MEDIA_INFO:
        break;

      default: { }
    }

}

//--------------------------------------------------
void MediaPlayerNotificationClient::beforePrepare()
{
    Mutex::Autolock _l(mLock);
    assert(mPlayerPrepared == PREPARE_NOT_STARTED);
    mPlayerPrepared = PREPARE_IN_PROGRESS;
}

//--------------------------------------------------
bool MediaPlayerNotificationClient::blockUntilPlayerPrepared() {
    Mutex::Autolock _l(mLock);
    assert(mPlayerPrepared != PREPARE_NOT_STARTED);
    while (mPlayerPrepared == PREPARE_IN_PROGRESS) {
        mPlayerPreparedCondition.wait(mLock);
    }
    assert(mPlayerPrepared == PREPARE_COMPLETED_SUCCESSFULLY ||
            mPlayerPrepared == PREPARE_COMPLETED_UNSUCCESSFULLY);
    return mPlayerPrepared == PREPARE_COMPLETED_SUCCESSFULLY;
}

//--------------------------------------------------------------------------------------------------
GenericMediaPlayer::GenericMediaPlayer(const AudioPlayback_Parameters* params, bool hasVideo) :
    GenericPlayer(params),
    mHasVideo(hasVideo),
    mSeekTimeMsec(0),
    mVideoSurface(0),
    mVideoSurfaceTexture(0),
    mPlayer(0),
    mPlayerClient(0),
    mGetMediaPlayerInfoGenCount(0)
{
    SL_LOGD("GenericMediaPlayer::GenericMediaPlayer()");

    mServiceManager = defaultServiceManager();
    mBinder = mServiceManager->getService(String16("media.player"));
    mMediaPlayerService = interface_cast<IMediaPlayerService>(mBinder);

    CHECK(mMediaPlayerService.get() != NULL);

    mPlayerClient = new MediaPlayerNotificationClient(this);
}

GenericMediaPlayer::~GenericMediaPlayer() {
    SL_LOGD("GenericMediaPlayer::~GenericMediaPlayer()");
}

void GenericMediaPlayer::preDestroy() {
    SL_LOGD("GenericMediaPlayer::preDestroy()");
    // we might be in the middle of blocking for a getXXX call
    {
        android::Mutex::Autolock autoLock(mGetMediaPlayerInfoLock);
        mGetMediaPlayerInfoGenCount++;
        mGetMediaPlayerInfoCondition.broadcast();
    }
    GenericPlayer::preDestroy();
}

//--------------------------------------------------
// overridden from GenericPlayer
// pre-condition:
//   msec != NULL
// post-condition
//   *msec == mPositionMsec ==
//                  ANDROID_UNKNOWN_TIME if position is unknown at time of query,
//               or the current MediaPlayer position
void GenericMediaPlayer::getPositionMsec(int* msec) {
    SL_LOGD("GenericMediaPlayer::getPositionMsec()");
    uint32_t currentGen = 0;
    {
        android::Mutex::Autolock autoLock(mGetMediaPlayerInfoLock);
        currentGen = mGetMediaPlayerInfoGenCount;
    }
    // send a message to update the MediaPlayer position in the event loop where it's safe to
    //   access the MediaPlayer. We block until the message kWhatMediaPlayerInfo has been processed
    (new AMessage(kWhatMediaPlayerInfo, id()))->post();
    {
        android::Mutex::Autolock autoLock(mGetMediaPlayerInfoLock);
        // mGetMediaPlayerInfoGenCount will be incremented when the kWhatMediaPlayerInfo
        //  gets processed.
        while (currentGen == mGetMediaPlayerInfoGenCount) {
            mGetMediaPlayerInfoCondition.wait(mGetMediaPlayerInfoLock);
            // if multiple GetPosition calls were issued before any got processed on the event queue
            // then they will all return the same "recent-enough" position
        }
        // at this point mPositionMsec has been updated
        // so now updates msec from mPositionMsec, while holding the lock protecting it
        GenericPlayer::getPositionMsec(msec);
    }
}

//--------------------------------------------------
void GenericMediaPlayer::setVideoSurface(const sp<Surface> &surface) {
    SL_LOGV("GenericMediaPlayer::setVideoSurface()");
    mVideoSurface = surface;
}

void GenericMediaPlayer::setVideoSurfaceTexture(const sp<ISurfaceTexture> &surfaceTexture) {
    SL_LOGV("GenericMediaPlayer::setVideoSurfaceTexture()");
    mVideoSurfaceTexture = surfaceTexture;
}

//--------------------------------------------------
void GenericMediaPlayer::onMessageReceived(const sp<AMessage> &msg) {
    SL_LOGV("GenericMediaPlayer::onMessageReceived()");
    switch (msg->what()) {
        case kWhatMediaPlayerInfo:
            onGetMediaPlayerInfo();
            break;

        default:
            GenericPlayer::onMessageReceived(msg);
            break;
    }
}

//--------------------------------------------------
// Event handlers

void GenericMediaPlayer::onPrepare() {
    SL_LOGD("GenericMediaPlayer::onPrepare()");
    // Attempt to prepare at most once, and only if there is a MediaPlayer
    if (!(mStateFlags & (kFlagPrepared | kFlagPreparedUnsuccessfully)) && (mPlayer != 0)) {
        if (mHasVideo) {
            if (mVideoSurface != 0) {
                mPlayer->setVideoSurface(mVideoSurface);
            } else if (mVideoSurfaceTexture != 0) {
                mPlayer->setVideoSurfaceTexture(mVideoSurfaceTexture);
            }
        }
        mPlayer->setAudioStreamType(mPlaybackParams.streamType);
        mPlayerClient->beforePrepare();
        mPlayer->prepareAsync();
        mStateFlags |= mPlayerClient->blockUntilPlayerPrepared() ?
                kFlagPrepared : kFlagPreparedUnsuccessfully;
        onAfterMediaPlayerPrepared();
        GenericPlayer::onPrepare();
    }
    SL_LOGD("GenericMediaPlayer::onPrepare() done, mStateFlags=0x%x", mStateFlags);
}


void GenericMediaPlayer::onPlay() {
    SL_LOGD("GenericMediaPlayer::onPlay()");
    if ((mStateFlags & kFlagPrepared) && (mPlayer != 0)) {
        SL_LOGD("starting player");
        mPlayer->start();
        mStateFlags |= kFlagPlaying;
    } else {
        SL_LOGV("NOT starting player mStateFlags=0x%x", mStateFlags);
    }
}


void GenericMediaPlayer::onPause() {
    SL_LOGD("GenericMediaPlayer::onPause()");
    if ((mStateFlags & kFlagPrepared) && (mPlayer != 0)) {
        mPlayer->pause();
        mStateFlags &= ~kFlagPlaying;
    }
}

/**
 * pre-condition: WHATPARAM_SEEK_SEEKTIME_MS parameter value >= 0
 */
void GenericMediaPlayer::onSeek(const sp<AMessage> &msg) {
    SL_LOGV("GenericMediaPlayer::onSeek");
    int64_t timeMsec = ANDROID_UNKNOWN_TIME;
    if (!msg->findInt64(WHATPARAM_SEEK_SEEKTIME_MS, &timeMsec)) {
        // invalid command, drop it
        return;
    }
    if ((mStateFlags & kFlagSeeking) && (timeMsec == mSeekTimeMsec)) {
        // already seeking to the same time, cancel this command
        return;
    } else if (mStateFlags & kFlagPreparedUnsuccessfully) {
        // discard seeks after unsuccessful prepare
    } else if (!(mStateFlags & kFlagPrepared)) {
        // we are not ready to accept a seek command at this time, retry later
        msg->post(DEFAULT_COMMAND_DELAY_FOR_REPOST_US);
    } else {
        if (msg->findInt64(WHATPARAM_SEEK_SEEKTIME_MS, &timeMsec) && (mPlayer != 0)) {
            mStateFlags |= kFlagSeeking;
            mSeekTimeMsec = (int32_t)timeMsec;
            if (OK != mPlayer->seekTo(timeMsec)) {
                mStateFlags &= ~kFlagSeeking;
                mSeekTimeMsec = ANDROID_UNKNOWN_TIME;
            } else {
                mPositionMsec = mSeekTimeMsec;
            }
        }
    }
}


void GenericMediaPlayer::onLoop(const sp<AMessage> &msg) {
    SL_LOGV("GenericMediaPlayer::onLoop");
    int32_t loop = 0;
    if (msg->findInt32(WHATPARAM_LOOP_LOOPING, &loop)) {
        if (mPlayer != 0 && OK == mPlayer->setLooping(loop)) {
            if (loop) {
                mStateFlags |= kFlagLooping;
            } else {
                mStateFlags &= ~kFlagLooping;
            }
        }
    }
}


void GenericMediaPlayer::onVolumeUpdate() {
    SL_LOGD("GenericMediaPlayer::onVolumeUpdate()");
    // use settings lock to read the volume settings
    Mutex::Autolock _l(mSettingsLock);
    if (mPlayer != 0) {
        if (mAndroidAudioLevels.mMute) {
            mPlayer->setVolume(0.0f, 0.0f);
        } else {
            mPlayer->setVolume(mAndroidAudioLevels.mFinalVolume[0],
                    mAndroidAudioLevels.mFinalVolume[1]);
        }
    }
}


void GenericMediaPlayer::onBufferingUpdate(const sp<AMessage> &msg) {
    int32_t fillLevel = 0;
    if (msg->findInt32(WHATPARAM_BUFFERING_UPDATE, &fillLevel)) {
        SL_LOGD("GenericMediaPlayer::onBufferingUpdate(fillLevel=%d)", fillLevel);

        Mutex::Autolock _l(mSettingsLock);
        mCacheFill = fillLevel;
        // handle cache fill update
        if (mCacheFill - mLastNotifiedCacheFill >= mCacheFillNotifThreshold) {
            notifyCacheFill();
        }
        // handle prefetch status update
        //   compute how much time ahead of position is buffered
        int durationMsec, positionMsec = -1;
        if ((mStateFlags & kFlagPrepared) && (mPlayer != 0)
                && (OK == mPlayer->getDuration(&durationMsec))
                        && (OK == mPlayer->getCurrentPosition(&positionMsec))) {
            if ((-1 != durationMsec) && (-1 != positionMsec)) {
                // evaluate prefetch status based on buffer time thresholds
                int64_t bufferedDurationMsec = (durationMsec * fillLevel / 100) - positionMsec;
                CacheStatus_t newCacheStatus = mCacheStatus;
                if (bufferedDurationMsec > DURATION_CACHED_HIGH_MS) {
                    newCacheStatus = kStatusHigh;
                } else if (bufferedDurationMsec > DURATION_CACHED_MED_MS) {
                    newCacheStatus = kStatusEnough;
                } else if (bufferedDurationMsec > DURATION_CACHED_LOW_MS) {
                    newCacheStatus = kStatusIntermediate;
                } else if (bufferedDurationMsec == 0) {
                    newCacheStatus = kStatusEmpty;
                } else {
                    newCacheStatus = kStatusLow;
                }

                if (newCacheStatus != mCacheStatus) {
                    mCacheStatus = newCacheStatus;
                    notifyStatus();
                }
            }
        }
    } else {
        SL_LOGV("GenericMediaPlayer::onBufferingUpdate(fillLevel=unknown)");
    }
}


void GenericMediaPlayer::onGetMediaPlayerInfo() {
    SL_LOGD("GenericMediaPlayer::onGetMediaPlayerInfo()");
    {
        android::Mutex::Autolock autoLock(mGetMediaPlayerInfoLock);

        if ((!(mStateFlags & kFlagPrepared)) || (mPlayer == 0)) {
            mPositionMsec = ANDROID_UNKNOWN_TIME;
        } else {
            mPlayer->getCurrentPosition(&mPositionMsec);
        }

        // the MediaPlayer info has been refreshed
        mGetMediaPlayerInfoGenCount++;
        // there might be multiple requests for MediaPlayer info, so use broadcast instead of signal
        mGetMediaPlayerInfoCondition.broadcast();
    }
}


//--------------------------------------------------
/**
 * called from the event handling loop
 * pre-condition: mPlayer is prepared
 */
void GenericMediaPlayer::onAfterMediaPlayerPrepared() {
    SL_LOGV("GenericMediaPlayer::onAfterMediaPlayerPrepared()");
    // the MediaPlayer mPlayer is prepared, retrieve its duration
    // FIXME retrieve channel count
    {
        Mutex::Autolock _l(mSettingsLock);
        int msec = 0;
        if (OK == mPlayer->getDuration(&msec)) {
            mDurationMsec = msec;
        }
    }
    // when the MediaPlayer mPlayer is prepared, there is "sufficient data" in the playback buffers
    // if the data source was local, and the buffers are considered full so we need to notify that
    bool isLocalSource = true;
    if (kDataLocatorUri == mDataLocatorType) {
        for (unsigned int i = 0 ; i < NB_DISTANT_PROTOCOLS ; i++) {
            if (!strncasecmp(mDataLocator.uriRef,
                    kDistantProtocolPrefix[i], strlen(kDistantProtocolPrefix[i]))) {
                isLocalSource = false;
                break;
            }
        }
    }
    if (isLocalSource) {
        SL_LOGD("media player prepared on local source");
        {
            Mutex::Autolock _l(mSettingsLock);
            mCacheStatus = kStatusHigh;
            mCacheFill = 1000;
            notifyStatus();
            notifyCacheFill();
        }
    } else {
        SL_LOGD("media player prepared on non-local source");
    }
}

} // namespace android
