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

#undef NDEBUG

#include "SLES/OpenSLES.h"
#include "SLES/OpenSLES_Android.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define ASSERT_EQ(x, y) do { if ((x) == (y)) ; else { fprintf(stderr, "0x%x != 0x%x\n", \
    (unsigned) (x), (unsigned) (y)); assert((x) == (y)); } } while (0)

// default values
static SLuint32 rxBufCount = 1;     // -r#
static SLuint32 txBufCount = 2;     // -t#
static SLuint32 bufSizeInFrames = 512;  // -f#
static SLuint32 channels = 2;       // -c#
static SLuint32 sampleRate = 44100; // -s#
static SLuint32 appBufCount = 0;    // -n#
static SLuint32 bufSizeInBytes = 0; // calculated
static SLboolean verbose = SL_BOOLEAN_FALSE;

// Storage area for the buffers
static char *buffers = NULL;

// Index of which buffer to enqueue next
static SLuint32 whichRecord;
static SLuint32 whichPlay;

SLAndroidSimpleBufferQueueItf recorderBufferQueue;
SLBufferQueueItf playerBufferQueue;

// Compute maximum of two values
static SLuint32 max(SLuint32 a, SLuint32 b)
{
    return a >= b ? a : b;
}

// Compute minimum of two values
static SLuint32 min(SLuint32 a, SLuint32 b)
{
    return a <= b ? a : b;
}

// Called after audio recorder fills a buffer with data
static void recorderCallback(SLAndroidSimpleBufferQueueItf caller, void *context)
{
    SLresult result;
    if (verbose) {
        putchar('*');
        fflush(stdout);
    }

    // Enqueue the next empty buffer for the recorder to fill
    assert(whichRecord < appBufCount);
    void *buffer = &buffers[bufSizeInBytes * whichRecord];
    result = (*recorderBufferQueue)->Enqueue(recorderBufferQueue, buffer, bufSizeInBytes);
    ASSERT_EQ(SL_RESULT_SUCCESS, result);
    if (++whichRecord >= appBufCount)
        whichRecord = 0;

    // Enqueue the just-filled buffer for the player to empty
    assert(whichPlay < appBufCount); // sic not tx
    buffer = &buffers[bufSizeInBytes * whichPlay];
    result = (*playerBufferQueue)->Enqueue(playerBufferQueue, buffer, bufSizeInBytes);
    // FIXME not sure yet why this is overflowing
    if (SL_RESULT_SUCCESS == result) {
        if (++whichPlay >= appBufCount)
            whichPlay = 0;
    } else {
        ASSERT_EQ(SL_RESULT_BUFFER_INSUFFICIENT, result);
    }

}

