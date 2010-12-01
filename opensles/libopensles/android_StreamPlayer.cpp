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
void android_StreamPlayer_realize_l(CAudioPlayer *ap) {
    SL_LOGV("android_StreamPlayer_realize_l(%p)", ap);

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
            if (ap->mStreamPlayer != 0) {
                ap->mStreamPlayer->prepare();
            }
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
            // FIXME PRIORITY1 prepare should update the obj state
            //  for the moment test app sets state to PAUSED to prepare, then to PLAYING
            /*if (ap->mStreamPlayer != 0) {
                ap->mStreamPlayer->prepare();
            }*/
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


void android_StreamPlayer_registerCallback_l(CAudioPlayer *ap) {
    if (ap->mStreamPlayer != 0) {
        ap->mStreamPlayer->appRegisterCallback(ap->mAndroidBufferQueue.mCallback,
                ap->mAndroidBufferQueue.mContext, (const void*)&(ap->mAndroidBufferQueue.mItf));
    }
}


void android_StreamPlayer_enqueue_l(CAudioPlayer *ap,
        SLuint32 bufferId, SLuint32 length, SLAbufferQueueEvent event, void *pData) {
    //SL_LOGE("#################### android_StreamPlayer_enqueue_l");
    if (ap->mStreamPlayer != 0) {
        ap->mStreamPlayer->appEnqueue(bufferId, length, event, pData);
    }
}


void android_StreamPlayer_clear_l(CAudioPlayer *ap) {
    if (ap->mStreamPlayer != 0) {
        ap->mStreamPlayer->appClear();
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
StreamSourceAppProxy::StreamSourceAppProxy(
        slAndroidBufferQueueCallback callback, void *context, const void *caller) :
    mCallback(callback),
    mAppContext(context),
    mCaller(caller)
{
    SL_LOGV("StreamSourceAppProxy::StreamSourceAppProxy()");
}

StreamSourceAppProxy::~StreamSourceAppProxy() {
    SL_LOGV("StreamSourceAppProxy::~StreamSourceAppProxy()");
    mListener.clear();
    mBuffers.clear();
}

//--------------------------------------------------
// IStreamSource implementation
void StreamSourceAppProxy::setListener(const sp<IStreamListener> &listener) {
    Mutex::Autolock _l(mListenerLock);
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

    // FIXME PRIORITY1 needs to be called asynchronously, from AudioPlayer code after having
    //   obtained under lock the callback function pointer and context
    (*mCallback)((SLAndroidBufferQueueItf)mCaller,     /* SLAndroidBufferQueueItf self */
            mAppContext,  /* void *pContext */
            index,        /* SLuint32 bufferId */
            length,       /*  SLAint64 bufferLength */
            mem->pointer()/* void *pBufferDataLocation */
    );
}

void StreamSourceAppProxy::receivedFromAppCommand(IStreamListener::Command cmd) {
    Mutex::Autolock _l(mListenerLock);
    if (mListener != 0) {
        mListener->queueCommand(cmd);
    }
}

void StreamSourceAppProxy::receivedFromAppBuffer(size_t buffIndex, size_t buffLength) {
    Mutex::Autolock _l(mListenerLock);
    if (mListener != 0) {
        mListener->queueBuffer(buffIndex, buffLength);
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

void StreamPlayer::prepare() {
    // FIXME PRIORITY1 must be async
    SL_LOGE("StreamPlayer::prepare()");
    mPlayer = mMediaPlayerService->create(getpid(), mMPClient /*IMediaPlayerClient*/,
            mAppProxy /*IStreamSource*/, mPlaybackParams.sessionId);
    SL_LOGE("StreamPlayer::prepare() after mMediaPlayerService->create()");
    // FIXME PRIORITY1
    //mPlayer->setAudioStreamType(mPlaybackParams.streamType);
}

void StreamPlayer::stop() {
    // FIXME PRIORITY1 must be async
    Mutex::Autolock _l(mLock);
    if (mPlayer != 0) {
        mPlayer->stop();
    }
}

void StreamPlayer::pause() {
    // FIXME PRIORITY1 must be async
    Mutex::Autolock _l(mLock);
    if (mPlayer != 0) {
        mPlayer->pause();
    }
}

void StreamPlayer::play() {
    // FIXME PRIORITY1 must be async
    Mutex::Autolock _l(mLock);
    if (mPlayer != 0) {
        mPlayer->start();
    }
}

void StreamPlayer::appRegisterCallback(slAndroidBufferQueueCallback callback, void *context, const void *caller) {
    //SL_LOGE("#################### appRegisterCallback");

    Mutex::Autolock _l(mLock);

    mAppProxy = new StreamSourceAppProxy(callback, context, caller);
    mMPClient = new StreamMediaPlayerClient();
    CHECK(mAppProxy != 0);
}

void StreamPlayer::appEnqueue(SLuint32 bufferId, SLuint32 length, SLAbufferQueueEvent event,
        void *pData) {
    Mutex::Autolock _l(mLock);
    if (mAppProxy != 0) {
        if (event != SL_ANDROIDBUFFERQUEUE_EVENT_NONE) {
            if (event & SL_ANDROIDBUFFERQUEUE_EVENT_FLUSH) {
                mAppProxy->receivedFromAppCommand(IStreamListener::FLUSH);
            }
            if (event & SL_ANDROIDBUFFERQUEUE_EVENT_DISCONTINUITY) {
                mAppProxy->receivedFromAppCommand(IStreamListener::DISCONTINUITY);
            }
            if (event & SL_ANDROIDBUFFERQUEUE_EVENT_EOS) {
                mAppProxy->receivedFromAppCommand(IStreamListener::EOS);
            }
        }
        if (length > 0) {
            // FIXME PRIORITY1 verify given length isn't bigger than declared length in app callback
            mAppProxy->receivedFromAppBuffer((size_t)bufferId, (size_t)length);
        }
    }
}

void StreamPlayer::appClear() {
    Mutex::Autolock _l(mLock);
    if (mAppProxy != 0) {
        // FIXME PRIORITY1 implement
        SL_LOGE("[ FIXME implement StreamPlayer::appClear() ]");
    }
}

} // namespace android
