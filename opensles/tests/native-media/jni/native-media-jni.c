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

// engine interfaces
static XAObjectItf engineObject = NULL;
static XAEngineItf engineEngine;

// output mix interfaces
static XAObjectItf outputMixObject = NULL;

// streaming media player interfaces
static XAObjectItf streamingPlayerObject = NULL;
static XAPlayItf streamingPlayerPlay;

// cached surface
static jobject theSurface;
static JNIEnv *theEnv;

// create the engine and output mix objects
void Java_com_example_nativemedia_NativeMedia_createEngine(JNIEnv* env, jclass clazz)
{
    XAresult result;

    // create engine
    result = xaCreateEngine(&engineObject, 0, NULL, 0, NULL, NULL);
    assert(XA_RESULT_SUCCESS == result);

    // realize the engine
    result = (*engineObject)->Realize(engineObject, XA_BOOLEAN_FALSE);
    assert(XA_RESULT_SUCCESS == result);

    // get the engine interface, which is needed in order to create other objects
    result = (*engineObject)->GetInterface(engineObject, XA_IID_ENGINE, &engineEngine);
    assert(XA_RESULT_SUCCESS == result);

    // create output mix
    result = (*engineEngine)->CreateOutputMix(engineEngine, &outputMixObject, 0, NULL, NULL);
    assert(XA_RESULT_SUCCESS == result);

    // realize the output mix
    result = (*outputMixObject)->Realize(outputMixObject, XA_BOOLEAN_FALSE);
    assert(XA_RESULT_SUCCESS == result);

}


// create streaming media player
jboolean Java_com_example_nativemedia_NativeMedia_createStreamingMediaPlayer(JNIEnv* env,
        jclass clazz, jstring filename)
{
    XAresult result;

    // convert Java string to UTF-8
    const char *utf8 = (*env)->GetStringUTFChars(env, filename, NULL);
    assert(NULL != utf8);

    // configure audio source
    XADataLocator_URI loc_uri = {XA_DATALOCATOR_URI, (XAchar *) utf8};
    XADataFormat_MIME format_mime = {XA_DATAFORMAT_MIME, NULL, XA_CONTAINERTYPE_UNSPECIFIED};
    XADataSource dataSrc = {&loc_uri, &format_mime};

    // configure audio sink
    XADataLocator_OutputMix loc_outmix = {XA_DATALOCATOR_OUTPUTMIX, outputMixObject};
    XADataSink audioSnk = {&loc_outmix, NULL};

    // configure image video sink
    XADataLocator_NativeDisplay loc_nd = {XA_DATALOCATOR_NATIVEDISPLAY, theSurface, theEnv};
    XADataSink imageVideoSink = {&loc_nd, NULL};

    // create media player
    result = (*engineEngine)->CreateMediaPlayer(engineEngine, &streamingPlayerObject, &dataSrc,
            NULL, &audioSnk, &imageVideoSink, NULL, NULL, 0, NULL, NULL);
    assert(XA_RESULT_SUCCESS == result);

    // release the Java string and UTF-8
    (*env)->ReleaseStringUTFChars(env, filename, utf8);

    // realize the player
    result = (*streamingPlayerObject)->Realize(streamingPlayerObject, XA_BOOLEAN_FALSE);
    assert(XA_RESULT_SUCCESS == result);

    // get the play interface
    result = (*streamingPlayerObject)->GetInterface(streamingPlayerObject, XA_IID_PLAY,
            &streamingPlayerPlay);
    assert(XA_RESULT_SUCCESS == result);

    return JNI_TRUE;
}


// set the playing state for the streaming media player
void Java_com_example_nativemedia_NativeMedia_setPlayingStreamingMediaPlayer(JNIEnv* env,
        jclass clazz, jboolean isPlaying)
{
    XAresult result;

    // make sure the streaming media player was created
    if (NULL != streamingPlayerPlay) {

        // set the player's state
        result = (*streamingPlayerPlay)->SetPlayState(streamingPlayerPlay, isPlaying ?
            XA_PLAYSTATE_PLAYING : XA_PLAYSTATE_PAUSED);
        assert(XA_RESULT_SUCCESS == result);

    }

}


// shut down the native media system
void Java_com_example_nativemedia_NativeMedia_shutdown(JNIEnv* env, jclass clazz)
{

    // destroy streaming media player object, and invalidate all associated interfaces
    if (streamingPlayerObject != NULL) {
        (*streamingPlayerObject)->Destroy(streamingPlayerObject);
        streamingPlayerObject = NULL;
        streamingPlayerPlay = NULL;
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

}


// set the surface
void Java_com_example_nativemedia_NativeMedia_setSurface(JNIEnv *env, jclass clazz, jobject surface)
{
    theEnv = env;
    theSurface = surface;
}
