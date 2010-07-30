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

/** \file BufferQueue_test.cpp */

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "SLES/OpenSLES.h"

#ifdef ANDROID
#include "gtest/gtest.h"
#else
#define ASSERT_EQ(x, y) assert((x) == (y))
#endif

typedef struct {
    short left;
    short right;
} stereo;

// 1 second of stereo audio at 44.1 kHz
static stereo stereoBuffer1[44100 * 1];

static void do_my_testing()
{
    SLresult result;

    // initialize the test tone to be a sine sweep from 441 Hz to 882 Hz
    unsigned nframes = sizeof(stereoBuffer1) / sizeof(stereoBuffer1[0]);
    float nframes_ = (float) nframes;
    SLuint32 i;
    for (i = 0; i < nframes; ++i) {
        float i_ = (float) i;
        float pcm_ = sin((i_*(1.0f+0.5f*(i_/nframes_))*0.01 * M_PI * 2.0));
        int pcm = (int) (pcm_ * 32766.0);
        assert(-32768 <= pcm && pcm <= 32767);
        stereoBuffer1[i].left = pcm;
        stereoBuffer1[nframes - 1 - i].right = pcm;
    }

    // create engine
    SLObjectItf engineObject;
    result = slCreateEngine(&engineObject, 0, NULL, 0, NULL, NULL);
    ASSERT_EQ(SL_RESULT_SUCCESS, result);
    result = (*engineObject)->Realize(engineObject, SL_BOOLEAN_FALSE);
    ASSERT_EQ(SL_RESULT_SUCCESS, result);
    SLEngineItf engineEngine;
    result = (*engineObject)->GetInterface(engineObject, SL_IID_ENGINE, &engineEngine);
    ASSERT_EQ(SL_RESULT_SUCCESS, result);

    // create output mix
    SLObjectItf outputmixObject;
    result = (*engineEngine)->CreateOutputMix(engineEngine, &outputmixObject, 0, NULL, NULL);
    ASSERT_EQ(SL_RESULT_SUCCESS, result);
    result = (*outputmixObject)->Realize(outputmixObject, SL_BOOLEAN_FALSE);
    ASSERT_EQ(SL_RESULT_SUCCESS, result);

    // set up data structures to create an audio player with buffer queue source and output mix sink
    SLDataSource audiosrc;
    SLDataSink audiosnk;
    SLDataFormat_PCM pcm;
    SLDataLocator_OutputMix locator_outputmix;
    SLDataLocator_BufferQueue locator_bufferqueue;
    locator_bufferqueue.locatorType = SL_DATALOCATOR_BUFFERQUEUE;
    locator_bufferqueue.numBuffers = 0;
    locator_outputmix.locatorType = SL_DATALOCATOR_OUTPUTMIX;
    locator_outputmix.outputMix = outputmixObject;
    pcm.formatType = SL_DATAFORMAT_PCM;
    pcm.numChannels = 2;
    pcm.samplesPerSec = SL_SAMPLINGRATE_44_1;
    pcm.bitsPerSample = SL_PCMSAMPLEFORMAT_FIXED_16;
    pcm.containerSize = 16;
    pcm.channelMask = SL_SPEAKER_FRONT_LEFT | SL_SPEAKER_FRONT_RIGHT;
    pcm.endianness = SL_BYTEORDER_LITTLEENDIAN;
    audiosrc.pLocator = &locator_bufferqueue;
    audiosrc.pFormat = &pcm;
    audiosnk.pLocator = &locator_outputmix;
    audiosnk.pFormat = NULL;
    SLObjectItf playerObject;
    SLInterfaceID ids[1] = {SL_IID_BUFFERQUEUE};
    SLboolean flags[1] = {SL_BOOLEAN_TRUE};

    // try creating audio player with various invalid values for numBuffers
    static const SLuint32 invalidNumBuffers[] = {0, 0xFFFFFFFF, 0x80000000, 0x10002, 0x102, 0x101, 0x100};
    for (i = 0; i < sizeof(invalidNumBuffers) / sizeof(invalidNumBuffers[0]); ++i) {
        locator_bufferqueue.numBuffers = invalidNumBuffers[i];
        result = (*engineEngine)->CreateAudioPlayer(engineEngine, &playerObject, &audiosrc, &audiosnk, 1, ids, flags);
        ASSERT_EQ(SL_RESULT_PARAMETER_INVALID, result);
    }

    // now try some valid values for numBuffers */
    static const SLuint32 validNumBuffers[] = {1, 2, 3, 4, 5, 6, 7, 8, 255};
    for (i = 0; i < sizeof(validNumBuffers) / sizeof(validNumBuffers[0]); ++i) {
        locator_bufferqueue.numBuffers = validNumBuffers[i];
        result = (*engineEngine)->CreateAudioPlayer(engineEngine, &playerObject, &audiosrc, &audiosnk, 1, ids, flags);
        ASSERT_EQ(SL_RESULT_SUCCESS, result);
        result = (*playerObject)->Realize(playerObject, SL_BOOLEAN_FALSE);
        ASSERT_EQ(SL_RESULT_SUCCESS, result);
        // get the play interface
        SLPlayItf playerPlay;
        result = (*playerObject)->GetInterface(playerObject, SL_IID_PLAY, &playerPlay);
        ASSERT_EQ(SL_RESULT_SUCCESS, result);
        SLuint32 playerState;
        // verify that player is initially stopped
        result = (*playerPlay)->GetPlayState(playerPlay, &playerState);
        ASSERT_EQ(SL_RESULT_SUCCESS, result);
        ASSERT_EQ(SL_PLAYSTATE_STOPPED, playerState);
        // get the buffer queue interface
        SLBufferQueueItf playerBufferQueue;
        result = (*playerObject)->GetInterface(playerObject, SL_IID_BUFFERQUEUE, &playerBufferQueue);
        ASSERT_EQ(SL_RESULT_SUCCESS, result);
        // verify that buffer queue is initially empty
        SLBufferQueueState bufferqueueState;
        result = (*playerBufferQueue)->GetState(playerBufferQueue, &bufferqueueState);
        ASSERT_EQ(SL_RESULT_SUCCESS, result);
        ASSERT_EQ((SLuint32) 0, bufferqueueState.count);
        ASSERT_EQ((SLuint32) 0, bufferqueueState.playIndex);
        // try enqueuing the maximum number of buffers while stopped
        SLuint32 j;
        for (j = 0; j < validNumBuffers[i]; ++j) {
            result = (*playerBufferQueue)->Enqueue(playerBufferQueue, "test", 4);
            ASSERT_EQ(SL_RESULT_SUCCESS, result);
            // verify that each buffer is enqueued properly and increments the buffer count
            result = (*playerBufferQueue)->GetState(playerBufferQueue, &bufferqueueState);
            ASSERT_EQ(SL_RESULT_SUCCESS, result);
            ASSERT_EQ(j + 1, bufferqueueState.count);
            ASSERT_EQ((SLuint32) 0, bufferqueueState.playIndex);
            // verify that player is still stopped; enqueue should not affect play state
            result = (*playerPlay)->GetPlayState(playerPlay, &playerState);
            ASSERT_EQ(SL_RESULT_SUCCESS, result);
            ASSERT_EQ(SL_PLAYSTATE_STOPPED, playerState);
        }
        // enqueue one more buffer and make sure it fails
        result = (*playerBufferQueue)->Enqueue(playerBufferQueue, "test", 4);
        ASSERT_EQ(SL_RESULT_BUFFER_INSUFFICIENT, result);
        // verify that the failed enqueue did not affect the buffer count
        result = (*playerBufferQueue)->GetState(playerBufferQueue, &bufferqueueState);
        ASSERT_EQ(SL_RESULT_SUCCESS, result);
        ASSERT_EQ(validNumBuffers[i], bufferqueueState.count);
        ASSERT_EQ((SLuint32) 0, bufferqueueState.playIndex);
        // now clear the buffer queue
        result = (*playerBufferQueue)->Clear(playerBufferQueue);
        ASSERT_EQ(SL_RESULT_SUCCESS, result);
        // make sure the clear works
        result = (*playerBufferQueue)->GetState(playerBufferQueue, &bufferqueueState);
        ASSERT_EQ(SL_RESULT_SUCCESS, result);
        ASSERT_EQ((SLuint32) 0, bufferqueueState.count);
        ASSERT_EQ((SLuint32) 0, bufferqueueState.playIndex);
        // change play state from paused to stopped
        result = (*playerPlay)->SetPlayState(playerPlay, SL_PLAYSTATE_PAUSED);
        ASSERT_EQ(SL_RESULT_SUCCESS, result);
        // verify that player is really paused
        result = (*playerPlay)->GetPlayState(playerPlay, &playerState);
        ASSERT_EQ(SL_RESULT_SUCCESS, result);
        ASSERT_EQ(SL_PLAYSTATE_PAUSED, playerState);
        // try enqueuing the maximum number of buffers while paused
        for (j = 0; j < validNumBuffers[i]; ++j) {
            result = (*playerBufferQueue)->Enqueue(playerBufferQueue, "test", 4);
            ASSERT_EQ(SL_RESULT_SUCCESS, result);
            // verify that each buffer is enqueued properly and increments the buffer count
            result = (*playerBufferQueue)->GetState(playerBufferQueue, &bufferqueueState);
            ASSERT_EQ(SL_RESULT_SUCCESS, result);
            ASSERT_EQ(j + 1, bufferqueueState.count);
            ASSERT_EQ((SLuint32) 0, bufferqueueState.playIndex);
            // verify that player is still paused; enqueue should not affect play state
            result = (*playerPlay)->GetPlayState(playerPlay, &playerState);
            ASSERT_EQ(SL_RESULT_SUCCESS, result);
            ASSERT_EQ(SL_PLAYSTATE_PAUSED, playerState);
        }
        // enqueue one more buffer and make sure it fails
        result = (*playerBufferQueue)->Enqueue(playerBufferQueue, "test", 4);
        ASSERT_EQ(SL_RESULT_BUFFER_INSUFFICIENT, result);
        // verify that the failed enqueue did not affect the buffer count
        result = (*playerBufferQueue)->GetState(playerBufferQueue, &bufferqueueState);
        ASSERT_EQ(SL_RESULT_SUCCESS, result);
        ASSERT_EQ(validNumBuffers[i], bufferqueueState.count);
        ASSERT_EQ((SLuint32) 0, bufferqueueState.playIndex);
        // now clear the buffer queue
        result = (*playerBufferQueue)->Clear(playerBufferQueue);
        ASSERT_EQ(SL_RESULT_SUCCESS, result);
        // make sure the clear works
        result = (*playerBufferQueue)->GetState(playerBufferQueue, &bufferqueueState);
        ASSERT_EQ(SL_RESULT_SUCCESS, result);
        ASSERT_EQ((SLuint32) 0, bufferqueueState.count);
        ASSERT_EQ((SLuint32) 0, bufferqueueState.playIndex);
        // try every possible play state transition while buffer queue is empty
        static const SLuint32 newStates1[] = {
            SL_PLAYSTATE_PAUSED,    // paused -> paused
            SL_PLAYSTATE_STOPPED,   // paused -> stopped
            SL_PLAYSTATE_PAUSED,    // stopped -> paused, also done earlier
            SL_PLAYSTATE_PLAYING,   // paused -> playing
            SL_PLAYSTATE_PLAYING,   // playing -> playing
            SL_PLAYSTATE_STOPPED,   // playing -> stopped
            SL_PLAYSTATE_STOPPED,   // stopped -> stopped
            SL_PLAYSTATE_PLAYING,   // stopped -> playing
            SL_PLAYSTATE_PAUSED     // playing -> paused
        };
        // initially the play state is (still) paused
        result = (*playerPlay)->GetPlayState(playerPlay, &playerState);
        ASSERT_EQ(SL_RESULT_SUCCESS, result);
        ASSERT_EQ(SL_PLAYSTATE_PAUSED, playerState);
        for (j = 0; j < sizeof(newStates1) / sizeof(newStates1[0]); ++j) {
            // change play state
            result = (*playerPlay)->SetPlayState(playerPlay, newStates1[j]);
            ASSERT_EQ(SL_RESULT_SUCCESS, result);
            // make sure the new play state is taken
            result = (*playerPlay)->GetPlayState(playerPlay, &playerState);
            ASSERT_EQ(SL_RESULT_SUCCESS, result);
            ASSERT_EQ(newStates1[j], playerState);
            // changing the play state should not affect the buffer count
            result = (*playerBufferQueue)->GetState(playerBufferQueue, &bufferqueueState);
            ASSERT_EQ(SL_RESULT_SUCCESS, result);
            ASSERT_EQ((SLuint32) 0, bufferqueueState.count);
            ASSERT_EQ((SLuint32) 0, bufferqueueState.playIndex);
        }
        // finally the play state is (again) paused
        result = (*playerPlay)->GetPlayState(playerPlay, &playerState);
        ASSERT_EQ(SL_RESULT_SUCCESS, result);
        ASSERT_EQ(SL_PLAYSTATE_PAUSED, playerState);
        // enqueue a buffer
        result = (*playerBufferQueue)->Enqueue(playerBufferQueue, "test", 4);
        ASSERT_EQ(SL_RESULT_SUCCESS, result);
        // try several inaudible play state transitions while buffer queue is non-empty
        static const SLuint32 newStates2[] = {
            SL_PLAYSTATE_PAUSED,    // paused -> paused
            SL_PLAYSTATE_STOPPED,   // paused -> stopped
            SL_PLAYSTATE_STOPPED,   // stopped -> stopped
            SL_PLAYSTATE_PAUSED    // stopped -> paused
        };
        for (j = 0; j < sizeof(newStates2) / sizeof(newStates2[0]); ++j) {
            // change play state
            result = (*playerPlay)->SetPlayState(playerPlay, newStates2[j]);
            ASSERT_EQ(SL_RESULT_SUCCESS, result);
            // make sure the new play state is taken
            result = (*playerPlay)->GetPlayState(playerPlay, &playerState);
            ASSERT_EQ(SL_RESULT_SUCCESS, result);
            ASSERT_EQ(newStates2[j], playerState);
            // changing the play state should not affect the buffer count
            result = (*playerBufferQueue)->GetState(playerBufferQueue, &bufferqueueState);
            ASSERT_EQ(SL_RESULT_SUCCESS, result);
            ASSERT_EQ((SLuint32) 1, bufferqueueState.count);
            ASSERT_EQ((SLuint32) 0, bufferqueueState.playIndex);
        }
        // clear the buffer queue
        result = (*playerBufferQueue)->Clear(playerBufferQueue);
        ASSERT_EQ(SL_RESULT_SUCCESS, result);

        // now we're finally ready to make some noise

        // enqueue a buffer
        result = (*playerBufferQueue)->Enqueue(playerBufferQueue, stereoBuffer1, sizeof(stereoBuffer1));
        ASSERT_EQ(SL_RESULT_SUCCESS, result);
        // set play state to playing
        result = (*playerPlay)->SetPlayState(playerPlay, SL_PLAYSTATE_PLAYING);
        ASSERT_EQ(SL_RESULT_SUCCESS, result);
        // state should be playing immediately after enqueue
        result = (*playerPlay)->GetPlayState(playerPlay, &playerState);
        ASSERT_EQ(SL_RESULT_SUCCESS, result);
        ASSERT_EQ(SL_PLAYSTATE_PLAYING, playerState);
        // buffer should still be on the queue
        result = (*playerBufferQueue)->GetState(playerBufferQueue, &bufferqueueState);
        ASSERT_EQ(SL_RESULT_SUCCESS, result);
        ASSERT_EQ((SLuint32) 1, bufferqueueState.count);
        ASSERT_EQ((SLuint32) 0, bufferqueueState.playIndex);
        // wait 1.5 seconds
        usleep(1500000);
        // state should still be playing
        result = (*playerPlay)->GetPlayState(playerPlay, &playerState);
        ASSERT_EQ(SL_RESULT_SUCCESS, result);
        ASSERT_EQ(SL_PLAYSTATE_PLAYING, playerState);
        // buffer should be removed from the queue
        result = (*playerBufferQueue)->GetState(playerBufferQueue, &bufferqueueState);
        ASSERT_EQ(SL_RESULT_SUCCESS, result);
        ASSERT_EQ((SLuint32) 0, bufferqueueState.count);
        ASSERT_EQ((SLuint32) 1, bufferqueueState.playIndex);
        // destroy the player
        (*playerObject)->Destroy(playerObject);
    }

    // destroy the output mix
    (*outputmixObject)->Destroy(outputmixObject);
    // destroy the engine
    (*engineObject)->Destroy(engineObject);

}

int main(int argc, char **argv)
{
    do_my_testing();
    return EXIT_SUCCESS;
}
