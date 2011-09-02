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

// OpenMAX AL MediaPlayer command-line player

#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <OMXAL/OpenMAXAL.h>
#include <OMXAL/OpenMAXAL_Android.h>
#include "nativewindow.h"

#define MPEG2TS_PACKET_SIZE 188  // MPEG-2 transport stream packet size in bytes
#define PACKETS_PER_BUFFER 20    // Number of MPEG-2 transport stream packets per buffer

#define NB_BUFFERS 2    // Number of buffers in Android buffer queue

// MPEG-2 transport stream packet
typedef struct {
    char data[MPEG2TS_PACKET_SIZE];
} MPEG2TS_Packet;

#if 0
// Each buffer in Android buffer queue
typedef struct {
    MPEG2TS_Packet packets[PACKETS_PER_BUFFER];
} Buffer;
#endif

// Globals shared between main thread and buffer queue callback
MPEG2TS_Packet *packets;
size_t numPackets;
size_t curPacket;
size_t discPacket;

// These are extensions to OpenMAX AL 1.0.1 values

#define PREFETCHSTATUS_UNKNOWN ((XAuint32) 0)
#define PREFETCHSTATUS_ERROR   ((XAuint32) (-1))

// Mutex and condition shared with main program to protect prefetch_status

static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
XAuint32 prefetch_status = PREFETCHSTATUS_UNKNOWN;

/* used to detect errors likely to have occured when the OpenMAX AL framework fails to open
 * a resource, for instance because a file URI is invalid, or an HTTP server doesn't respond.
 */
#define PREFETCHEVENT_ERROR_CANDIDATE \
        (XA_PREFETCHEVENT_STATUSCHANGE | XA_PREFETCHEVENT_FILLLEVELCHANGE)

// stream event change callback
void streamEventChangeCallback(XAStreamInformationItf caller, XAuint32 eventId,
        XAuint32 streamIndex, void *pEventData, void *pContext)
{
    // context parameter is specified as NULL and is unused here
    assert(NULL == pContext);
    switch (eventId) {
    case XA_STREAMCBEVENT_PROPERTYCHANGE:
        printf("XA_STREAMCBEVENT_PROPERTYCHANGE on stream index %u, pEventData %p\n", streamIndex,
                pEventData);
        break;
    default:
        printf("Unknown stream event ID %u\n", eventId);
        break;
    }
}

// prefetch status callback
void prefetchStatusCallback(XAPrefetchStatusItf caller,  void *pContext, XAuint32 event)
{
    // pContext is unused here, so we pass NULL
    assert(pContext == NULL);
    XApermille level = 0;
    XAresult result;
    result = (*caller)->GetFillLevel(caller, &level);
    assert(XA_RESULT_SUCCESS == result);
    XAuint32 status;
    result = (*caller)->GetPrefetchStatus(caller, &status);
    assert(XA_RESULT_SUCCESS == result);
    if (event & XA_PREFETCHEVENT_FILLLEVELCHANGE) {
        printf("PrefetchEventCallback: Buffer fill level is = %d\n", level);
    }
    if (event & XA_PREFETCHEVENT_STATUSCHANGE) {
        printf("PrefetchEventCallback: Prefetch Status is = %u\n", status);
    }
    XAuint32 new_prefetch_status;
    if ((PREFETCHEVENT_ERROR_CANDIDATE == (event & PREFETCHEVENT_ERROR_CANDIDATE))
            && (level == 0) && (status == XA_PREFETCHSTATUS_UNDERFLOW)) {
        printf("PrefetchEventCallback: Error while prefetching data, exiting\n");
        new_prefetch_status = PREFETCHSTATUS_ERROR;
    } else if (event == XA_PREFETCHEVENT_STATUSCHANGE) {
        new_prefetch_status = status;
    } else {
        return;
    }
    int ok;
    ok = pthread_mutex_lock(&mutex);
    assert(ok == 0);
    prefetch_status = new_prefetch_status;
    ok = pthread_cond_signal(&cond);
    assert(ok == 0);
    ok = pthread_mutex_unlock(&mutex);
    assert(ok == 0);
}

