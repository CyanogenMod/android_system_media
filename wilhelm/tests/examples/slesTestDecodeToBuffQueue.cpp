/*
 * Copyright (C) 2011 The Android Open Source Project
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

/* Audio Decode Test

First run the program from shell:
  # slesTest_decodeToBuffQueue /sdcard/myFile.mp3 4

These use adb on host to retrieve the decoded file:
  % adb pull /sdcard/myFile.mp3.raw myFile.raw

How to examine the output with Audacity:
 Project / Import raw data
 Select myFile.raw file, then click Open button
 Choose these options:
  Signed 16-bit PCM
  Little-endian
  1 Channel (Mono) / 2 Channels (Stereo) based on the selected file
  Sample rate same as the selected file
 Click Import button

*/


#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>
#include <fcntl.h>
#include <utils/threads.h>

#include <SLES/OpenSLES.h>
#include <SLES/OpenSLES_Android.h>
#include <SLES/OpenSLES_AndroidConfiguration.h>

/* Explicitly requesting SL_IID_ANDROIDSIMPLEBUFFERQUEUE and SL_IID_PREFETCHSTATUS
 * on the AudioPlayer object */
#define NUM_EXPLICIT_INTERFACES_FOR_PLAYER 2

/* Size of the decode buffer queue */
#define NB_BUFFERS_IN_QUEUE 4
/* Size of each buffer in the queue */
#define BUFFER_SIZE_IN_SAMPLES 1152 // number of samples per MP3 frame
#define BUFFER_SIZE_IN_BYTES   (2*BUFFER_SIZE_IN_SAMPLES)

/* Local storage for decoded audio data */
int8_t pcmData[NB_BUFFERS_IN_QUEUE * BUFFER_SIZE_IN_BYTES];

/* destination for decoded data */
static FILE* gFp;

/* to display the number of decode iterations */
static int counter=0;

/* to signal to the test app the end of the stream to decode has been reached */
bool eos = false;
android::Mutex eosLock;
android::Condition eosCondition;

/* used to detect errors likely to have occured when the OpenSL ES framework fails to open
 * a resource, for instance because a file URI is invalid, or an HTTP server doesn't respond.
 */
#define PREFETCHEVENT_ERROR_CANDIDATE \
        (SL_PREFETCHEVENT_STATUSCHANGE | SL_PREFETCHEVENT_FILLLEVELCHANGE)

//-----------------------------------------------------------------
/* Exits the application if an error is encountered */
#define ExitOnError(x) ExitOnErrorFunc(x,__LINE__)

void ExitOnErrorFunc( SLresult result , int line)
{
    if (SL_RESULT_SUCCESS != result) {
        fprintf(stdout, "%lu error code encountered at line %d, exiting\n", result, line);
        exit(EXIT_FAILURE);
    }
}

//-----------------------------------------------------------------
/* Structure for passing information to callback function */
typedef struct CallbackCntxt_ {
    SLPlayItf playItf;
    SLuint32  size;
    SLint8*   pDataBase;    // Base address of local audio data storage
    SLint8*   pData;        // Current address of local audio data storage
} CallbackCntxt;

//-----------------------------------------------------------------
void SignalEos() {
    android::Mutex::Autolock autoLock(eosLock);
    eos = true;
    eosCondition.signal();
}

//-----------------------------------------------------------------
/* Callback for "prefetch" events, here used to detect audio resource opening errors */
void PrefetchEventCallback( SLPrefetchStatusItf caller,  void *pContext, SLuint32 event)
{
    SLpermille level = 0;
    (*caller)->GetFillLevel(caller, &level);
    SLuint32 status;
    //fprintf(stdout, "PrefetchEventCallback: received event %lu\n", event);
    (*caller)->GetPrefetchStatus(caller, &status);
    if ((PREFETCHEVENT_ERROR_CANDIDATE == (event & PREFETCHEVENT_ERROR_CANDIDATE))
            && (level == 0) && (status == SL_PREFETCHSTATUS_UNDERFLOW)) {
        fprintf(stdout, "PrefetchEventCallback: Error while prefetching data, exiting\n");
        SignalEos();
    }
}

