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
LocAVPlayer::LocAVPlayer(AudioPlayback_Parameters* params) : AVPlayer(params),
        mDataLocatorType(kDataLocatorNone)
{
    SL_LOGI("LocAVPlayer::LocAVPlayer()");

}


LocAVPlayer::~LocAVPlayer() {
    SL_LOGI("LocAVPlayer::~LocAVPlayer()");

    resetDataLocator();
}


//--------------------------------------------------
// Event handlers
void LocAVPlayer::onPrepare() {
    SL_LOGI("LocAVPlayer::onPrepare()");
    Mutex::Autolock _l(mLock);
    switch (mDataLocatorType) {
    case kDataLocatorUri:
        mPlayer = mMediaPlayerService->create(getpid(), mPlayerClient /*IMediaPlayerClient*/,
                mDataLocator.uri /*url*/, NULL /*headers*/, mPlaybackParams.sessionId);
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
    AVPlayer::onPrepare();
    SL_LOGI("LocAVPlayer::onPrepare() done");
}


//--------------------------------------------------
/*
 * post-condition: mDataLocatorType == kDataLocatorNone
 *
 */
void LocAVPlayer::resetDataLocator() {
    if (kDataLocatorUri == mDataLocatorType) {
        if (NULL != mDataLocator.uri) {
            free(mDataLocator.uri);
            mDataLocator.uri = NULL;
        }
    }
    mDataLocatorType = kDataLocatorNone;
}


void LocAVPlayer::setDataSource(const char *uri) {
    resetDataLocator();

    // FIXME: a copy of the URI has already been made and is guaranteed to exist
    // as long as the SLES/OMXAL object exists, so the copy here is not necessary
    size_t len = strlen((const char *) uri);
    char* newUri = (char*) malloc(len + 1);
    if (NULL == newUri) {
        // mem issue
        SL_LOGE("LocAVPlayer::setDataSource: not enough memory to allocator URI string");
        return;
    }
    memcpy(newUri, uri, len + 1);
    mDataLocator.uri = newUri;

    mDataLocatorType = kDataLocatorUri;
}


void LocAVPlayer::setDataSource(const int fd, const int64_t offset, const int64_t length) {
    resetDataLocator();

    mDataLocator.fdi.fd = fd;

    struct stat sb;
    int ret = fstat(fd, &sb);
    if (ret != 0) {
        SL_LOGE("LocAVPlayer::setDataSource: fstat(%d) failed: %d, %s", fd, ret, strerror(errno));
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

} // namespace android