// playback event callback
void playEventCallback(XAPlayItf caller, void *pContext, XAuint32 event)
{
    // pContext is unused here, so we pass NULL
    assert(NULL == pContext);

    XAmillisecond position;

    if (XA_PLAYEVENT_HEADATEND & event) {
        printf("XA_PLAYEVENT_HEADATEND reached\n");
        //SignalEos();
    }

    XAresult result;
    if (XA_PLAYEVENT_HEADATNEWPOS & event) {
        result = (*caller)->GetPosition(caller, &position);
        assert(XA_RESULT_SUCCESS == result);
        printf("XA_PLAYEVENT_HEADATNEWPOS current position=%ums\n", position);
    }

    if (XA_PLAYEVENT_HEADATMARKER & event) {
        result = (*caller)->GetPosition(caller, &position);
        assert(XA_RESULT_SUCCESS == result);
        printf("XA_PLAYEVENT_HEADATMARKER current position=%ums\n", position);
    }
}

// Android buffer queue callback
XAresult bufferQueueCallback(
        XAAndroidBufferQueueItf caller,
        void *pCallbackContext,
        void *pBufferContext,
        void *pBufferData,
        XAuint32 dataSize,
        XAuint32 dataUsed,
        const XAAndroidBufferItem *pItems,
        XAuint32 itemsLength)
{
    // enqueue the .ts data directly from mapped memory, so ignore the empty buffer pBufferData
    if (curPacket <= numPackets) {
        static const XAAndroidBufferItem discontinuity = {XA_ANDROID_ITEMKEY_DISCONTINUITY, 0};
        static const XAAndroidBufferItem eos = {XA_ANDROID_ITEMKEY_EOS, 0};
        const XAAndroidBufferItem *items;
        XAuint32 itemSize;
        // compute number of packets to be enqueued in this buffer
        XAuint32 packetsThisBuffer = numPackets - curPacket;
        if (packetsThisBuffer > PACKETS_PER_BUFFER) {
            packetsThisBuffer = PACKETS_PER_BUFFER;
        }
        // last packet? this should only happen once
        if (curPacket == numPackets) {
            (void) write(1, "e", 1);
            items = &eos;
            itemSize = sizeof(eos);
        // discontinuity requested?
        } else if (curPacket == discPacket) {
            printf("sending discontinuity, rewinding from beginning of stream\n");
            items = &discontinuity;
            itemSize = sizeof(discontinuity);
            curPacket = 0;
        // pure data with no items
        } else {
            items = NULL;
            itemSize = 0;
        }
        XAresult result;
        // enqueue the optional data and optional items; there is always at least one or the other
        assert(packetsThisBuffer > 0 || itemSize > 0);
        result = (*caller)->Enqueue(caller, NULL, &packets[curPacket],
                sizeof(MPEG2TS_Packet) * packetsThisBuffer, items, itemSize);
        assert(XA_RESULT_SUCCESS == result);
        curPacket += packetsThisBuffer;
    }
    return XA_RESULT_SUCCESS;
}

