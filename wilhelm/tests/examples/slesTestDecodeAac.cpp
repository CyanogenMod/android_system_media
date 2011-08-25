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

/* AAC ADTS Decode Test

First run the program from shell:
  # slesTestDecodeAac /sdcard/myFile.adts

These use adb on host to retrieve the decoded file:
  % adb pull /sdcard/myFile.adts.raw myFile.raw

How to examine the output with Audacity:
 Project / Import raw data
 Select myFile.raw file, then click Open button
 Choose these options:
  Signed 16-bit PCM
  Little-endian
  1 Channel (Mono) / 2 Channels (Stereo) based on the PCM information obtained when decoding
  Sample rate based on the PCM information obtained when decoding
 Click Import button

*/

#define QUERY_METADATA

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>
#include <fcntl.h>
#include <pthread.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include <SLES/OpenSLES.h>
#include <SLES/OpenSLES_Android.h>

/* Explicitly requesting SL_IID_ANDROIDBUFFERQUEUE and SL_IID_ANDROIDSIMPLEBUFFERQUEUE
 * on the AudioPlayer object for decoding, and
 * SL_IID_METADATAEXTRACTION for retrieving the format of the decoded audio.
 */
#define NUM_EXPLICIT_INTERFACES_FOR_PLAYER 3

/* Number of decoded samples produced by one AAC frame; defined by the standard */
#define SAMPLES_PER_AAC_FRAME 1024
/* Size of the encoded AAC ADTS buffer queue */
#define NB_BUFFERS_IN_ADTS_QUEUE 2 // 2 to 4 is typical

/* Size of the decoded PCM buffer queue */
#define NB_BUFFERS_IN_PCM_QUEUE 2  // 2 to 4 is typical
/* Size of each PCM buffer in the queue */
#define BUFFER_SIZE_IN_BYTES   (2*sizeof(short)*SAMPLES_PER_AAC_FRAME)

/* Local storage for decoded PCM audio data */
int8_t pcmData[NB_BUFFERS_IN_PCM_QUEUE * BUFFER_SIZE_IN_BYTES];

/* destination for decoded data */
static FILE* outputFp;

#ifdef QUERY_METADATA
/* metadata key index for the PCM format information we want to retrieve */
static int channelCountKeyIndex = -1;
static int sampleRateKeyIndex = -1;
/* size of the struct to retrieve the PCM format metadata values: the values we're interested in
 * are SLuint32, but it is saved in the data field of a SLMetadataInfo, hence the larger size.
 * Note that this size is queried and displayed at l.XXX for demonstration/test purposes.
 *  */
#define PCM_METADATA_VALUE_SIZE 32
/* used to query metadata values */
static SLMetadataInfo *pcmMetaData = NULL;
/* we only want to query / display the PCM format once */
static bool formatQueried = false;
#endif

/* to signal to the test app that the end of the encoded ADTS stream has been reached */
bool eos = false;
bool endOfEncodedStream = false;

void *ptr;
unsigned char *frame;
size_t filelen;
size_t encodedFrames = 0;
size_t encodedSamples = 0;
size_t decodedFrames = 0;
size_t decodedSamples = 0;

/* protects shared variables */
pthread_mutex_t eosLock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t eosCondition = PTHREAD_COND_INITIALIZER;

//-----------------------------------------------------------------
/* Exits the application if an error is encountered */
#define ExitOnError(x) ExitOnErrorFunc(x,__LINE__)

void ExitOnErrorFunc( SLresult result , int line)
{
    if (SL_RESULT_SUCCESS != result) {
        fprintf(stderr, "Error code %u encountered at line %d, exiting\n", result, line);
        exit(EXIT_FAILURE);
    }
}

//-----------------------------------------------------------------
/* Structure for passing information to callback function */
typedef struct CallbackCntxt_ {
#ifdef QUERY_METADATA
    SLMetadataExtractionItf metaItf;
#endif
    SLint8*   pDataBase;    // Base address of local audio data storage
    SLint8*   pData;        // Current address of local audio data storage
} CallbackCntxt;

