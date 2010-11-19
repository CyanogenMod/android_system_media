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

StreamPlayer::StreamPlayer(StreamPlayback_Parameters* params)
{
    SL_LOGV("StreamPlayer::StreamPlayer()");

}


StreamPlayer::~StreamPlayer() {
    SL_LOGV("StreamPlayer::~StreamPlayer()");

}


} // namespace android
