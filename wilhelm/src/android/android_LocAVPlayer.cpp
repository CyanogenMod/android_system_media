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


namespace android {

//--------------------------------------------------------------------------------------------------
LocAVPlayer::LocAVPlayer(AudioPlayback_Parameters* params, bool hasVideo) :
        GenericMediaPlayer(params, hasVideo)
{
    SL_LOGD("LocAVPlayer::LocAVPlayer()");

}


LocAVPlayer::~LocAVPlayer() {
    SL_LOGD("LocAVPlayer::~LocAVPlayer()");

}


//--------------------------------------------------
// Event handlers
void LocAVPlayer::onPrepare() {
    SL_LOGD("LocAVPlayer::onPrepare()");
    switch (mDataLocatorType) {
    case kDataLocatorUri:
        mPlayer = mMediaPlayerService->create(getpid(), mPlayerClient /*IMediaPlayerClient*/,
                mDataLocator.uriRef /*url*/, NULL /*headers*/, mPlaybackParams.sessionId);
        break;
    case kDataLocatorFd:
        mPlayer = mMediaPlayerService->create(getpid(), mPlayerClient /*IMediaPlayerClient*/,
                mDataLocator.fdi.fd, mDataLocator.fdi.offset, mDataLocator.fdi.length,
                mPlaybackParams.sessionId);
        break;
    case kDataLocatorNone:
        SL_LOGE("no data locator for MediaPlayer object");
        break;
    default:
        SL_LOGE("unsupported data locator %d for MediaPlayer object", mDataLocatorType);
        break;
    }
    // blocks until mPlayer is prepared
    GenericMediaPlayer::onPrepare();
    SL_LOGD("LocAVPlayer::onPrepare() done");
}

} // namespace android