//-----------------------------------------------------------------
/* Callback for AndroidBufferQueueItf through which we supply ADTS buffers */
SLresult AndroidBufferQueueCallback(
        SLAndroidBufferQueueItf caller,
        void *pCallbackContext,        /* input */
        void *pBufferContext,          /* input */
        void *pBufferData,             /* input */
        SLuint32 dataSize,             /* input */
        SLuint32 dataUsed,             /* input */
        const SLAndroidBufferItem *pItems,/* input */
        SLuint32 itemsLength           /* input */)
{
    // mutex on all global variables
    pthread_mutex_lock(&eosLock);
    SLresult res;

    if (endOfEncodedStream) {
        // we continue to receive acknowledgement after each buffer was processed
    } else if (filelen == 0) {
        // signal EOS to the decoder rather than just starving it
        SLAndroidBufferItem msgEos;
        msgEos.itemKey = SL_ANDROID_ITEMKEY_EOS;
        msgEos.itemSize = 0;
        // EOS message has no parameters, so the total size of the message is the size of the key
        //   plus the size of itemSize, both SLuint32
        res = (*caller)->Enqueue(caller, NULL /*pBufferContext*/,
                NULL /*pData*/, 0 /*dataLength*/,
                &msgEos /*pMsg*/,
                sizeof(SLuint32)*2 /*msgLength*/);
        ExitOnError(res);
        endOfEncodedStream = true;
    // verify that we are at start of an ADTS frame
    } else if (!(filelen < 7 || frame[0] != 0xFF || (frame[1] & 0xF0) != 0xF0)) {
        unsigned framelen = ((frame[3] & 3) << 11) | (frame[4] << 3) | (frame[5] >> 5);
        if (framelen <= filelen) {
            // push more data to the queue
            res = (*caller)->Enqueue(caller, NULL /*pBufferContext*/,
                    frame, framelen, NULL, 0);
            ExitOnError(res);
            frame += framelen;
            filelen -= framelen;
            ++encodedFrames;
            encodedSamples += SAMPLES_PER_AAC_FRAME;
        } else {
            fprintf(stderr,
                    "partial ADTS frame at EOF discarded; offset=%u, framelen=%u, filelen=%u\n",
                    frame - (unsigned char *) ptr, framelen, filelen);
            frame += filelen;
            filelen = 0;
        }
    } else {
        fprintf(stderr, "corrupt ADTS frame encountered; offset=%u, filelen=%u\n",
                frame - (unsigned char *) ptr, filelen);
        frame += filelen;
        filelen = 0;
    }
    pthread_mutex_unlock(&eosLock);

    return SL_RESULT_SUCCESS;
}