// main program
int main(int argc, char **argv)
{
    const char *prog = argv[0];
    int i;

    XAboolean abq = XA_BOOLEAN_FALSE;   // use AndroidBufferQueue, default is URI
    XAboolean looping = XA_BOOLEAN_FALSE;
#ifdef REINITIALIZE
    int reinit_counter = 0;
#endif
    for (i = 1; i < argc; ++i) {
        const char *arg = argv[i];
        if (arg[0] != '-')
            break;
        switch (arg[1]) {
        case 'a':
            abq = XA_BOOLEAN_TRUE;
            break;
        case 'd':
            discPacket = atoi(&arg[2]);
            break;
        case 'l':
            looping = XA_BOOLEAN_TRUE;
            break;
#ifdef REINITIALIZE
        case 'r':
            reinit_counter = atoi(&arg[2]);
            break;
#endif
        default:
            fprintf(stderr, "%s: unknown option %s\n", prog, arg);
            break;
        }
    }

    // check that exactly one URI was specified
    if (argc - i != 1) {
        fprintf(stderr, "usage: %s [-a] [-d#] [-l] uri\n", prog);
        return EXIT_FAILURE;
    }
    const char *uri = argv[i];

    // for AndroidBufferQueue, interpret URI as a filename and open
    int fd = -1;
    if (abq) {
        fd = open(uri, O_RDONLY);
        if (fd < 0) {
            perror(uri);
            goto close;
        }
        int ok;
        struct stat statbuf;
        ok = fstat(fd, &statbuf);
        if (ok < 0) {
            perror(uri);
            goto close;
        }
        if (!S_ISREG(statbuf.st_mode)) {
            fprintf(stderr, "%s: not an ordinary file\n", uri);
            goto close;
        }
        void *ptr;
        ptr = mmap(NULL, statbuf.st_size, PROT_READ, MAP_FILE | MAP_PRIVATE, fd, (off_t) 0);
        if (ptr == MAP_FAILED) {
            perror(uri);
            goto close;
        }
        size_t filelen = statbuf.st_size;
        if ((filelen % MPEG2TS_PACKET_SIZE) != 0) {
            fprintf(stderr, "%s: warning file length %zu is not a multiple of %d\n", uri, filelen,
                    MPEG2TS_PACKET_SIZE);
        }
        packets = (MPEG2TS_Packet *) ptr;
        numPackets = filelen / MPEG2TS_PACKET_SIZE;
        printf("%s has %zu packets\n", uri, numPackets);
    }

    ANativeWindow *nativeWindow;

#ifdef REINITIALIZE
reinitialize:    ;
#endif

    XAresult result;
    XAObjectItf engineObject;

    // create engine
    result = xaCreateEngine(&engineObject, 0, NULL, 0, NULL, NULL);
    assert(XA_RESULT_SUCCESS == result);
    result = (*engineObject)->Realize(engineObject, XA_BOOLEAN_FALSE);
    assert(XA_RESULT_SUCCESS == result);
    XAEngineItf engineEngine;
    result = (*engineObject)->GetInterface(engineObject, XA_IID_ENGINE, &engineEngine);
    assert(XA_RESULT_SUCCESS == result);

    // create output mix
    XAObjectItf outputMixObject;
    result = (*engineEngine)->CreateOutputMix(engineEngine, &outputMixObject, 0, NULL, NULL);
    assert(XA_RESULT_SUCCESS == result);
    result = (*outputMixObject)->Realize(outputMixObject, XA_BOOLEAN_FALSE);
    assert(XA_RESULT_SUCCESS == result);

    // configure media source
    XADataLocator_URI locUri;
    locUri.locatorType = XA_DATALOCATOR_URI;
    locUri.URI = (XAchar *) uri;
    XADataFormat_MIME fmtMime;
    fmtMime.formatType = XA_DATAFORMAT_MIME;
    if (abq) {
        fmtMime.mimeType = (XAchar *) "video/mp2ts";
        fmtMime.containerType = XA_CONTAINERTYPE_MPEG_TS;
    } else {
        fmtMime.mimeType = NULL;
        fmtMime.containerType = XA_CONTAINERTYPE_UNSPECIFIED;
    }
    XADataLocator_AndroidBufferQueue locABQ;
    locABQ.locatorType = XA_DATALOCATOR_ANDROIDBUFFERQUEUE;
    locABQ.numBuffers = NB_BUFFERS;
    XADataSource dataSrc;
    if (abq) {
        dataSrc.pLocator = &locABQ;
    } else {
        dataSrc.pLocator = &locUri;
    }
    dataSrc.pFormat = &fmtMime;

    // configure audio sink
    XADataLocator_OutputMix locOM;
    locOM.locatorType = XA_DATALOCATOR_OUTPUTMIX;
    locOM.outputMix = outputMixObject;
    XADataSink audioSnk;
    audioSnk.pLocator = &locOM;
    audioSnk.pFormat = NULL;

    // configure video sink
    nativeWindow = getNativeWindow();
    XADataLocator_NativeDisplay locND;
    locND.locatorType = XA_DATALOCATOR_NATIVEDISPLAY;
    locND.hWindow = nativeWindow;
    locND.hDisplay = NULL;
    XADataSink imageVideoSink;
    imageVideoSink.pLocator = &locND;
    imageVideoSink.pFormat = NULL;

    // create media player
    XAObjectItf playerObject;
    XAInterfaceID ids[4] = {XA_IID_STREAMINFORMATION, XA_IID_PREFETCHSTATUS, XA_IID_SEEK,
            XA_IID_ANDROIDBUFFERQUEUESOURCE};
    XAboolean req[4] = {XA_BOOLEAN_TRUE, XA_BOOLEAN_TRUE, XA_BOOLEAN_TRUE, XA_BOOLEAN_TRUE};
    result = (*engineEngine)->CreateMediaPlayer(engineEngine, &playerObject, &dataSrc, NULL,
            &audioSnk, nativeWindow != NULL ? &imageVideoSink : NULL, NULL, NULL, abq ? 4 : 3, ids,
            req);
    assert(XA_RESULT_SUCCESS == result);

    // realize the player
    result = (*playerObject)->Realize(playerObject, XA_BOOLEAN_FALSE);
    assert(XA_RESULT_SUCCESS == result);

    if (abq) {

        // get the Android buffer queue interface
        XAAndroidBufferQueueItf playerAndroidBufferQueue;
        result = (*playerObject)->GetInterface(playerObject, XA_IID_ANDROIDBUFFERQUEUESOURCE,
                &playerAndroidBufferQueue);
        assert(XA_RESULT_SUCCESS == result);

        // register the buffer queue callback
        result = (*playerAndroidBufferQueue)->RegisterCallback(playerAndroidBufferQueue,
                bufferQueueCallback, NULL);
        assert(XA_RESULT_SUCCESS == result);
        result = (*playerAndroidBufferQueue)->SetCallbackEventsMask(playerAndroidBufferQueue,
                XA_ANDROIDBUFFERQUEUEEVENT_PROCESSED);
        assert(XA_RESULT_SUCCESS == result);

        // enqueue the initial buffers until buffer queue is full
        XAuint32 packetsThisBuffer;
        for (curPacket = 0; curPacket < numPackets; curPacket += packetsThisBuffer) {
            // handle the unlikely case of a very short .ts
            packetsThisBuffer = numPackets - curPacket;
            if (packetsThisBuffer > PACKETS_PER_BUFFER) {
                packetsThisBuffer = PACKETS_PER_BUFFER;
            }
            result = (*playerAndroidBufferQueue)->Enqueue(playerAndroidBufferQueue, NULL,
                    &packets[curPacket], MPEG2TS_PACKET_SIZE * packetsThisBuffer, NULL, 0);
            if (XA_RESULT_BUFFER_INSUFFICIENT == result) {
                printf("Enqueued initial %u packets in %u buffers\n", curPacket, curPacket / PACKETS_PER_BUFFER);
                break;
            }
            assert(XA_RESULT_SUCCESS == result);
        }

    }

    // get the stream information interface
    XAStreamInformationItf playerStreamInformation;
    result = (*playerObject)->GetInterface(playerObject, XA_IID_STREAMINFORMATION,
            &playerStreamInformation);
    assert(XA_RESULT_SUCCESS == result);

    // register the stream event change callback
    result = (*playerStreamInformation)->RegisterStreamChangeCallback(playerStreamInformation,
            streamEventChangeCallback, NULL);
    assert(XA_RESULT_SUCCESS == result);

    // get the prefetch status interface
    XAPrefetchStatusItf playerPrefetchStatus;
    result = (*playerObject)->GetInterface(playerObject, XA_IID_PREFETCHSTATUS,
            &playerPrefetchStatus);
    assert(XA_RESULT_SUCCESS == result);

    // register prefetch status callback
    result = (*playerPrefetchStatus)->RegisterCallback(playerPrefetchStatus, prefetchStatusCallback,
            NULL);
    assert(XA_RESULT_SUCCESS == result);
    result = (*playerPrefetchStatus)->SetCallbackEventsMask(playerPrefetchStatus,
            XA_PREFETCHEVENT_FILLLEVELCHANGE | XA_PREFETCHEVENT_STATUSCHANGE);
    assert(XA_RESULT_SUCCESS == result);

    // get the seek interface
    if (looping) {
        XASeekItf playerSeek;
        result = (*playerObject)->GetInterface(playerObject, XA_IID_SEEK, &playerSeek);
        assert(XA_RESULT_SUCCESS == result);
        result = (*playerSeek)->SetLoop(playerSeek, XA_BOOLEAN_TRUE, (XAmillisecond) 0,
                XA_TIME_UNKNOWN);
        assert(XA_RESULT_SUCCESS == result);
    }

    // get the play interface
    XAPlayItf playerPlay;
    result = (*playerObject)->GetInterface(playerObject, XA_IID_PLAY, &playerPlay);
    assert(XA_RESULT_SUCCESS == result);

    // register play event callback
    result = (*playerPlay)->RegisterCallback(playerPlay, playEventCallback, NULL);
    assert(XA_RESULT_SUCCESS == result);
#if 0 // FIXME broken
    result = (*playerPlay)->SetCallbackEventsMask(playerPlay,
            XA_PLAYEVENT_HEADATEND | XA_PLAYEVENT_HEADATMARKER | XA_PLAYEVENT_HEADATNEWPOS);
    assert(XA_RESULT_SUCCESS == result);
#endif

    // set a marker
    result = (*playerPlay)->SetMarkerPosition(playerPlay, 10000);
    assert(XA_RESULT_SUCCESS == result);

    // set position update period
    result = (*playerPlay)->SetPositionUpdatePeriod(playerPlay, 1000);
    assert(XA_RESULT_SUCCESS == result);

    // get the duration
    XAmillisecond duration;
    result = (*playerPlay)->GetDuration(playerPlay, &duration);
    assert(XA_RESULT_SUCCESS == result);
    if (XA_TIME_UNKNOWN == duration)
        printf("Duration: unknown\n");
    else
        printf("Duration: %.1f\n", duration / 1000.0f);

    // set the player's state to paused, to start prefetching
    printf("start prefetch\n");
    result = (*playerPlay)->SetPlayState(playerPlay, XA_PLAYSTATE_PAUSED);
    assert(XA_RESULT_SUCCESS == result);

    // wait for prefetch status callback to indicate either sufficient data or error
    pthread_mutex_lock(&mutex);
    while (prefetch_status == PREFETCHSTATUS_UNKNOWN) {
        pthread_cond_wait(&cond, &mutex);
    }
    pthread_mutex_unlock(&mutex);
    if (prefetch_status == PREFETCHSTATUS_ERROR) {
        fprintf(stderr, "Error during prefetch, exiting\n");
        goto destroyRes;
    }

    // get duration again, now it should be known
    result = (*playerPlay)->GetDuration(playerPlay, &duration);
    assert(XA_RESULT_SUCCESS == result);
    if (duration == XA_TIME_UNKNOWN) {
        fprintf(stdout, "Content duration is unknown (after prefetch completed)\n");
    } else {
        fprintf(stdout, "Content duration is %u ms (after prefetch completed)\n", duration);
    }

    // start playing
    printf("starting to play\n");
    result = (*playerPlay)->SetPlayState(playerPlay, XA_PLAYSTATE_PLAYING);
    assert(XA_RESULT_SUCCESS == result);

    // continue playing until end of media
    for (;;) {
        XAuint32 status;
        result = (*playerPlay)->GetPlayState(playerPlay, &status);
        assert(XA_RESULT_SUCCESS == result);
        if (status == XA_PLAYSTATE_PAUSED)
            break;
        assert(status == XA_PLAYSTATE_PLAYING);
        sleep(1);
    }

    // wait a bit more in case of additional callbacks
    printf("end of media\n");
    sleep(3);

destroyRes:

    // destroy the player
    (*playerObject)->Destroy(playerObject);

    // destroy the output mix
    (*outputMixObject)->Destroy(outputMixObject);

    // destroy the engine
    (*engineObject)->Destroy(engineObject);

#ifdef REINITIALIZE
    if (--reinit_count > 0) {
        prefetch_status = PREFETCHSTATUS_UNKNOWN;
        goto reinitialize;
    }
#endif

#if 0
    if (nativeWindow != NULL) {
        ANativeWindow_release(nativeWindow);
    }
#endif

close:
    if (fd >= 0) {
        (void) close(fd);
    }

    return EXIT_SUCCESS;
}
