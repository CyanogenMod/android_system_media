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

#include <assert.h>
#include <jni.h>
#include <pthread.h>
#include <string.h>
#define LOG_TAG "NativeMedia"
#include <utils/Log.h>
//FIXME shouldn't be needed here, but needed for declaration of XA_IID_ANDROIDBUFFERQUEUE
#include "SLES/OpenSLES.h"
#include "SLES/OpenSLES_Android.h"
#include "OMXAL/OpenMAXAL.h"
#include "OMXAL/OpenMAXAL_Android.h"
#include <android/native_window_jni.h>

// engine interfaces
static XAObjectItf engineObject = NULL;
static XAEngineItf engineEngine;

// output mix interfaces
static XAObjectItf outputMixObject = NULL;

// streaming media player interfaces
static XAObjectItf             playerObj = NULL;
static XAPlayItf               playerPlayItf = NULL;
static XAAndroidBufferQueueItf playerBQItf = NULL;

// cached surface where the video display happens
static ANativeWindow* theNativeWindow;

// handle of the file to play
FILE *file;

// AndroidBufferQueueItf callback for an audio player
XAresult AndroidBufferQueueCallback(
        XAAndroidBufferQueueItf caller,
        void *pContext,
        XAuint32 bufferId,
        XAuint32 bufferLength,
        void *pBufferDataLocation)

{
    size_t nbRead = fread(pBufferDataLocation, 1, bufferLength, file);

    XAAbufferQueueEvent event = XA_ANDROIDBUFFERQUEUE_EVENT_NONE;
    if (nbRead <= 0) {
        event = XA_ANDROIDBUFFERQUEUE_EVENT_EOS;
    } else {
        event = XA_ANDROIDBUFFERQUEUE_EVENT_NONE; // no event to report
    }

    // enqueue the data right-away because in this example we're reading from a file, so we
    // can afford to do that. When streaming from the network, we would write from our cache
    // to this queue.
    // last param is NULL because we've already written the data in the buffer queue
    (*caller)->Enqueue(caller, bufferId, nbRead, event, NULL);

    return XA_RESULT_SUCCESS;
}


// create the engine and output mix objects
void Java_com_example_nativemedia_NativeMedia_createEngine(JNIEnv* env, jclass clazz)
{
    XAresult res;

    // create engine
    res = xaCreateEngine(&engineObject, 0, NULL, 0, NULL, NULL);
    assert(XA_RESULT_SUCCESS == res);

    // realize the engine
    res = (*engineObject)->Realize(engineObject, XA_BOOLEAN_FALSE);
    assert(XA_RESULT_SUCCESS == res);

    // get the engine interface, which is needed in order to create other objects
    res = (*engineObject)->GetInterface(engineObject, XA_IID_ENGINE, &engineEngine);
    assert(XA_RESULT_SUCCESS == res);

    // create output mix
    res = (*engineEngine)->CreateOutputMix(engineEngine, &outputMixObject, 0, NULL, NULL);
    assert(XA_RESULT_SUCCESS == res);

    // realize the output mix
    res = (*outputMixObject)->Realize(outputMixObject, XA_BOOLEAN_FALSE);
    assert(XA_RESULT_SUCCESS == res);

}