//-----------------------------------------------------------------
/* Callback for decoding buffer queue events */
void DecPlayCallback(
        SLAndroidSimpleBufferQueueItf queueItf,
        void *pContext)
{
    // mutex on all global variables
    pthread_mutex_lock(&eosLock);

    CallbackCntxt *pCntxt = (CallbackCntxt*)pContext;

    /* Save the decoded data to output file */
    if (fwrite(pCntxt->pData, 1, BUFFER_SIZE_IN_BYTES, outputFp) < BUFFER_SIZE_IN_BYTES) {
        fprintf(stderr, "Error writing to output file");
    }

    /* Re-enqueue the now empty buffer */
    SLresult res;
    res = (*queueItf)->Enqueue(queueItf, pCntxt->pData, BUFFER_SIZE_IN_BYTES);
    ExitOnError(res);

    /* Increase data pointer by buffer size, with circular wraparound */
    pCntxt->pData += BUFFER_SIZE_IN_BYTES;
    if (pCntxt->pData >= pCntxt->pDataBase + (NB_BUFFERS_IN_PCM_QUEUE * BUFFER_SIZE_IN_BYTES)) {
        pCntxt->pData = pCntxt->pDataBase;
    }

    // Note: adding a sleep here or any sync point is a way to slow down the decoding, or
    //  synchronize it with some other event, as the OpenSL ES framework will block until the
    //  buffer queue callback return to proceed with the decoding.

#ifdef QUERY_METADATA
    /* Example: query of the decoded PCM format */
    if (!formatQueried) {
        /* memory to receive the PCM format metadata */
        union {
            SLMetadataInfo pcmMetaData;
            char withData[PCM_METADATA_VALUE_SIZE];
        } u;
        res = (*pCntxt->metaItf)->GetValue(pCntxt->metaItf, sampleRateKeyIndex,
                PCM_METADATA_VALUE_SIZE, &u.pcmMetaData);  ExitOnError(res);
        // Note: here we could verify the following:
        //         u.pcmMetaData->encoding == SL_CHARACTERENCODING_BINARY
        //         u.pcmMetaData->size == sizeof(SLuint32)
        //      but the call was successful for the PCM format keys, so those conditions are implied
        printf("sample rate = %dHz, ", *((SLuint32*)u.pcmMetaData.data));
        res = (*pCntxt->metaItf)->GetValue(pCntxt->metaItf, channelCountKeyIndex,
                PCM_METADATA_VALUE_SIZE, &u.pcmMetaData);  ExitOnError(res);
        printf(" channel count = %d\n", *((SLuint32*)u.pcmMetaData.data));
        formatQueried = true;
    }
#endif

    ++decodedFrames;
    decodedSamples += SAMPLES_PER_AAC_FRAME;

    if (endOfEncodedStream && decodedSamples >= encodedSamples) {
        eos = true;
        pthread_cond_signal(&eosCondition);
    }
    pthread_mutex_unlock(&eosLock);
}

//-----------------------------------------------------------------

