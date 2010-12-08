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

//#define USE_LOG SLAndroidLogLevel_Verbose

#include "sles_allinclusive.h"
#undef this // FIXME shouldn't have to do this, no pun intended
#include <media/IMediaPlayerService.h>

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
AVPlayer::AVPlayer(AudioPlayback_Parameters* params) :
    mPlayer(0),
    mPlayerClient(0)
{
    SL_LOGI("AVPlayer::AVPlayer()");

    mLooper = new android::ALooper();

    mPlaybackParams = *params;

    mServiceManager = defaultServiceManager();
    mBinder = mServiceManager->getService(String16("media.player"));
    mMediaPlayerService = interface_cast<IMediaPlayerService>(mBinder);

    CHECK(mMediaPlayerService.get() != NULL);

    mPlayerClient = new MediaPlayerNotificationClient();
}

AVPlayer::~AVPlayer() {
    SL_LOGI("AVPlayer::~AVPlayer()");

    mLooper->stop();
    mLooper->unregisterHandler(id());
    mLooper.clear();

    mPlayerClient.clear();
    mPlayer.clear();
    mMediaPlayerService.clear();
    mBinder.clear();
    mServiceManager.clear();
}

void AVPlayer::init() {
    mLooper->registerHandler(this);
    mLooper->start(false /*runOnCallingThread*/, false /*canCallJava*/); // use default priority
}

void AVPlayer::prepare() {
    SL_LOGI("AVPlayer::prepare()");
    sp<AMessage> msg = new AMessage(kWhatPrepare, id());
    msg->post();
}

void AVPlayer::play() {
    SL_LOGI("AVPlayer::play()");
    sp<AMessage> msg = new AMessage(kWhatPlay, id());
    msg->post();
}

void AVPlayer::pause() {
    SL_LOGI("AVPlayer::prepare()");
    sp<AMessage> msg = new AMessage(kWhatPause, id());
    msg->post();
}

void AVPlayer::stop() {
    SL_LOGI("AVPlayer::stop()");
    sp<AMessage> msg = new AMessage(kWhatStop, id());
    msg->post();
}


//--------------------------------------------------
// AHandler implementation
void AVPlayer::onMessageReceived(const sp<AMessage> &msg) {
    switch (msg->what()) {
        case kWhatPrepare:
            onPrepare();
            break;
/*
        case kWhatNotif:
            onNotify(msg);
            break;
*/
        case kWhatPlay:
            onPlay();
            break;

        case kWhatPause:
            onPause();
            break;

        case kWhatStop:
            onStop();
            break;

        default:
            TRESPASS();
    }
}

//--------------------------------------------------
// Event handlers
void AVPlayer::onPrepare() {
    SL_LOGI("AVPlayer::onPrepare()");
    if (mPlayer != 0) {
        mPlayer->setAudioStreamType(mPlaybackParams.streamType);
        mPlayer->prepareAsync();
        mPlayerClient->blockUntilPlayerPrepared();
    }
    SL_LOGI("AVPlayer::onPrepare() done");
}

void AVPlayer::onPause() {
    SL_LOGI("AVPlayer::onPause()");
    if (mPlayer != 0) {
        mPlayer->pause();
    }
}

void AVPlayer::onPlay() {
    SL_LOGI("AVPlayer::onPlay()");
    if (mPlayer != 0) {
        mPlayer->start();
    }
}

void AVPlayer::onStop() {
    SL_LOGI("AVPlayer::onStop()");
    if (mPlayer != 0) {
        mPlayer->stop();
    }
}


} // namespace android