//-----------------------------------------------------------------
/* Callback for "playback" events, i.e. event happening during decoding */
void DecCallback(
        SLPlayItf caller,
        void *pContext,
        SLuint32 event)
{
    if (SL_PLAYEVENT_HEADATEND & event) {
        fprintf(stdout, "SL_PLAYEVENT_HEADATEND reached\n");
        SignalEos();
    }

    if (SL_PLAYEVENT_HEADATNEWPOS & event) {
        SLmillisecond pMsec = 0;
        (*caller)->GetPosition(caller, &pMsec);
        fprintf(stdout, "SL_PLAYEVENT_HEADATNEWPOS current position=%lums\n", pMsec);
    }

    if (SL_PLAYEVENT_HEADATMARKER & event) {
        SLmillisecond pMsec = 0;
        (*caller)->GetPosition(caller, &pMsec);
        fprintf(stdout, "SL_PLAYEVENT_HEADATMARKER current position=%lums\n", pMsec);
    }
}

//-----------------------------------------------------------------
/* Callback for decoding buffer queue events */
void DecBufferQueueCallback(
        SLAndroidSimpleBufferQueueItf queueItf,
        void *pContext)
{
    counter++;
    fprintf(stdout, "DecBufferQueueCallback called (iteration %d)\n", counter);

    CallbackCntxt *pCntxt = (CallbackCntxt*)pContext;

    /* Save the decoded data  */
    if (fwrite(pCntxt->pDataBase, BUFFER_SIZE_IN_BYTES, 1, gFp) < BUFFER_SIZE_IN_BYTES) {
        fprintf(stdout, "Error writing to output file, signaling EOS\n");
        SignalEos();
        return;
    }

    /* Increase data pointer by buffer size */
    pCntxt->pData += BUFFER_SIZE_IN_BYTES;

    if (pCntxt->pData >= pCntxt->pDataBase + (NB_BUFFERS_IN_QUEUE * BUFFER_SIZE_IN_BYTES)) {
        pCntxt->pData = pCntxt->pDataBase;
    }

    ExitOnError( (*queueItf)->Enqueue(queueItf, pCntxt->pDataBase, BUFFER_SIZE_IN_BYTES) );
    // Note: adding a sleep here or any sync point is a way to slow down the decoding, or
    //  synchronize it with some other event, as the OpenSL ES framework will block until the
    //  buffer queue callback return to proceed with the decoding.

/*
    SLAndroidSimpleBufferQueueState decQueueState;
    ExitOnError( (*queueItf)->GetState(queueItf, &decQueueState) );

    fprintf(stderr, "\DecBufferQueueCallback now has pCntxt->pData=%p queue: "
            "count=%lu playIndex=%lu\n",
            pCntxt->pData, decQueueState.count, decQueueState.index);
*/
}

//-----------------------------------------------------------------