/* Decode an audio path by opening a file descriptor on that path  */
void TestDecToBuffQueue( SLObjectItf sl, const char *path, int fd)
{
    // check what kind of object it is
    int ok;
    struct stat statbuf;
    ok = fstat(fd, &statbuf);
    if (ok < 0) {
        perror(path);
        return;
    }

    // verify that's it is a file
    if (!S_ISREG(statbuf.st_mode)) {
        fprintf(stderr, "%s: not an ordinary file\n", path);
        return;
    }

    // map file contents into memory to make it easier to access the ADTS frames directly
    ptr = mmap(NULL, statbuf.st_size, PROT_READ, MAP_FILE | MAP_PRIVATE, fd, (off_t) 0);
    if (ptr == MAP_FAILED) {
        perror(path);
        return;
    }
    frame = (unsigned char *) ptr;
    filelen = statbuf.st_size;

    // create PCM .raw file
    size_t len = strlen((const char *) path);
    char* outputPath = (char*) malloc(len + 4 + 1); // save room to concatenate ".raw"
    if (NULL == outputPath) {
        ExitOnError(SL_RESULT_RESOURCE_ERROR);
    }
    memcpy(outputPath, path, len + 1);
    strcat(outputPath, ".raw");
    outputFp = fopen(outputPath, "w");
    if (NULL == outputFp) {
        ExitOnError(SL_RESULT_RESOURCE_ERROR);
    }

    SLresult res;
    SLEngineItf EngineItf;

    /* Objects this application uses: one audio player */
    SLObjectItf  player;

    /* Interfaces for the audio player */
    SLPlayItf                     playItf;
#ifdef QUERY_METADATA
    /*   to retrieve the decoded PCM format */
    SLMetadataExtractionItf       mdExtrItf;
#endif
    /*   to retrieve the PCM samples */
    SLAndroidSimpleBufferQueueItf decBuffQueueItf;
    /*   to queue the AAC data to decode */
    SLAndroidBufferQueueItf       aacBuffQueueItf;

    SLboolean required[NUM_EXPLICIT_INTERFACES_FOR_PLAYER];
    SLInterfaceID iidArray[NUM_EXPLICIT_INTERFACES_FOR_PLAYER];

    /* Get the SL Engine Interface which is implicit */
    res = (*sl)->GetInterface(sl, SL_IID_ENGINE, (void*)&EngineItf);
    ExitOnError(res);

    /* Initialize arrays required[] and iidArray[] */
    unsigned int i;
    for (i=0 ; i < NUM_EXPLICIT_INTERFACES_FOR_PLAYER ; i++) {
        required[i] = SL_BOOLEAN_FALSE;
        iidArray[i] = SL_IID_NULL;
    }

    /* ------------------------------------------------------ */
    /* Configuration of the player  */

    /* Request the AndroidSimpleBufferQueue interface */
    required[0] = SL_BOOLEAN_TRUE;
    iidArray[0] = SL_IID_ANDROIDSIMPLEBUFFERQUEUE;
    /* Request the AndroidBufferQueue interface */
    required[1] = SL_BOOLEAN_TRUE;
    iidArray[1] = SL_IID_ANDROIDBUFFERQUEUE;
#ifdef QUERY_METADATA
    /* Request the MetadataExtraction interface */
    required[2] = SL_BOOLEAN_TRUE;
    iidArray[2] = SL_IID_METADATAEXTRACTION;
#endif

    /* Setup the data source for queueing AAC buffers of ADTS data */
    SLDataLocator_AndroidBufferQueue loc_srcAbq = {
            SL_DATALOCATOR_ANDROIDBUFFERQUEUE /*locatorType*/,
            NB_BUFFERS_IN_ADTS_QUEUE          /*numBuffers*/};
    SLDataFormat_MIME format_srcMime = {
            SL_DATAFORMAT_MIME         /*formatType*/,
            (SLchar *)"audio/aac-adts" /*mimeType*/,
            SL_CONTAINERTYPE_RAW       /*containerType*/};
    SLDataSource decSource = {&loc_srcAbq /*pLocator*/, &format_srcMime /*pFormat*/};

    /* Setup the data sink, a buffer queue for buffers of PCM data */
    SLDataLocator_AndroidSimpleBufferQueue loc_destBq = {
            SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE/*locatorType*/,
            NB_BUFFERS_IN_PCM_QUEUE                /*numBuffers*/ };

    /*    declare we're decoding to PCM, the parameters after that need to be valid,
          but are ignored, the decoded format will match the source */
    SLDataFormat_PCM format_destPcm = { /*formatType*/ SL_DATAFORMAT_PCM, /*numChannels*/ 1,
            /*samplesPerSec*/ SL_SAMPLINGRATE_8, /*pcm.bitsPerSample*/ SL_PCMSAMPLEFORMAT_FIXED_16,
            /*/containerSize*/ 16, /*channelMask*/ SL_SPEAKER_FRONT_LEFT,
            /*endianness*/ SL_BYTEORDER_LITTLEENDIAN };
    SLDataSink decDest = {&loc_destBq /*pLocator*/, &format_destPcm /*pFormat*/};

    /* Create the audio player */
    res = (*EngineItf)->CreateAudioPlayer(EngineItf, &player, &decSource, &decDest,
#ifdef QUERY_METADATA
            NUM_EXPLICIT_INTERFACES_FOR_PLAYER,
#else
            NUM_EXPLICIT_INTERFACES_FOR_PLAYER - 1,
#endif
            iidArray, required);
    ExitOnError(res);
    printf("Player created\n");

    /* Realize the player in synchronous mode. */
    res = (*player)->Realize(player, SL_BOOLEAN_FALSE);
    ExitOnError(res);
    printf("Player realized\n");

    /* Get the play interface which is implicit */
    res = (*player)->GetInterface(player, SL_IID_PLAY, (void*)&playItf);
    ExitOnError(res);

    /* Get the buffer queue interface which was explicitly requested */
    res = (*player)->GetInterface(player, SL_IID_ANDROIDSIMPLEBUFFERQUEUE, (void*)&decBuffQueueItf);
    ExitOnError(res);

    /* Get the Android buffer queue interface which was explicitly requested */
    res = (*player)->GetInterface(player, SL_IID_ANDROIDBUFFERQUEUE, (void*)&aacBuffQueueItf);
    ExitOnError(res);

#ifdef QUERY_METADATA
    /* Get the metadata extraction interface which was explicitly requested */
    res = (*player)->GetInterface(player, SL_IID_METADATAEXTRACTION, (void*)&mdExtrItf);
    ExitOnError(res);
#endif

    /* ------------------------------------------------------ */
    /* Initialize the callback and its context for the buffer queue of the decoded PCM */
    CallbackCntxt sinkCntxt;
#ifdef QUERY_METADATA
    sinkCntxt.metaItf = mdExtrItf;
#endif
    sinkCntxt.pDataBase = (int8_t*)&pcmData;
    sinkCntxt.pData = sinkCntxt.pDataBase;
    res = (*decBuffQueueItf)->RegisterCallback(decBuffQueueItf, DecPlayCallback, &sinkCntxt);
    ExitOnError(res);

    /* Enqueue buffers to map the region of memory allocated to store the decoded data */
    printf("Enqueueing initial empty buffers to receive decoded PCM data");
    for(i = 0 ; i < NB_BUFFERS_IN_PCM_QUEUE ; i++) {
        printf(" %d", i);
        res = (*decBuffQueueItf)->Enqueue(decBuffQueueItf, sinkCntxt.pData, BUFFER_SIZE_IN_BYTES);
        ExitOnError(res);
        sinkCntxt.pData += BUFFER_SIZE_IN_BYTES;
        if (sinkCntxt.pData >= sinkCntxt.pDataBase +
                (NB_BUFFERS_IN_PCM_QUEUE * BUFFER_SIZE_IN_BYTES)) {
            sinkCntxt.pData = sinkCntxt.pDataBase;
        }
    }
    printf("\n");

    /* Initialize the callback for the Android buffer queue of the encoded data */
    res = (*aacBuffQueueItf)->RegisterCallback(aacBuffQueueItf, AndroidBufferQueueCallback, NULL);
    ExitOnError(res);

    /* Enqueue the content of our encoded data before starting to play,
       we don't want to starve the player initially */
    printf("Enqueueing initial full buffers of encoded ADTS data");
    for (i=0 ; i < NB_BUFFERS_IN_ADTS_QUEUE ; i++) {
        if (filelen < 7 || frame[0] != 0xFF || (frame[1] & 0xF0) != 0xF0)
            break;
        unsigned framelen = ((frame[3] & 3) << 11) | (frame[4] << 3) | (frame[5] >> 5);
        printf(" %d", i);
        res = (*aacBuffQueueItf)->Enqueue(aacBuffQueueItf, NULL /*pBufferContext*/,
                frame, framelen, NULL, 0);
        ExitOnError(res);
        frame += framelen;
        filelen -= framelen;
    }
    printf("\n");

#ifdef QUERY_METADATA
    /* ------------------------------------------------------ */
    /* Display the metadata obtained from the decoder */
    //   This is for test / demonstration purposes only where we discover the key and value sizes
    //   of a PCM decoder. An application that would want to directly get access to those values
    //   can make assumptions about the size of the keys and their matching values (all SLuint32)
    SLuint32 itemCount;
    res = (*mdExtrItf)->GetItemCount(mdExtrItf, &itemCount);
    ExitOnError(res);
    printf("itemCount=%u\n", itemCount);
    SLuint32 keySize, valueSize;
    SLMetadataInfo *keyInfo, *value;
    for(i=0 ; i<itemCount ; i++) {
        keyInfo = NULL; keySize = 0;
        value = NULL;   valueSize = 0;
        res = (*mdExtrItf)->GetKeySize(mdExtrItf, i, &keySize);
        ExitOnError(res);
        res = (*mdExtrItf)->GetValueSize(mdExtrItf, i, &valueSize);
        ExitOnError(res);
        keyInfo = (SLMetadataInfo*) malloc(keySize);
        if (NULL != keyInfo) {
            res = (*mdExtrItf)->GetKey(mdExtrItf, i, keySize, keyInfo);
            ExitOnError(res);
            printf("key[%d] size=%d, name=%s \tvalue size=%d encoding=0x%X langCountry=%s\n",
                    i, keyInfo->size, keyInfo->data, valueSize, keyInfo->encoding,
                    keyInfo->langCountry);
            /* find out the key index of the metadata we're interested in */
            if (!strcmp((char*)keyInfo->data, ANDROID_KEY_PCMFORMAT_NUMCHANNELS)) {
                channelCountKeyIndex = i;
            } else if (!strcmp((char*)keyInfo->data, ANDROID_KEY_PCMFORMAT_SAMPLESPERSEC)) {
                sampleRateKeyIndex = i;
            }
            free(keyInfo);
        }
    }
    if (channelCountKeyIndex != -1) {
        printf("Key %s is at index %d\n",
                ANDROID_KEY_PCMFORMAT_NUMCHANNELS, channelCountKeyIndex);
    } else {
        fprintf(stderr, "Unable to find key %s\n", ANDROID_KEY_PCMFORMAT_NUMCHANNELS);
    }
    if (sampleRateKeyIndex != -1) {
        printf("Key %s is at index %d\n",
                ANDROID_KEY_PCMFORMAT_SAMPLESPERSEC, sampleRateKeyIndex);
    } else {
        fprintf(stderr, "Unable to find key %s\n", ANDROID_KEY_PCMFORMAT_SAMPLESPERSEC);
    }
#endif

    /* ------------------------------------------------------ */
    /* Start decoding */
    printf("Starting to decode\n");
    res = (*playItf)->SetPlayState(playItf, SL_PLAYSTATE_PLAYING);
    ExitOnError(res);

    /* Decode until the end of the stream is reached */
    pthread_mutex_lock(&eosLock);
    while (!eos) {
        pthread_cond_wait(&eosCondition, &eosLock);
    }
    pthread_mutex_unlock(&eosLock);

    /* This just means done enqueueing; there may still more data in decode queue! */
    usleep(100 * 1000);

    pthread_mutex_lock(&eosLock);
    printf("Frame counters: encoded=%u decoded=%u\n", encodedFrames, decodedFrames);
    pthread_mutex_unlock(&eosLock);

    /* ------------------------------------------------------ */
    /* End of decoding */

destroyRes:
    /* Destroy the AudioPlayer object */
    (*player)->Destroy(player);

    fclose(outputFp);

    // unmap the ADTS AAC file from memory
    ok = munmap(ptr, statbuf.st_size);
    if (0 != ok) {
        perror(path);
    }
}

