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


#include <stdlib.h>
#include <stdio.h>
//#include <string.h>
#include <unistd.h>
//#include <sys/time.h>

#include "SLES/OpenSLES.h"
#include "SLES/OpenSLES_Android.h"


#define MAX_NUMBER_INTERFACES 2

#define PREFETCHEVENT_ERROR_CANDIDATE \
        (SL_PREFETCHEVENT_STATUSCHANGE | SL_PREFETCHEVENT_FILLLEVELCHANGE)

#define NB_BUFFERS 16
#define MPEG2_TS_BLOCK_SIZE 188
#define BUFFER_SIZE 20*MPEG2_TS_BLOCK_SIZE

/* Where we store the data to play */
char dataCache[BUFFER_SIZE * NB_BUFFERS];
/* From where we read the data to play */
FILE *file;

//-----------------------------------------------------------------
//* Exits the application if an error is encountered */
#define CheckErr(x) ExitOnErrorFunc(x,__LINE__)

void ExitOnErrorFunc( SLresult result , int line)
{
    if (SL_RESULT_SUCCESS != result) {
        fprintf(stderr, "%lu error code encountered at line %d, exiting\n", result, line);
        exit(EXIT_FAILURE);
    }
}

bool prefetchError = false;

//-----------------------------------------------------------------
/* AndroidBufferQueueItf callback for an audio player */
SLresult AndroidBufferQueueCallback(
        SLAndroidBufferQueueItf caller,
        void *pContext,                /* input */
        const void *pBufferData,       /* input */
        SLuint32 dataSize,             /* input */
        SLuint32 dataUsed,             /* input */
        const SLAndroidBufferItem *pItems,/* input */
        SLuint32 itemsLength           /* input */)
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
        SLAndroidBufferItem msgEos;
        msgEos.itemKey = SL_ANDROID_ITEMKEY_EOS;
        msgEos.itemSize = 0;
        // EOS message has no parameters, so the total size of the message is the size of the key
        //   plus the size if itemSize, both SLuint32
        (*caller)->Enqueue(caller, NULL /*pData*/, 0 /*dataLength*/,
                        &msgEos /*pMsg*/,
                        sizeof(SLuint32)*2 /*msgLength*/);
    }

    return SL_RESULT_SUCCESS;
}


//-----------------------------------------------------------------

