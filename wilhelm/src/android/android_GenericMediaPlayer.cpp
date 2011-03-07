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
MediaPlayerNotificationClient::MediaPlayerNotificationClient() :
    mPlayerPrepared(false)
{

}

MediaPlayerNotificationClient::~MediaPlayerNotificationClient() {

}

//--------------------------------------------------
// IMediaPlayerClient implementation
void MediaPlayerNotificationClient::notify(int msg, int ext1, int ext2) {
    SL_LOGI("MediaPlayerNotificationClient::notify(msg=%d, ext1=%d, ext2=%d)", msg, ext1, ext2);

    if (msg == MEDIA_PREPARED) {
        mPlayerPrepared = true;
        mPlayerPreparedCondition.signal();
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
    SL_LOGI("GenericMediaPlayer::GenericMediaPlayer()");

    mServiceManager = defaultServiceManager();
    mBinder = mServiceManager->getService(String16("media.player"));
    mMediaPlayerService = interface_cast<IMediaPlayerService>(mBinder);

    CHECK(mMediaPlayerService.get() != NULL);

    mPlayerClient = new MediaPlayerNotificationClient();
}

GenericMediaPlayer::~GenericMediaPlayer() {
    SL_LOGI("GenericMediaPlayer::~GenericMediaPlayer()");

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
    SL_LOGI("GenericMediaPlayer::onPrepare()");
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
    SL_LOGI("GenericMediaPlayer::onPrepare() done, mStateFlags=0x%x", mStateFlags);
}

void GenericMediaPlayer::onPlay() {
    SL_LOGI("GenericMediaPlayer::onPlay()");
    if ((mStateFlags & kFlagPrepared) && (mPlayer != 0)) {
        SL_LOGI("starting player");
        mPlayer->start();
        mStateFlags |= kFlagPlaying;
    } else {
        SL_LOGV("NOT starting player mStateFlags=0x%x", mStateFlags);
    }
}

void GenericMediaPlayer::onPause() {
    SL_LOGI("GenericMediaPlayer::onPause()");
    if ((mStateFlags & kFlagPrepared) && (mPlayer != 0)) {
        mPlayer->pause();
        mStateFlags &= ~kFlagPlaying;
    }

}

} // namespace android