//-----------------------------------------------------------------
int main(int argc, char* const argv[])
{
    SLresult    res;
    SLObjectItf sl;

    printf("OpenSL ES test %s: decodes a file containing AAC ADTS data\n", argv[0]);

    if (argc != 2) {
        printf("Usage: \t%s source_file\n", argv[0]);
        printf("Example: \"%s /sdcard/myFile.adts\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    // open pathname of encoded ADTS AAC file to get a file descriptor
    int fd;
    fd = open(argv[1], O_RDONLY);
    if (fd < 0) {
        perror(argv[1]);
        return EXIT_FAILURE;
    }

    SLEngineOption EngineOption[] = {
            {(SLuint32) SL_ENGINEOPTION_THREADSAFE, (SLuint32) SL_BOOLEAN_TRUE}
    };

    res = slCreateEngine( &sl, 1, EngineOption, 0, NULL, NULL);
    ExitOnError(res);

    /* Realizing the SL Engine in synchronous mode. */
    res = (*sl)->Realize(sl, SL_BOOLEAN_FALSE);
    ExitOnError(res);

    TestDecToBuffQueue(sl, argv[1], fd);

    /* Shutdown OpenSL ES */
    (*sl)->Destroy(sl);

    // close the file
    (void) close(fd);

    return EXIT_SUCCESS;
}
