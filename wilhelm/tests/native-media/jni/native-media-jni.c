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

#include "OMXAL/OpenMAXAL.h"
#include "OMXAL/OpenMAXAL_Android.h"

#include <android/native_window_jni.h>

// define as 1 if ANativeWindow * is not supported as a video sink
#define NO_NATIVE_WINDOW 1

// engine interfaces
static XAObjectItf engineObject = NULL;
static XAEngineItf engineEngine;

// output mix interfaces
static XAObjectItf outputMixObject = NULL;

// streaming media player interfaces
static XAObjectItf             playerObj = NULL;
static XAPlayItf               playerPlayItf = NULL;
static XAAndroidBufferQueueItf playerBQItf = NULL;
// number of required interfaces for the MediaPlayer creation
#define NB_MAXAL_INTERFACES 2 // XAAndroidBufferQueueItf and XAPlayItf

// cached surface where the video display happens
#if NO_NATIVE_WINDOW
static jobject theSurfaceOrSurfaceTexture;
#else
static ANativeWindow* theNativeWindow;
#endif

// number of buffers in our buffer queue
#define NB_BUFFERS 16
// we're streaming MPEG-2 transport stream data, operate on transport stream block size
#define MPEG2_TS_BLOCK_SIZE 188
// determines how much memory we're dedicating to memory caching
#define BUFFER_SIZE 20*MPEG2_TS_BLOCK_SIZE // 20 is an arbitrary number chosen here

// where we cache in memory the data to play
char dataCache[BUFFER_SIZE * NB_BUFFERS];
// handle of the file to play
FILE *file;

// AndroidBufferQueueItf callback for an audio player
XAresult AndroidBufferQueueCallback(
        XAAndroidBufferQueueItf caller,
        void *pContext,                /* input */
        const void *pBufferData,       /* input */
        XAuint32 dataSize,             /* input */
        XAuint32 dataUsed,             /* input */
        const XAAndroidBufferItem *pItems,/* input */
        XAuint32 itemsLength           /* input */)
{
    // assert(BUFFER_SIZE <= dataSize);
    size_t nbRead = fread((void*)pBufferData, 1, BUFFER_SIZE, file);
    if (nbRead > 0) {
        (*caller)->Enqueue(caller,
                pBufferData /*pData*/,
                nbRead /*dataLength*/,
                NULL /*pMsg*/,
                0 /*msgLength*/);
    } else {
        // signal EOS
        XAAndroidBufferItem msgEos;
        msgEos.itemKey = XA_ANDROID_ITEMKEY_EOS;
        msgEos.itemSize = 0;
        // EOS message has no parameters, so the total size of the message is the size of the key
        //   plus the size if itemSize, both XAuint32
        (*caller)->Enqueue(caller, NULL /*pData*/, 0 /*dataLength*/,
                        &msgEos /*pMsg*/,
                        sizeof(XAuint32)*2 /*msgLength*/);
    }

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
    XADataLocator_AndroidBufferQueue loc_abq = { XA_DATALOCATOR_ANDROIDBUFFERQUEUE, NB_BUFFERS };
    XADataFormat_MIME format_mime = {
            XA_DATAFORMAT_MIME, (XAchar *)"video/mp2ts", XA_CONTAINERTYPE_MPEG_TS };
    XADataSource dataSrc = {&loc_abq, &format_mime};

    // configure audio sink
    XADataLocator_OutputMix loc_outmix = { XA_DATALOCATOR_OUTPUTMIX, outputMixObject };
    XADataSink audioSnk = { &loc_outmix, NULL };

    // configure image video sink
    XADataLocator_NativeDisplay loc_nd = {
            XA_DATALOCATOR_NATIVEDISPLAY,        // locatorType
#if NO_NATIVE_WINDOW
            (void *) theSurfaceOrSurfaceTexture, // jobject
            (void *) env                         // JNIEnv *env
#else
            // later the video sink can be an ANativeWindow created from a Surface or SurfaceTexture
            (void*)theNativeWindow,              // hWindow
            // must be NULL
            NULL                                 // hDisplay
#endif
    };
    XADataSink imageVideoSink = {&loc_nd, NULL};

    // declare interfaces to use
    XAboolean     required[NB_MAXAL_INTERFACES] = {XA_BOOLEAN_TRUE, XA_BOOLEAN_TRUE};
    XAInterfaceID iidArray[NB_MAXAL_INTERFACES] = {XA_IID_PLAY,     XA_IID_ANDROIDBUFFERQUEUE};

    // create media player
    res = (*engineEngine)->CreateMediaPlayer(engineEngine, &playerObj, &dataSrc,
            NULL, &audioSnk, &imageVideoSink, NULL, NULL,
            NB_MAXAL_INTERFACES /*XAuint32 numInterfaces*/,
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
    res = (*playerBQItf)->RegisterCallback(playerBQItf, AndroidBufferQueueCallback, NULL);

    /* Fill our cache */
    if (fread(dataCache, 1, BUFFER_SIZE * NB_BUFFERS, file) <= 0) {
        LOGE("Error filling cache, exiting\n");
        return JNI_FALSE;
    }
    /* Enqueue the content of our cache before starting to play,
       we don't want to starve the player */
    int i;
    for (i=0 ; i < NB_BUFFERS ; i++) {
        res = (*playerBQItf)->Enqueue(playerBQItf, dataCache + i*BUFFER_SIZE, BUFFER_SIZE, NULL, 0);
        assert(XA_RESULT_SUCCESS == res);
    }


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

#if !NO_NATIVE_WINDOW
    // make sure we don't leak native windows
    if (theNativeWindow != NULL) {
        ANativeWindow_release(theNativeWindow);
    }
#endif
}


// set the surface
void Java_com_example_nativemedia_NativeMedia_setSurface(JNIEnv *env, jclass clazz, jobject surface)
{
#if NO_NATIVE_WINDOW
    theSurfaceOrSurfaceTexture = surface;
#else
    // obtain a native window from a Java surface
    theNativeWindow = ANativeWindow_fromSurface(env, surface);
#endif
}


// set the surface texture
void Java_com_example_nativemedia_NativeMedia_setSurfaceTexture(JNIEnv *env, jclass clazz,
        jobject surfaceTexture)
{
#if NO_NATIVE_WINDOW
    theSurfaceOrSurfaceTexture = surfaceTexture;
#else
    // obtain a native window from a Java surface texture
    theNativeWindow = ANativeWindow_fromSurfaceTexture(env, surfaceTexture);
#endif
}