/* Play some music from a URI  */
void TestPlayStream( SLObjectItf sl, const char* path)
{
    SLEngineItf                EngineItf;

    SLint32                    numOutputs = 0;
    SLuint32                   deviceID = 0;

    SLresult                   res;

    SLDataSource               audioSource;
    SLDataLocator_AndroidBufferQueue streamLocator;
    SLDataFormat_MIME          mime;

    SLDataSink                 audioSink;
    SLDataLocator_OutputMix    locator_outputmix;

    SLObjectItf                player;
    SLPlayItf                  playItf;
    SLVolumeItf                volItf;
    SLAndroidBufferQueueItf    abqItf;

    SLObjectItf                OutputMix;

    SLboolean required[MAX_NUMBER_INTERFACES];
    SLInterfaceID iidArray[MAX_NUMBER_INTERFACES];

    int playTimeInSec = 90;

    file = fopen(path, "rb");

    /* Get the SL Engine Interface which is implicit */
    res = (*sl)->GetInterface(sl, SL_IID_ENGINE, (void*)&EngineItf);
    CheckErr(res);

    /* Initialize arrays required[] and iidArray[] */
    for (int i=0 ; i < MAX_NUMBER_INTERFACES ; i++) {
        required[i] = SL_BOOLEAN_FALSE;
        iidArray[i] = SL_IID_NULL;
    }

    // Set arrays required[] and iidArray[] for VOLUME and PREFETCHSTATUS interface
    required[0] = SL_BOOLEAN_TRUE;
    iidArray[0] = SL_IID_VOLUME;
    required[1] = SL_BOOLEAN_TRUE;
    iidArray[1] = SL_IID_ANDROIDBUFFERQUEUE;
    // Create Output Mix object to be used by player
    res = (*EngineItf)->CreateOutputMix(EngineItf, &OutputMix, 0,
            iidArray, required); CheckErr(res);

    // Realizing the Output Mix object in synchronous mode.
    res = (*OutputMix)->Realize(OutputMix, SL_BOOLEAN_FALSE);
    CheckErr(res);

    /* Setup the data source structure for the URI */
    streamLocator.locatorType  = SL_DATALOCATOR_ANDROIDBUFFERQUEUE;
    streamLocator.numBuffers   = NB_BUFFERS;
    mime.formatType    = SL_DATAFORMAT_MIME;
    mime.mimeType      = (SLchar *) "video/mp2ts";//(SLchar*)NULL;
    mime.containerType = SL_CONTAINERTYPE_MPEG_TS;

    audioSource.pFormat      = (void *)&mime;
    audioSource.pLocator     = (void *)&streamLocator;

    /* Setup the data sink structure */
    locator_outputmix.locatorType   = SL_DATALOCATOR_OUTPUTMIX;
    locator_outputmix.outputMix    = OutputMix;
    audioSink.pLocator           = (void *)&locator_outputmix;
    audioSink.pFormat            = NULL;

    /* Create the audio player */
    res = (*EngineItf)->CreateAudioPlayer(EngineItf, &player, &audioSource, &audioSink,
            MAX_NUMBER_INTERFACES, iidArray, required); CheckErr(res);

    /* Realizing the player in synchronous mode. */
    res = (*player)->Realize(player, SL_BOOLEAN_FALSE); CheckErr(res);
    fprintf(stdout, "URI example: after Realize\n");

    /* Get interfaces */
    res = (*player)->GetInterface(player, SL_IID_PLAY, (void*)&playItf); CheckErr(res);

    res = (*player)->GetInterface(player, SL_IID_VOLUME,  (void*)&volItf); CheckErr(res);

    res = (*player)->GetInterface(player, SL_IID_ANDROIDBUFFERQUEUE, (void*)&abqItf);
    CheckErr(res);

    res = (*abqItf)->RegisterCallback(abqItf, AndroidBufferQueueCallback,
            // context is not used in the example, but can be used to track who registered
            // the buffer queue callback
            NULL /*pContext*/); CheckErr(res);

    /* Display duration */
    SLmillisecond durationInMsec = SL_TIME_UNKNOWN;
    res = (*playItf)->GetDuration(playItf, &durationInMsec);
    CheckErr(res);
    if (durationInMsec == SL_TIME_UNKNOWN) {
        fprintf(stdout, "Content duration is unknown (before starting to prefetch)\n");
    } else {
        fprintf(stdout, "Content duration is %lu ms (before starting to prefetch)\n",
                durationInMsec);
    }

    /* Set the player volume */
    res = (*volItf)->SetVolumeLevel( volItf, 0);//-300);
    CheckErr(res);


    /* Play the URI */
    /*     first cause the player to prefetch the data */
    fprintf(stdout, "Before set to PAUSED\n");
    res = (*playItf)->SetPlayState( playItf, SL_PLAYSTATE_PAUSED );
    fprintf(stdout, "After set to PAUSED\n");
    CheckErr(res);


    /* Fill our cache */
    if (fread(dataCache, 1, BUFFER_SIZE * NB_BUFFERS, file) <= 0) {
        fprintf(stderr, "Error filling cache, exiting\n");
        goto destroyRes;
    }
    /* Enqueue the content of our cache before starting to play,
         * we don't want to starve the player */
    for (int i=0 ; i < NB_BUFFERS ; i++) {
        res = (*abqItf)->Enqueue(abqItf, dataCache + i*BUFFER_SIZE, BUFFER_SIZE, NULL, 0);
        CheckErr(res);
    }




    /*     wait until there's data to play */
    //SLpermille fillLevel = 0;
 /*
    SLuint32 prefetchStatus = SL_PREFETCHSTATUS_UNDERFLOW;
    SLuint32 timeOutIndex = 2;
    while ((prefetchStatus != SL_PREFETCHSTATUS_SUFFICIENTDATA) && (timeOutIndex > 0) &&
            !prefetchError) {
        usleep(1 * 1000 * 1000); // 1s
        //(*prefetchItf)->GetPrefetchStatus(prefetchItf, &prefetchStatus);
        timeOutIndex--;
    }

    if (timeOutIndex == 0 || prefetchError) {
        fprintf(stderr, "We\'re done waiting, failed to prefetch data in time, exiting\n");
        goto destroyRes;
    }
*/

    /* Display duration again, */
/*    res = (*playItf)->GetDuration(playItf, &durationInMsec);
    CheckErr(res);
    if (durationInMsec == SL_TIME_UNKNOWN) {
        fprintf(stdout, "Content duration is unknown (after prefetch completed)\n");
    } else {
        fprintf(stdout, "Content duration is %lu ms (after prefetch completed)\n", durationInMsec);
    }
*/

    fprintf(stdout, "URI example: starting to play\n");
    res = (*playItf)->SetPlayState( playItf, SL_PLAYSTATE_PLAYING );
    CheckErr(res);

    /* Wait as long as the duration of the content before stopping */
    fprintf(stdout, "Letting playback go on for %d sec\n", playTimeInSec);
    usleep(playTimeInSec /*s*/ * 1000 * 1000);


    /* Make sure player is stopped */
    fprintf(stdout, "URI example: stopping playback\n");
    res = (*playItf)->SetPlayState(playItf, SL_PLAYSTATE_STOPPED);
    CheckErr(res);

    fprintf(stdout, "sleeping to verify playback stopped\n");
    usleep(2 /*s*/ * 1000 * 1000);

destroyRes:

    /* Destroy the player */
    (*player)->Destroy(player);

    /* Destroy Output Mix object */
    (*OutputMix)->Destroy(OutputMix);

    fclose(file);
}

//-----------------------------------------------------------------
int main(int argc, char* const argv[])
{
    SLresult    res;
    SLObjectItf sl;

    fprintf(stdout, "OpenSL ES test %s: exercises SLPlayItf, SLVolumeItf, SLAndroidBufferQueue \n",
            argv[0]);
    fprintf(stdout, "and AudioPlayer with SL_DATALOCATOR_ANDROIDBUFFERQUEUE source / OutputMix sink\n");
    fprintf(stdout, "Plays a sound and stops after its reported duration\n\n");

    if (argc == 1) {
        fprintf(stdout, "Usage: %s path \n\t%s url\n", argv[0], argv[0]);
        fprintf(stdout, "Example: \"%s /sdcard/my.mp3\"  or \"%s file:///sdcard/my.mp3\"\n",
                argv[0], argv[0]);
        exit(EXIT_FAILURE);
    }

    SLEngineOption EngineOption[] = {
            {(SLuint32) SL_ENGINEOPTION_THREADSAFE,
            (SLuint32) SL_BOOLEAN_TRUE}};

    res = slCreateEngine( &sl, 1, EngineOption, 0, NULL, NULL);
    CheckErr(res);
    /* Realizing the SL Engine in synchronous mode. */
    res = (*sl)->Realize(sl, SL_BOOLEAN_FALSE);
    CheckErr(res);

    TestPlayStream(sl, argv[1]);

    /* Shutdown OpenSL ES */
    (*sl)->Destroy(sl);

    return EXIT_SUCCESS;
}
