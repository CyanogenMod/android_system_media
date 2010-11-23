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
#include <media/IMediaPlayerService.h>

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

    ap->mStreamPlayer.clear();
}


void android_StreamPlayer_setPlayState(CAudioPlayer *ap, SLuint32 playState,
        AndroidObject_state objState)
{
    switch (playState) {
    case SL_PLAYSTATE_STOPPED: {
        SL_LOGV("setting StreamPlayer to SL_PLAYSTATE_STOPPED");
        if (ap->mStreamPlayer != 0) {
            ap->mStreamPlayer->stop();
        }
    } break;
    case SL_PLAYSTATE_PAUSED: {
        SL_LOGV("setting StreamPlayer to SL_PLAYSTATE_PAUSED");
        switch(objState) {
        case(ANDROID_UNINITIALIZED):
            // FIXME prepare?
        break;
        case(ANDROID_PREPARING):
            break;
        case(ANDROID_READY):
            if (ap->mStreamPlayer != 0) {
                ap->mStreamPlayer->pause();
            }
        break;
        default:
            break;
        }
    } break;
    case SL_PLAYSTATE_PLAYING: {
        SL_LOGV("setting StreamPlayer to SL_PLAYSTATE_PLAYING");
        switch(objState) {
        case(ANDROID_UNINITIALIZED):
            // FIXME prepare?
        // fall through
        case(ANDROID_PREPARING):
        case(ANDROID_READY):
            if (ap->mStreamPlayer != 0) {
                ap->mStreamPlayer->play();
            }
        break;
        default:
            break;
        }
    } break;

    default:
        // checked by caller, should not happen
        break;
    }
}


void android_StreamPlayer_registerCallback_lApObj(CAudioPlayer *ap) {
    if (ap->mStreamPlayer != 0) {
        ap->mStreamPlayer->useCallbackToApp(ap->mAndroidStreamSource.mCallback,
                ap->mAndroidStreamSource.mContext);
    }
}


namespace android {

//--------------------------------------------------------------------------------------------------
StreamMediaPlayerClient::StreamMediaPlayerClient() {

}

StreamMediaPlayerClient::~StreamMediaPlayerClient() {

}

//--------------------------------------------------
// IMediaPlayerClient implementation
void StreamMediaPlayerClient::notify(int msg, int ext1, int ext2) {
    SL_LOGV("StreamMediaPlayerClient::notify(msg=%d, ext1=%d, ext2=%d)", msg, ext1, ext2);
}

//--------------------------------------------------------------------------------------------------
StreamSourceAppProxy::StreamSourceAppProxy(slAndroidStreamSourceCallback callback, void *context) :
    mCallback(callback),
    mAppContext(context)
{
    SL_LOGV("StreamSourceAppProxy::StreamSourceAppProxy()");
}

StreamSourceAppProxy::~StreamSourceAppProxy() {
    SL_LOGV("StreamSourceAppProxy::~StreamSourceAppProxy()");
}

//--------------------------------------------------
// IStreamSource implementation
void StreamSourceAppProxy::setListener(const sp<IStreamListener> &listener) {
    mListener = listener;
}

void StreamSourceAppProxy::setBuffers(const Vector<sp<IMemory> > &buffers) {
    mBuffers = buffers;
}

void StreamSourceAppProxy::onBufferAvailable(size_t index) {
    LOGI("onBufferAvailable %d", index);

    CHECK_LT(index, mBuffers.size());
    sp<IMemory> mem = mBuffers.itemAt(index);

    SLAint64 length = (SLAint64) mem->size();
    SLAstreamEvent event = SL_ANDROID_STREAMEVENT_NONE;
    (*mCallback)(NULL /* FIXME needs SLAndroidStreamSourceItf */, mAppContext, &length, &event,
            mem->pointer());

    if (event != SL_ANDROID_STREAMEVENT_NONE) {
        if (event & SL_ANDROID_STREAMEVENT_FLUSH) {
            mListener->queueCommand(IStreamListener::FLUSH);
        }
        if (event & SL_ANDROID_STREAMEVENT_DISCONTINUITY) {
            mListener->queueCommand(IStreamListener::DISCONTINUITY);
        }
        if (event & SL_ANDROID_STREAMEVENT_EOS) {
            mListener->queueCommand(IStreamListener::EOS);
        }
    }
    if (length <= 0) {
        mListener->queueCommand(IStreamListener::EOS);
    } else {
        mListener->queueBuffer(index, length);
    }
}

//--------------------------------------------------------------------------------------------------
StreamPlayer::StreamPlayer(StreamPlayback_Parameters* params) :
    mAppProxy(0),
    mMPClient(0),
    mPlayer(0)
{
    SL_LOGV("StreamPlayer::StreamPlayer()");

    mPlaybackParams = *params;

    mServiceManager = defaultServiceManager();
    mBinder = mServiceManager->getService(String16("media.player"));
    mMediaPlayerService = interface_cast<IMediaPlayerService>(mBinder);

    CHECK(mMediaPlayerService.get() != NULL);
}


StreamPlayer::~StreamPlayer() {
    SL_LOGV("StreamPlayer::~StreamPlayer()");

    mAppProxy.clear();
    mMPClient.clear();

    mPlayer.clear();
    mMediaPlayerService.clear();
    mBinder.clear();
    mServiceManager.clear();
}

void StreamPlayer::stop() {
    Mutex::Autolock _l(mLock);
    if (mPlayer != 0) {
        mPlayer->stop();
    }
}

void StreamPlayer::pause() {
    Mutex::Autolock _l(mLock);
    if (mPlayer != 0) {
        mPlayer->pause();
    }
}

void StreamPlayer::play() {
    Mutex::Autolock _l(mLock);
    if (mPlayer != 0) {
        mPlayer->start();
    }
}

void StreamPlayer::useCallbackToApp(slAndroidStreamSourceCallback callback, void *context) {
    Mutex::Autolock _l(mLock);

    mAppProxy = new StreamSourceAppProxy(callback, context);
    mMPClient = new StreamMediaPlayerClient();
    CHECK(mAppProxy != NULL);
    mPlayer = mMediaPlayerService->create(getpid(), mMPClient /*IMediaPlayerClient*/,
            mAppProxy /*IStreamSource*/, mPlaybackParams.sessionId);
    mPlayer->setAudioStreamType(mPlaybackParams.streamType);
}

} // namespace android
