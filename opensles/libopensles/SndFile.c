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

/* libsndfile integration */

#include "sles_allinclusive.h"

#ifdef USE_SNDFILE

void SndFile_Callback(SLBufferQueueItf caller, void *pContext)
{
    CAudioPlayer *thisAP = (CAudioPlayer *) pContext;
    object_lock_peek(&thisAP->mObject);
    SLuint32 state = thisAP->mPlay.mState;
    object_unlock_peek(&thisAP->mObject);
    if (SL_PLAYSTATE_PLAYING != state)
        return;
    struct SndFile *this = &thisAP->mSndFile;
    SLresult result;
    if (NULL != this->mRetryBuffer && 0 < this->mRetrySize) {
        result = (*caller)->Enqueue(caller, this->mRetryBuffer, this->mRetrySize);
        if (SL_RESULT_BUFFER_INSUFFICIENT == result)
            return;     // what, again?
        assert(SL_RESULT_SUCCESS == result);
        this->mRetryBuffer = NULL;
        this->mRetrySize = 0;
        return;
    }
    pthread_mutex_lock(&this->mMutex);
    if (this->mEOF) {
        pthread_mutex_unlock(&this->mMutex);
        return;
    }
    short *pBuffer = &this->mBuffer[this->mWhich * SndFile_BUFSIZE];
    if (++this->mWhich >= SndFile_NUMBUFS)
        this->mWhich = 0;
    sf_count_t count;
    count = sf_read_short(this->mSNDFILE, pBuffer, (sf_count_t) SndFile_BUFSIZE);
    if (0 >= count)
        this->mEOF = SL_BOOLEAN_TRUE;
    pthread_mutex_unlock(&this->mMutex);
    if (0 < count) {
        SLuint32 size = (SLuint32) (count * sizeof(short));
        result = (*caller)->Enqueue(caller, pBuffer, size);
        if (SL_RESULT_BUFFER_INSUFFICIENT == result) {
            this->mRetryBuffer = pBuffer;
            this->mRetrySize = size;
            return;
        }
        assert(SL_RESULT_SUCCESS == result);
    } else {
        object_lock_exclusive(&thisAP->mObject);
        // Should not pause yet - we just ran out of new data to enqueue,
        // but there may still be (partially) full buffers in the queue.
        thisAP->mPlay.mState = SL_PLAYSTATE_PAUSED;
        thisAP->mPlay.mPosition = thisAP->mPlay.mDuration;
        object_unlock_exclusive_attributes(&thisAP->mObject, ATTR_TRANSPORT);
    }
}

SLboolean SndFile_IsSupported(const SF_INFO *sfinfo)
{
    switch (sfinfo->format & SF_FORMAT_TYPEMASK) {
    case SF_FORMAT_WAV:
        break;
    default:
        return SL_BOOLEAN_FALSE;
    }
    switch (sfinfo->format & SF_FORMAT_SUBMASK) {
    case SF_FORMAT_PCM_U8:
    case SF_FORMAT_PCM_16:
        break;
    default:
        return SL_BOOLEAN_FALSE;
    }
    switch (sfinfo->samplerate) {
    case 11025:
    case 22050:
    case 44100:
        break;
    default:
        return SL_BOOLEAN_FALSE;
    }
    switch (sfinfo->channels) {
    case 1:
    case 2:
        break;
    default:
        return SL_BOOLEAN_FALSE;
    }
    return SL_BOOLEAN_TRUE;
}

SLresult SndFile_checkAudioPlayerSourceSink(CAudioPlayer *this)
{
    const SLDataSource *pAudioSrc = &this->mDataSource.u.mSource;
    SLuint32 locatorType = *(SLuint32 *)pAudioSrc->pLocator;
    SLuint32 formatType = *(SLuint32 *)pAudioSrc->pFormat;
    switch (locatorType) {
    case SL_DATALOCATOR_BUFFERQUEUE:
        this->mBufferQueue.mNumBuffers =
            ((SLDataLocator_BufferQueue *) pAudioSrc->pLocator)->numBuffers;
        break;
    case SL_DATALOCATOR_URI:
        {
        SLDataLocator_URI *dl_uri = (SLDataLocator_URI *) pAudioSrc->pLocator;
        SLchar *uri = dl_uri->URI;
        if (NULL == uri)
            return SL_RESULT_PARAMETER_INVALID;
        if (!strncmp((const char *) uri, "file:///", 8))
            uri += 8;
        switch (formatType) {
        case SL_DATAFORMAT_NULL:    // OK to omit the data format
        case SL_DATAFORMAT_MIME:    // we ignore a MIME type if specified
            break;
        default:
            return SL_RESULT_CONTENT_UNSUPPORTED;
        }
        this->mSndFile.mPathname = uri;
        this->mBufferQueue.mNumBuffers = SndFile_NUMBUFS;
        }
        break;
    default:
        return SL_RESULT_CONTENT_UNSUPPORTED;
    }
    this->mSndFile.mWhich = 0;
    this->mSndFile.mSNDFILE = NULL;
    // this->mSndFile.mMutex is initialized only when there is a valid mSNDFILE
    this->mSndFile.mEOF = SL_BOOLEAN_FALSE;
    this->mSndFile.mRetryBuffer = NULL;
    this->mSndFile.mRetrySize = 0;
    return SL_RESULT_SUCCESS;
}

void audioPlayerTransportUpdate(CAudioPlayer *audioPlayer)
{
    // marker and position updates here???
    SLmillisecond pos;

    pos = audioPlayer->mSeek.mPos;
    audioPlayer->mSeek.mPos = SL_TIME_UNKNOWN;

    if ((SL_TIME_UNKNOWN != pos) && (NULL != audioPlayer->mSndFile.mSNDFILE)) {
        pthread_mutex_lock(&audioPlayer->mSndFile.mMutex);
        // use pos not 0
        (void) sf_seek(audioPlayer->mSndFile.mSNDFILE, (sf_count_t) 0, SEEK_SET);
        audioPlayer->mSndFile.mEOF = SL_BOOLEAN_FALSE;
        pthread_mutex_unlock(&audioPlayer->mSndFile.mMutex);
    }

}

#endif // USE_SNDFILE
