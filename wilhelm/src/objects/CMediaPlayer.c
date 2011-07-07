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

#include "sles_allinclusive.h"

#ifdef ANDROID
#include <gui/SurfaceTextureClient.h>
#include "android/android_GenericMediaPlayer.h"
using namespace android;
#endif


XAresult CMediaPlayer_Realize(void *self, XAboolean async)
{
    XAresult result = XA_RESULT_SUCCESS;

#ifdef ANDROID
    CMediaPlayer *thiz = (CMediaPlayer *) self;

    // realize player
    result = android_Player_realize(thiz, async);
    if (XA_RESULT_SUCCESS == result) {

        // if there is a video sink
        if (XA_DATALOCATOR_NATIVEDISPLAY ==
                thiz->mImageVideoSink.mLocator.mLocatorType) {
            ANativeWindow *nativeWindow = (ANativeWindow *)
                    thiz->mImageVideoSink.mLocator.mNativeDisplay.hWindow;
            // we already verified earlier that hWindow is non-NULL
            assert(nativeWindow != NULL);
            int err;
            int value;
            // this could crash if app passes in a bad parameter, but that's OK
            err = (*nativeWindow->query)(nativeWindow, NATIVE_WINDOW_CONCRETE_TYPE, &value);
            if (0 != err) {
                SL_LOGE("Query NATIVE_WINDOW_CONCRETE_TYPE on ANativeWindow * %p failed; "
                        "errno %d", nativeWindow, err);
            } else {
                // thiz->mAVPlayer != 0 is implied by success after android_Player_realize()
                switch (value) {
                case NATIVE_WINDOW_SURFACE: {                // Surface
                    sp<Surface> nativeSurface(static_cast<Surface *>(nativeWindow));
                    result = android_Player_setVideoSurface(thiz->mAVPlayer, nativeSurface);
                    } break;
                case NATIVE_WINDOW_SURFACE_TEXTURE_CLIENT: { // SurfaceTextureClient
                    sp<SurfaceTextureClient> surfaceTextureClient(
                            static_cast<SurfaceTextureClient *>(nativeWindow));
                    sp<ISurfaceTexture> nativeSurfaceTexture(
                            surfaceTextureClient->getISurfaceTexture());
                    result = android_Player_setVideoSurfaceTexture(thiz->mAVPlayer,
                            nativeSurfaceTexture);
                    } break;
                case NATIVE_WINDOW_FRAMEBUFFER:              // FramebufferNativeWindow
                    // fall through
                default:
                    SL_LOGE("ANativeWindow * %p has unknown or unsupported concrete type %d",
                            nativeWindow, value);
                    break;
                }
            }
        }
    }
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
    CMediaPlayer *thiz = (CMediaPlayer *) self;
    freeDataLocatorFormat(&thiz->mDataSource);
    freeDataLocatorFormat(&thiz->mBankSource);
    freeDataLocatorFormat(&thiz->mAudioSink);
    freeDataLocatorFormat(&thiz->mImageVideoSink);
    freeDataLocatorFormat(&thiz->mVibraSink);
    freeDataLocatorFormat(&thiz->mLEDArraySink);
#ifdef ANDROID
    android_Player_destroy(thiz);
#endif
}


predestroy_t CMediaPlayer_PreDestroy(void *self)
{
    return predestroy_ok;
}