int main(int argc, char **argv)
{
    // process command-line options
    int i;
    for (i = 1; i < argc; ++i) {
        char *arg = argv[i];
        if (arg[0] != '-')
            break;
        // -r# number of receive buffers
        if (!strncmp(arg, "-r", 2)) {
            rxBufCount = atoi(&arg[2]);
            if (rxBufCount < 1 || rxBufCount > 8)
                fprintf(stderr, "%s: unusual receive buffer queue size (%u buffers)", argv[0],
                    (unsigned) rxBufCount);
        // -t# number of receive buffers
        } else if (!strncmp(arg, "-t", 2)) {
            txBufCount = atoi(&arg[2]);
            if (txBufCount < 1 || txBufCount > 8)
                fprintf(stderr, "%s: unusual transmit buffer queue size (%u buffers)", argv[0],
                    (unsigned) txBufCount);
        // -n# number of application buffers
        } else if (!strncmp(arg, "-n", 2)) {
            appBufCount = atoi(&arg[2]);
        // -f# size of each buffer in frames
        } else if (!strncmp(arg, "-f", 2)) {
            bufSizeInFrames = atoi(&arg[2]);
            if (bufSizeInFrames == 0) {
                fprintf(stderr, "%s: unusual buffer size (%u frames)\n", argv[0],
                    (unsigned) bufSizeInFrames);
            }
        // -c1 mono or -c2 stereo
        } else if (!strncmp(arg, "-c", 2)) {
            channels = atoi(&arg[2]);
            if (channels < 1 || channels > 2) {
                fprintf(stderr, "%s: unusual channel count ignored (%u)\n", argv[0],
                    (unsigned) channels);
                channels = 2;
            }
        // -s# sample rate in Hz
        } else if (!strncmp(arg, "-s", 2)) {
            sampleRate = atoi(&arg[2]);
            switch (sampleRate) {
            case 8000:
            case 11025:
            case 16000:
            case 22050:
            case 32000:
            case 44100:
                break;
            default:
                fprintf(stderr, "%s: unusual sample rate (%u Hz)\n", argv[0],
                    (unsigned) sampleRate);
                break;
            }
        // -v verbose
        } else if (!strcmp(arg, "-v")) {
            verbose = SL_BOOLEAN_TRUE;
        } else
            fprintf(stderr, "%s: unknown option %s\n", argv[0], arg);
    }
    if (i < argc) {
        fprintf(stderr, "usage: %s -r# -t# -f# -r# -m/-s\n", argv[0]);
        fprintf(stderr, "  -r# receive buffer queue count for microphone input, default 1\n");
        fprintf(stderr, "  -t# transmit buffer queue count for speaker output, default 2\n");
        fprintf(stderr, "  -f# number of frames per buffer, default 512\n");
        fprintf(stderr, "  -s# sample rate in Hz, default 44100\n");
        fprintf(stderr, "  -n# number of application-allocated buffers, default max(-r#,-t#)\n");
        fprintf(stderr, "  -c1 mono\n");
        fprintf(stderr, "  -c2 stereo, default\n");
    }
    if (appBufCount == 0)
        appBufCount = max(rxBufCount, txBufCount);
    if (appBufCount == 0)
        appBufCount = 1;
    if (appBufCount < max(rxBufCount, txBufCount))
        fprintf(stderr, "%s: unusual application buffer count (%u buffers)", argv[0],
            (unsigned) appBufCount);
    bufSizeInBytes = channels * bufSizeInFrames * sizeof(short);
    buffers = (char *) malloc(bufSizeInBytes * appBufCount);
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
    SLDataLocator_BufferQueue locator_bufferqueue_tx;
    locator_bufferqueue_tx.locatorType = SL_DATALOCATOR_BUFFERQUEUE;
    locator_bufferqueue_tx.numBuffers = txBufCount;
    locator_outputmix.locatorType = SL_DATALOCATOR_OUTPUTMIX;
    locator_outputmix.outputMix = outputmixObject;
    pcm.formatType = SL_DATAFORMAT_PCM;
    pcm.numChannels = channels;
    pcm.samplesPerSec = sampleRate * 1000;
    pcm.bitsPerSample = SL_PCMSAMPLEFORMAT_FIXED_16;
    pcm.containerSize = 16;
    pcm.channelMask = channels == 1 ? SL_SPEAKER_FRONT_CENTER :
        (SL_SPEAKER_FRONT_LEFT | SL_SPEAKER_FRONT_RIGHT);
    pcm.endianness = SL_BYTEORDER_LITTLEENDIAN;
    audiosrc.pLocator = &locator_bufferqueue_tx;
    audiosrc.pFormat = &pcm;
    audiosnk.pLocator = &locator_outputmix;
    audiosnk.pFormat = NULL;
    SLObjectItf playerObject;
    SLInterfaceID ids_tx[1] = {SL_IID_BUFFERQUEUE};
    SLboolean flags_tx[1] = {SL_BOOLEAN_TRUE};
    result = (*engineEngine)->CreateAudioPlayer(engineEngine, &playerObject, &audiosrc, &audiosnk,
        1, ids_tx, flags_tx);
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
    SLDataLocator_AndroidSimpleBufferQueue locator_bufferqueue_rx;
    locator_iodevice.locatorType = SL_DATALOCATOR_IODEVICE;
    locator_iodevice.deviceType = SL_IODEVICE_AUDIOINPUT;
    locator_iodevice.deviceID = SL_DEFAULTDEVICEID_AUDIOINPUT;
    locator_iodevice.device = NULL;
    audiosrc.pLocator = &locator_iodevice;
    audiosrc.pFormat = &pcm;
    locator_bufferqueue_rx.locatorType = SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE;
    locator_bufferqueue_rx.numBuffers = rxBufCount;
    audiosnk.pLocator = &locator_bufferqueue_rx;
    audiosnk.pFormat = &pcm;
    SLObjectItf recorderObject;
    SLInterfaceID ids_rx[1] = {SL_IID_ANDROIDSIMPLEBUFFERQUEUE};
    SLboolean flags_rx[1] = {SL_BOOLEAN_TRUE};
    result = (*engineEngine)->CreateAudioRecorder(engineEngine, &recorderObject, &audiosrc,
        &audiosnk, 1, ids_rx, flags_rx);
    ASSERT_EQ(SL_RESULT_SUCCESS, result);
    result = (*recorderObject)->Realize(recorderObject, SL_BOOLEAN_FALSE);
    ASSERT_EQ(SL_RESULT_SUCCESS, result);
    SLRecordItf recorderRecord;
    result = (*recorderObject)->GetInterface(recorderObject, SL_IID_RECORD, &recorderRecord);
    ASSERT_EQ(SL_RESULT_SUCCESS, result);
    result = (*recorderObject)->GetInterface(recorderObject, SL_IID_ANDROIDSIMPLEBUFFERQUEUE,
        &recorderBufferQueue);
    ASSERT_EQ(SL_RESULT_SUCCESS, result);
    result = (*recorderBufferQueue)->RegisterCallback(recorderBufferQueue, recorderCallback, NULL);
    ASSERT_EQ(SL_RESULT_SUCCESS, result);

    // Enqueue some empty buffers for the recorder
    SLuint32 temp = min(rxBufCount, appBufCount);
    for (whichRecord = 0; whichRecord < (temp <= 1 ? 1 : temp - 1); ++whichRecord) {
        result = (*recorderBufferQueue)->Enqueue(recorderBufferQueue,
            &buffers[bufSizeInBytes * whichRecord], bufSizeInBytes);
        ASSERT_EQ(SL_RESULT_SUCCESS, result);
    }
    if (whichRecord >= appBufCount)
        whichRecord = 0;

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
        SLAndroidSimpleBufferQueueState recorderBQState;
        result = (*recorderBufferQueue)->GetState(recorderBufferQueue, &recorderBQState);
        ASSERT_EQ(SL_RESULT_SUCCESS, result);
        if (verbose) {
            printf("pC%u pI%u rC%u rI%u\n", (unsigned) playerBQState.count,
                (unsigned) playerBQState.playIndex, (unsigned) recorderBQState.count,
                (unsigned) recorderBQState.index);
            fflush(stdout);
        }
    }

    //return EXIT_SUCCESS;
}
