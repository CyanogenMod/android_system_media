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

/** \file CMediaPlayer.c MediaPlayer class */

#ifdef ANDROID
// FIXME JNI should not be part of the API we can avoid it
#include <jni.h>

#include <binder/ProcessState.h>

#include <media/IStreamSource.h>
#include <media/mediaplayer.h>
#include <media/stagefright/foundation/ADebug.h>

#include <binder/IServiceManager.h>
#include <media/IMediaPlayerService.h>
#include <surfaceflinger/ISurfaceComposer.h>
#include <surfaceflinger/SurfaceComposerClient.h>

#include <fcntl.h>
#endif

#include "sles_allinclusive.h"

#ifdef ANDROID
using namespace android;

#endif


XAresult CMediaPlayer_Realize(void *self, XAboolean async)
{
    CMediaPlayer *thiz = (CMediaPlayer *) self;

    SLresult result = XA_RESULT_SUCCESS;

#ifdef ANDROID
    result = android_Player_realize(thiz, async);
#endif

    // FIXME add support for display surface retrieval
#if 0
    assert(XA_DATALOCATOR_URI == thiz->mDataSource.mLocator.mLocatorType);
    // assert(XA_FORMAT_NULL == this->mDataSource.mFormat.mFormatType);
    assert(XA_DATALOCATOR_NATIVEDISPLAY == thiz->mImageVideoSink.mLocator.mLocatorType);
    // assert(XA_FORMAT_NULL == this->mImageVideoSink.mFormat.mFormatType);
    // FIXME ignore the audio sink
#ifdef ANDROID
    int fd = open((const char *) thiz->mDataSource.mLocator.mURI.URI, O_RDONLY);
    if (0 >= fd) {
        return err_to_result(errno);
    }

    JNIEnv *env = (JNIEnv *) thiz->mImageVideoSink.mLocator.mNativeDisplay.hDisplay;
    jobject jsurface = (jobject) thiz->mImageVideoSink.mLocator.mNativeDisplay.hWindow;
    jclass cls = env->GetObjectClass(jsurface);
    jfieldID fid = env->GetFieldID(cls, "mNativeSurface", "I");
    jint mNativeSurface = env->GetIntField(jsurface, fid);

    sp<Surface> surface = (Surface *) mNativeSurface;

    sp<IServiceManager> sm = defaultServiceManager();
    sp<IBinder> binder = sm->getService(String16("media.player"));
    sp<IMediaPlayerService> service = interface_cast<IMediaPlayerService>(binder);

    CHECK(service.get() != NULL);

    sp<MyClient> client = new MyClient;

    sp<IMediaPlayer> player = service->create(getpid(), client, new MyStreamSource(fd), 0);

    if (player != NULL) {
        thiz->mPlayer = player;
        player->setVideoSurface(surface);
    }
#endif
#endif
    return result;
}


XAresult CMediaPlayer_Resume(void *self, XAboolean async)
{
    return XA_RESULT_SUCCESS;
}


/** \brief Hook called by Object::Destroy when a media player is destroyed */

void CMediaPlayer_Destroy(void *self)
{
    CMediaPlayer *this = (CMediaPlayer *) self;
    freeDataLocatorFormat(&this->mDataSource);
    freeDataLocatorFormat(&this->mBankSource);
    freeDataLocatorFormat(&this->mAudioSink);
    freeDataLocatorFormat(&this->mImageVideoSink);
    freeDataLocatorFormat(&this->mVibraSink);
    freeDataLocatorFormat(&this->mLEDArraySink);
}


bool CMediaPlayer_PreDestroy(void *self)
{
    return true;
}
