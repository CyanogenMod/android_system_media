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
#include <media/IMediaPlayerService.h>
#include <surfaceflinger/ISurfaceComposer.h>
#include <surfaceflinger/SurfaceComposerClient.h>

namespace android {

//--------------------------------------------------------------------------------------------------
MediaPlayerNotificationClient::MediaPlayerNotificationClient(GenericMediaPlayer* gmp) :
    mGenericMediaPlayer(gmp),
    mPlayerPrepared(false)
{

}

MediaPlayerNotificationClient::~MediaPlayerNotificationClient() {

}

//--------------------------------------------------
// IMediaPlayerClient implementation
void MediaPlayerNotificationClient::notify(int msg, int ext1, int ext2) {
    SL_LOGI("MediaPlayerNotificationClient::notify(msg=%d, ext1=%d, ext2=%d)", msg, ext1, ext2);

    switch (msg) {
      case MEDIA_PREPARED:
        mPlayerPrepared = true;
        mPlayerPreparedCondition.signal();
        break;

      case MEDIA_SET_VIDEO_SIZE:
        mGenericMediaPlayer->notify(PLAYEREVENT_VIDEO_SIZE_UPDATE,
                (int32_t)ext1, (int32_t)ext2, true /*async*/);
        break;

      default: { }
    }
}

//--------------------------------------------------
void MediaPlayerNotificationClient::blockUntilPlayerPrepared() {
    Mutex::Autolock _l(mLock);
    while (!mPlayerPrepared) {
        mPlayerPreparedCondition.wait(mLock);
    }
}

//--------------------------------------------------------------------------------------------------
GenericMediaPlayer::GenericMediaPlayer(const AudioPlayback_Parameters* params, bool hasVideo) :
    GenericPlayer(params),
    mHasVideo(hasVideo),
    mVideoSurface(0),
    mVideoSurfaceTexture(0),
    mPlayer(0),
    mPlayerClient(0)
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

//--------------------------------------------------
void GenericMediaPlayer::setVideoSurface(const sp<Surface> &surface) {
    mVideoSurface = surface;
}

void GenericMediaPlayer::setVideoSurfaceTexture(const sp<ISurfaceTexture> &surfaceTexture) {
    mVideoSurfaceTexture = surfaceTexture;
}


//--------------------------------------------------
// Event handlers
void GenericMediaPlayer::onPrepare() {
    SL_LOGD("GenericMediaPlayer::onPrepare()");
    if (!(mStateFlags & kFlagPrepared) && (mPlayer != 0)) {
        if (mHasVideo) {
            if (mVideoSurface != 0) {
                mPlayer->setVideoSurface(mVideoSurface);
            } else if (mVideoSurfaceTexture != 0) {
                mPlayer->setVideoSurfaceTexture(mVideoSurfaceTexture);
            }
        }
        mPlayer->setAudioStreamType(mPlaybackParams.streamType);
        mPlayer->prepareAsync();
        mPlayerClient->blockUntilPlayerPrepared();
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


void GenericMediaPlayer::onVolumeUpdate() {
    // use settings lock to read the volume settings
    Mutex::Autolock _l(mSettingsLock);
    if (this->mAndroidAudioLevels.mMute) {
        mPlayer->setVolume(0.0f, 0.0f);
    } else {
        mPlayer->setVolume(mAndroidAudioLevels.mFinalVolume[0],
                mAndroidAudioLevels.mFinalVolume[1]);
    }

}

} // namespace android
