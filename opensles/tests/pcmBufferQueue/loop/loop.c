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

// Test program to record from default audio input and playback to default audio output

#include "SLES/OpenSLES.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define ASSERT_EQ(x, y) assert((x) == (y))

typedef struct {
    short left;
    short right;
} stereo;

// These ping-pong buffers hold 1 second of audio at 44.1 kHz
#define SR 8000 //44100
#define N 3
static stereo buffers[SR*N];

// Index of which buffer to enqueue next
static SLuint32 whichRecord;
static SLuint32 whichPlay;

SLBufferQueueItf recorderBufferQueue;
SLBufferQueueItf playerBufferQueue;

// Called after audio recorder fills a buffer with data
static void recorderCallback(SLBufferQueueItf caller, void *context)
{
    SLresult result;
    putchar('*');
    fflush(stdout);

    // Enqueue the next empty buffer for the recorder to fill
    assert(whichRecord < N);
    void *buffer = &buffers[SR*whichRecord];
    SLuint32 size = SR*sizeof(stereo);
    result = (*recorderBufferQueue)->Enqueue(recorderBufferQueue, buffer, size);
    ASSERT_EQ(SL_RESULT_SUCCESS, result);
    if (++whichRecord >= N)
        whichRecord = 0;

    // Enqueue the just-filled buffer for the player to empty
    assert(whichPlay < N);
    buffer = &buffers[SR*whichPlay];
    result = (*playerBufferQueue)->Enqueue(playerBufferQueue, buffer, size);
    ASSERT_EQ(SL_RESULT_SUCCESS, result);
    if (++whichPlay >= N)
        whichPlay = 0;

}

int main(int argc, char **argv)
{
    SLresult result;

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

    // create an audio player with buffer queue source and output mix sink
    SLDataSource audiosrc;
    SLDataSink audiosnk;
    SLDataFormat_PCM pcm;
    SLDataLocator_OutputMix locator_outputmix;
    SLDataLocator_BufferQueue locator_bufferqueue;
    locator_bufferqueue.locatorType = SL_DATALOCATOR_BUFFERQUEUE;
    locator_bufferqueue.numBuffers = 2;
    locator_outputmix.locatorType = SL_DATALOCATOR_OUTPUTMIX;
    locator_outputmix.outputMix = outputmixObject;
    pcm.formatType = SL_DATAFORMAT_PCM;
    pcm.numChannels = 1; //2;
    pcm.samplesPerSec = SL_SAMPLINGRATE_8; //44_1;
    pcm.bitsPerSample = SL_PCMSAMPLEFORMAT_FIXED_16;
    pcm.containerSize = 16;
    pcm.channelMask = SL_SPEAKER_FRONT_CENTER; //SL_SPEAKER_FRONT_LEFT | SL_SPEAKER_FRONT_RIGHT;
    pcm.endianness = SL_BYTEORDER_LITTLEENDIAN;
    audiosrc.pLocator = &locator_bufferqueue;
    audiosrc.pFormat = &pcm;
    audiosnk.pLocator = &locator_outputmix;
    audiosnk.pFormat = NULL;
    SLObjectItf playerObject;
    SLInterfaceID ids[1] = {SL_IID_BUFFERQUEUE};
    SLboolean flags[1] = {SL_BOOLEAN_TRUE};
    result = (*engineEngine)->CreateAudioPlayer(engineEngine, &playerObject, &audiosrc, &audiosnk, 1, ids, flags);
    ASSERT_EQ(SL_RESULT_SUCCESS, result);
    result = (*playerObject)->Realize(playerObject, SL_BOOLEAN_FALSE);
    ASSERT_EQ(SL_RESULT_SUCCESS, result);
    SLPlayItf playerPlay;
    result = (*playerObject)->GetInterface(playerObject, SL_IID_PLAY, &playerPlay);
    ASSERT_EQ(SL_RESULT_SUCCESS, result);
    result = (*playerObject)->GetInterface(playerObject, SL_IID_BUFFERQUEUE, &playerBufferQueue);
    ASSERT_EQ(SL_RESULT_SUCCESS, result);
    result = (*playerPlay)->SetPlayState(playerPlay, SL_PLAYSTATE_PLAYING);
    ASSERT_EQ(SL_RESULT_SUCCESS, result);

    // Create an audio recorder with microphone device source and buffer queue sink.
    // The buffer queue as sink is an Android-specific extension.

    SLDataLocator_IODevice locator_iodevice;
    locator_iodevice.locatorType = SL_DATALOCATOR_IODEVICE;
    locator_iodevice.deviceType = SL_IODEVICE_AUDIOINPUT;
    locator_iodevice.deviceID = SL_DEFAULTDEVICEID_AUDIOINPUT;
    locator_iodevice.device = NULL;
    audiosrc.pLocator = &locator_iodevice;
    audiosrc.pFormat = &pcm;
    audiosnk.pLocator = &locator_bufferqueue;
    audiosnk.pFormat = &pcm;
    SLObjectItf recorderObject;
    result = (*engineEngine)->CreateAudioRecorder(engineEngine, &recorderObject, &audiosrc, &audiosnk, 1, ids, flags);
    ASSERT_EQ(SL_RESULT_SUCCESS, result);
    result = (*recorderObject)->Realize(recorderObject, SL_BOOLEAN_FALSE);
    ASSERT_EQ(SL_RESULT_SUCCESS, result);
    SLRecordItf recorderRecord;
    result = (*recorderObject)->GetInterface(recorderObject, SL_IID_RECORD, &recorderRecord);
    ASSERT_EQ(SL_RESULT_SUCCESS, result);
    result = (*recorderObject)->GetInterface(recorderObject, SL_IID_BUFFERQUEUE, &recorderBufferQueue);
    ASSERT_EQ(SL_RESULT_SUCCESS, result);
    result = (*recorderBufferQueue)->RegisterCallback(recorderBufferQueue, recorderCallback, NULL);
    ASSERT_EQ(SL_RESULT_SUCCESS, result);

    // Enqueue some empty buffers for the recorder
    for (whichRecord = 0; whichRecord < N-1; ++whichRecord) {
        result = (*recorderBufferQueue)->Enqueue(recorderBufferQueue, &buffers[SR*whichRecord], SR*sizeof(stereo));
        ASSERT_EQ(SL_RESULT_SUCCESS, result);
    }

    // Kick off the recorder
    whichPlay = 0;
    result = (*recorderRecord)->SetRecordState(recorderRecord, SL_RECORDSTATE_RECORDING);
    ASSERT_EQ(SL_RESULT_SUCCESS, result);

    // Wait patiently
    for (;;) {
        usleep(1000000);
        putchar('.');
        SLBufferQueueState playerBQState;
        result = (*playerBufferQueue)->GetState(playerBufferQueue, &playerBQState);
        ASSERT_EQ(SL_RESULT_SUCCESS, result);
        SLBufferQueueState recorderBQState;
        result = (*recorderBufferQueue)->GetState(recorderBufferQueue, &recorderBQState);
        ASSERT_EQ(SL_RESULT_SUCCESS, result);
        printf("pC%u pI%u rC%u rI%u\n", (unsigned) playerBQState.count, (unsigned) playerBQState.playIndex, (unsigned) recorderBQState.count, (unsigned) recorderBQState.playIndex);
        fflush(stdout);
    }

    //return EXIT_SUCCESS;
}
