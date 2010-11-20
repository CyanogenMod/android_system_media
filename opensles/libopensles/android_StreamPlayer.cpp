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

#define USE_LOG SLAndroidLogLevel_Verbose

#include "sles_allinclusive.h"

//----------------------------------------------------------------
void android_StreamPlayer_realize_lApObj(CAudioPlayer *ap) {
    SL_LOGV("android_StreamPlayer_realize_lApObj(%p)", ap);

    StreamPlayback_Parameters ap_params;
    ap_params.sessionId = ap->mSessionId;
    ap_params.streamType = ap->mStreamType;
    ap->mStreamPlayer = new android::StreamPlayer(&ap_params);

    ap->mStreamPlayer->setStream(ap->mDataSource.mLocator.mStreamer.streamOrigin);
}


void android_StreamPlayer_destroy(CAudioPlayer *ap) {
    SL_LOGV("android_StreamPlayer_destroy(%p)", ap);

    if (ap->mStreamPlayer != NULL) {
        delete ap->mStreamPlayer;
        ap->mStreamPlayer = NULL;
    }

}

//----------------------------------------------------------------

namespace android {

StreamPlayer::StreamPlayer(StreamPlayback_Parameters* params) :
    mMediaPlayer(0)
{
    SL_LOGV("StreamPlayer::StreamPlayer()");

    mMediaPlayer = new MediaPlayer();
    mMediaPlayer->setAudioStreamType(params->streamType);
    mMediaPlayer->setAudioSessionId(params->sessionId);

    // TODO create MediaPlayer event listener
}


StreamPlayer::~StreamPlayer() {
    SL_LOGV("StreamPlayer::~StreamPlayer()");

    if (mMediaPlayer != NULL) {
        mMediaPlayer->setListener(0);
        mMediaPlayer->stop();
        mMediaPlayer->disconnect();
        mMediaPlayer.clear();
    }
}

void StreamPlayer::setStream(SLuint32 streamOrigin) {
    SL_LOGV("StreamPlayer::setStream(%ld)", streamOrigin);

    int type;
    switch(streamOrigin) {
    case SL_ANDROID_STREAMORIGIN_FILE:
        type = MEDIA_PLAYER_STREAM_ORIGIN_FILE;
        break;
    case SL_ANDROID_STREAMORIGIN_TRANSPORTSTREAM:
        type = MEDIA_PLAYER_STREAM_ORIGIN_TRANSPORT_STREAM;
        break;
    default:
        type = MEDIA_PLAYER_STREAM_ORIGIN_INVALID;
        break;
    }

    if (mMediaPlayer != NULL) {
        // FIXME uncomment when new setDataSource() interface is finalized
        //mMediaPlayer->setDataSource(type);
    }
}

} // namespace android