/* Decode an audio path by opening a file descriptor on that path  */
void TestDecToBuffQueue( SLObjectItf sl, const char* path)
{
    size_t len = strlen((const char *) path);
    char* outputPath = (char*) malloc(len + 4 + 1); // save room to concatenate ".raw"
    if (NULL == outputPath) {
        ExitOnError(SL_RESULT_RESOURCE_ERROR);
    }
    memcpy(outputPath, path, len + 1);
    strcat(outputPath, ".raw");
    gFp = fopen(outputPath, "w");
    if (NULL == gFp) {
        ExitOnError(SL_RESULT_RESOURCE_ERROR);
    }

    SLresult  result;
    SLEngineItf EngineItf;

    /* Objects this application uses: one audio player */
    SLObjectItf  player;

    /* Interfaces for the audio player */
    SLAndroidSimpleBufferQueueItf decBuffQueueItf;
    SLPrefetchStatusItf           prefetchItf;
    SLPlayItf                     playItf;

    /* Source of audio data for the decoding */
    SLDataSource      decSource;
    SLDataLocator_URI decUri;
    SLDataFormat_MIME decMime;

    /* Data sink for decoded audio */
    SLDataSink                decDest;
    SLDataLocator_AndroidSimpleBufferQueue decBuffQueue;
    SLDataFormat_PCM          pcm;

    SLboolean required[NUM_EXPLICIT_INTERFACES_FOR_PLAYER];
    SLInterfaceID iidArray[NUM_EXPLICIT_INTERFACES_FOR_PLAYER];

    /* Get the SL Engine Interface which is implicit */
    result = (*sl)->GetInterface(sl, SL_IID_ENGINE, (void*)&EngineItf);
    ExitOnError(result);

    /* Initialize arrays required[] and iidArray[] */
    for (int i=0 ; i < NUM_EXPLICIT_INTERFACES_FOR_PLAYER ; i++) {
        required[i] = SL_BOOLEAN_FALSE;
        iidArray[i] = SL_IID_NULL;
    }


    /* ------------------------------------------------------ */
    /* Configuration of the player  */

    /* Request the AndroidSimpleBufferQueue interface */
    required[0] = SL_BOOLEAN_TRUE;
    iidArray[0] = SL_IID_ANDROIDSIMPLEBUFFERQUEUE;
    /* Request the PrefetchStatus interface */
    required[1] = SL_BOOLEAN_TRUE;
    iidArray[1] = SL_IID_PREFETCHSTATUS;

    /* Setup the data source */
    decUri.locatorType = SL_DATALOCATOR_URI;
    decUri.URI = (SLchar*)path;
    decMime.formatType = SL_DATAFORMAT_MIME;
    /*     this is how ignored mime information is specified, according to OpenSL ES spec
     *     in 9.1.6 SLDataFormat_MIME and 8.23 SLMetadataTraversalItf GetChildInfo */
    decMime.mimeType      = (SLchar*)NULL;
    decMime.containerType = SL_CONTAINERTYPE_UNSPECIFIED;
    decSource.pLocator = (void *) &decUri;
    decSource.pFormat  = (void *) &decMime;

    /* Setup the data sink */
    decBuffQueue.locatorType = SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE;
    decBuffQueue.numBuffers = NB_BUFFERS_IN_QUEUE;
    /*    set up the format of the data in the buffer queue */
    pcm.formatType = SL_DATAFORMAT_PCM;
    // FIXME valid value required but currently ignored
    pcm.numChannels = 1;
    pcm.samplesPerSec = SL_SAMPLINGRATE_44_1;
    pcm.bitsPerSample = SL_PCMSAMPLEFORMAT_FIXED_16;
    pcm.containerSize = 16;
    pcm.channelMask = SL_SPEAKER_FRONT_LEFT;
    pcm.endianness = SL_BYTEORDER_LITTLEENDIAN;

    decDest.pLocator = (void *) &decBuffQueue;
    decDest.pFormat = (void * ) &pcm;

    /* Create the audio player */
    result = (*EngineItf)->CreateAudioPlayer(EngineItf, &player, &decSource, &decDest,
            NUM_EXPLICIT_INTERFACES_FOR_PLAYER, iidArray, required);
    ExitOnError(result);
    fprintf(stdout, "Player created\n");

    /* Realize the player in synchronous mode. */
    result = (*player)->Realize(player, SL_BOOLEAN_FALSE);
    ExitOnError(result);
    fprintf(stdout, "Player realized\n");

    /* Get the play interface which is implicit */
    result = (*player)->GetInterface(player, SL_IID_PLAY, (void*)&playItf);
    ExitOnError(result);

    /* Set up the player callback to get events during the decoding */
    // FIXME currently ignored
    result = (*playItf)->SetMarkerPosition(playItf, 2000);
    ExitOnError(result);
    result = (*playItf)->SetPositionUpdatePeriod(playItf, 500);
    ExitOnError(result);
    result = (*playItf)->SetCallbackEventsMask(playItf,
            SL_PLAYEVENT_HEADATMARKER | SL_PLAYEVENT_HEADATNEWPOS | SL_PLAYEVENT_HEADATEND);
    ExitOnError(result);
    result = (*playItf)->RegisterCallback(playItf, DecCallback, NULL);
    ExitOnError(result);
    fprintf(stdout, "Play callback registered\n");

    /* Get the buffer queue interface which was explicitly requested */
    result = (*player)->GetInterface(player, SL_IID_ANDROIDSIMPLEBUFFERQUEUE,
            (void*)&decBuffQueueItf);
    ExitOnError(result);

    /* Get the prefetch status interface which was explicitly requested */
    result = (*player)->GetInterface(player, SL_IID_PREFETCHSTATUS, (void*)&prefetchItf);
    ExitOnError(result);

    /* ------------------------------------------------------ */
    /* Initialize the callback and its context for the decoding buffer queue */
    CallbackCntxt cntxt;
    cntxt.playItf = playItf;
    cntxt.pDataBase = (int8_t*)&pcmData;
    cntxt.pData = cntxt.pDataBase;
    cntxt.size = sizeof(pcmData);
    result = (*decBuffQueueItf)->RegisterCallback(decBuffQueueItf, DecBufferQueueCallback, &cntxt);
    ExitOnError(result);

    /* Enqueue buffers to map the region of memory allocated to store the decoded data */
    fprintf(stdout,"Enqueueing buffer ");
    for(int i = 0 ; i < NB_BUFFERS_IN_QUEUE ; i++) {
        fprintf(stdout,"%d \n", i);
        result = (*decBuffQueueItf)->Enqueue(decBuffQueueItf, cntxt.pData, BUFFER_SIZE_IN_BYTES);
        ExitOnError(result);
        cntxt.pData += BUFFER_SIZE_IN_BYTES;
    }
    fprintf(stdout,"\n");
    cntxt.pData = cntxt.pDataBase;

    /* ------------------------------------------------------ */
    /* Initialize the callback for prefetch errors, if we can't open the resource to decode */
    result = (*prefetchItf)->RegisterCallback(prefetchItf, PrefetchEventCallback, &prefetchItf);
    ExitOnError(result);
    result = (*prefetchItf)->SetCallbackEventsMask(prefetchItf, PREFETCHEVENT_ERROR_CANDIDATE);
    ExitOnError(result);

    /* ------------------------------------------------------ */
    /* Start decoding */
    result = (*playItf)->SetPlayState(playItf, SL_PLAYSTATE_PLAYING);
    ExitOnError(result);
    fprintf(stdout, "Starting to decode\n");

    /* Decode until the end of the stream is reached */
    {
        android::Mutex::Autolock autoLock(eosLock);
        while (!eos) {
            eosCondition.wait(eosLock);
        }
    }
    fprintf(stdout, "EOS signaled\n");

    /* ------------------------------------------------------ */
    /* End of decoding */

    /* Stop decoding */
    result = (*playItf)->SetPlayState(playItf, SL_PLAYSTATE_STOPPED);
    ExitOnError(result);
    fprintf(stdout, "Stopped decoding\n");

    /* Destroy the AudioPlayer object */
    (*player)->Destroy(player);

    fclose(gFp);
}

//-----------------------------------------------------------------
int main(int argc, char* const argv[])
{
    SLresult    result;
    SLObjectItf sl;

    fprintf(stdout, "OpenSL ES test %s: exercises SLPlayItf and SLAndroidSimpleBufferQueueItf ",
            argv[0]);
    fprintf(stdout, "on an AudioPlayer object to decode a URI to PCM\n");

    if (argc != 2) {
        fprintf(stdout, "Usage: \t%s source_file\n", argv[0]);
        fprintf(stdout, "Example: \"%s /sdcard/myFile.mp3\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    SLEngineOption EngineOption[] = {
            {(SLuint32) SL_ENGINEOPTION_THREADSAFE, (SLuint32) SL_BOOLEAN_TRUE}
    };

    result = slCreateEngine( &sl, 1, EngineOption, 0, NULL, NULL);
    ExitOnError(result);

    /* Realizing the SL Engine in synchronous mode. */
    result = (*sl)->Realize(sl, SL_BOOLEAN_FALSE);
    ExitOnError(result);

    TestDecToBuffQueue(sl, argv[1]);

    /* Shutdown OpenSL ES */
    (*sl)->Destroy(sl);

    return EXIT_SUCCESS;
}
