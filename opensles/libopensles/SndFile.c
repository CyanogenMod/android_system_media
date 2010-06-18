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

// FIXME should run this asynchronously esp. for socket fd, not on mix thread, and add mutexes
void SndFile_Callback(SLBufferQueueItf caller, void *pContext)
{
    CAudioPlayer *thisAP = (CAudioPlayer *) pContext;
    if (thisAP->mPlay.mState != SL_PLAYSTATE_PLAYING)
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
    short *pBuffer = &this->mBuffer[this->mWhich * SndFile_BUFSIZE];
    if (++this->mWhich >= SndFile_NUMBUFS)
        this->mWhich = 0;
    sf_count_t count;
    count = sf_read_short(this->mSNDFILE, pBuffer, (sf_count_t) SndFile_BUFSIZE);
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
        thisAP->mPlay.mState = SL_PLAYSTATE_PAUSED;
        (void) sf_seek(this->mSNDFILE, (sf_count_t) 0, SEEK_SET);
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
        // FIXME check other schemes and handle, or return SL_RESULT_CONTENT_UNSUPPORTED
        if (!strncmp((const char *) uri, "file:///", 8))
            uri += 8;
        switch (formatType) {
        case SL_DATAFORMAT_MIME:
            break;
        default:
            return SL_RESULT_CONTENT_UNSUPPORTED;
        }
        this->mSndFile.mPathname = uri;
        // FIXME magic number, should be configurable
        this->mBufferQueue.mNumBuffers = 2;
        }
        break;
    default:
        return SL_RESULT_CONTENT_UNSUPPORTED;
    }
    this->mSndFile.mWhich = 0;
    this->mSndFile.mSNDFILE = NULL;
    this->mSndFile.mRetryBuffer = NULL;
    this->mSndFile.mRetrySize = 0;
    return SL_RESULT_SUCCESS;
}

#endif // USE_SNDFILE