// create streaming media player
jboolean Java_com_example_nativemedia_NativeMedia_createStreamingMediaPlayer(JNIEnv* env,
        jclass clazz, jstring filename)
{
    XAresult res;

    // convert Java string to UTF-8
    const char *utf8 = (*env)->GetStringUTFChars(env, filename, NULL);
    assert(NULL != utf8);

    // open the file to play
    file = fopen(utf8, "rb");
    if (file == NULL) {
        LOGE("Failed to open %s", utf8);
        return JNI_FALSE;
    }

    // configure data source
    XADataLocator_AndroidBufferQueue loc_abq = {XA_DATALOCATOR_ANDROIDBUFFERQUEUE,
            0, 0 /* number of buffers and size of queue are ignored for now, subject to change */};
    XADataFormat_MIME format_mime = {XA_DATAFORMAT_MIME, NULL, XA_CONTAINERTYPE_UNSPECIFIED};
    XADataSource dataSrc = {&loc_abq, &format_mime};

    // configure audio sink
    XADataLocator_OutputMix loc_outmix = {XA_DATALOCATOR_OUTPUTMIX, outputMixObject};
    XADataSink audioSnk = {&loc_outmix, NULL};

    // configure image video sink
    XADataLocator_NativeDisplay loc_nd = {
            XA_DATALOCATOR_NATIVEDISPLAY /* locatorType */,
            // currently the video sink only works on ANativeWindow created from a Surface
            (void*)theNativeWindow       /* hWindow */,
            // ignored here
            0                            /* hDisplay */};
    XADataSink imageVideoSink = {&loc_nd, NULL};

    // declare interfaces to use
    XAboolean     required[2] = {XA_BOOLEAN_TRUE, XA_BOOLEAN_TRUE};
    XAInterfaceID iidArray[2] = {XA_IID_PLAY,     XA_IID_ANDROIDBUFFERQUEUE};

    // create media player
    res = (*engineEngine)->CreateMediaPlayer(engineEngine, &playerObj, &dataSrc,
            NULL, &audioSnk, &imageVideoSink, NULL, NULL,
            2        /*XAuint32 numInterfaces*/,
            iidArray /*const XAInterfaceID *pInterfaceIds*/,
            required /*const XAboolean *pInterfaceRequired*/);
    assert(XA_RESULT_SUCCESS == res);

    // release the Java string and UTF-8
    (*env)->ReleaseStringUTFChars(env, filename, utf8);

    // realize the player
    res = (*playerObj)->Realize(playerObj, XA_BOOLEAN_FALSE);
    assert(XA_RESULT_SUCCESS == res);

    // get the play interface
    res = (*playerObj)->GetInterface(playerObj, XA_IID_PLAY, &playerPlayItf);
    assert(XA_RESULT_SUCCESS == res);

    // get the Android buffer queue interface
    res = (*playerObj)->GetInterface(playerObj, XA_IID_ANDROIDBUFFERQUEUE, &playerBQItf);
    assert(XA_RESULT_SUCCESS == res);

    // register the callback from which OpenMAX AL can retrieve the data to play
    res = (*playerBQItf)->RegisterCallback(playerBQItf, AndroidBufferQueueCallback, &playerBQItf);

    // prepare the player
    res = (*playerPlayItf)->SetPlayState(playerPlayItf, XA_PLAYSTATE_PAUSED);
    assert(XA_RESULT_SUCCESS == res);

    res = (*playerPlayItf)->SetPlayState(playerPlayItf, XA_PLAYSTATE_PLAYING);
        assert(XA_RESULT_SUCCESS == res);

    return JNI_TRUE;
}


// set the playing state for the streaming media player
void Java_com_example_nativemedia_NativeMedia_setPlayingStreamingMediaPlayer(JNIEnv* env,
        jclass clazz, jboolean isPlaying)
{
    XAresult res;

    // make sure the streaming media player was created
    if (NULL != playerPlayItf) {

        // set the player's state
        res = (*playerPlayItf)->SetPlayState(playerPlayItf, isPlaying ?
            XA_PLAYSTATE_PLAYING : XA_PLAYSTATE_PAUSED);
        assert(XA_RESULT_SUCCESS == res);

    }

}


// shut down the native media system
void Java_com_example_nativemedia_NativeMedia_shutdown(JNIEnv* env, jclass clazz)
{
    // destroy streaming media player object, and invalidate all associated interfaces
    if (playerObj != NULL) {
        (*playerObj)->Destroy(playerObj);
        playerObj = NULL;
        playerPlayItf = NULL;
        playerBQItf = NULL;
    }

    // destroy output mix object, and invalidate all associated interfaces
    if (outputMixObject != NULL) {
        (*outputMixObject)->Destroy(outputMixObject);
        outputMixObject = NULL;
    }

    // destroy engine object, and invalidate all associated interfaces
    if (engineObject != NULL) {
        (*engineObject)->Destroy(engineObject);
        engineObject = NULL;
        engineEngine = NULL;
    }

    // close the file
    if (file != NULL) {
        fclose(file);
    }

    // make sure we don't leak native windows
    ANativeWindow_release(theNativeWindow);
}


// set the surface
void Java_com_example_nativemedia_NativeMedia_setSurface(JNIEnv *env, jclass clazz, jobject surface)
{
    // obtain a native window from a Java surface
    theNativeWindow = ANativeWindow_fromSurface(env, surface);
}
