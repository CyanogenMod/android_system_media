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

/** \file CAudioPlayer.c AudioPlayer class */

#include "sles_allinclusive.h"


/** \brief Hook called by Object::Realize when an audio player is realized */

SLresult CAudioPlayer_Realize(void *self, SLboolean async)
{
    CAudioPlayer *this = (CAudioPlayer *) self;
    SLresult result = SL_RESULT_SUCCESS;

    // initialize cached data, to be overwritten by platform-specific initialization
    this->mNumChannels = 0;

#ifdef ANDROID
    result = android_audioPlayer_realize(this, async);
#endif

#ifdef USE_SNDFILE
    if (NULL != this->mSndFile.mPathname) {
        this->mSndFile.mSfInfo.format = 0;
        this->mSndFile.mSNDFILE = sf_open(
            (const char *) this->mSndFile.mPathname, SFM_READ, &this->mSndFile.mSfInfo);
        if (NULL == this->mSndFile.mSNDFILE) {
            result = SL_RESULT_CONTENT_NOT_FOUND;
        } else if (!SndFile_IsSupported(&this->mSndFile.mSfInfo)) {
            sf_close(this->mSndFile.mSNDFILE);
            this->mSndFile.mSNDFILE = NULL;
            result = SL_RESULT_CONTENT_UNSUPPORTED;
        } else {
            int ok;
            ok = pthread_mutex_init(&this->mSndFile.mMutex, (const pthread_mutexattr_t *) NULL);
            assert(0 == ok);
            SLBufferQueueItf bufferQueue = &this->mBufferQueue.mItf;
            IBufferQueue *thisBQ = (IBufferQueue *) bufferQueue;
            IBufferQueue_RegisterCallback(&thisBQ->mItf, SndFile_Callback, this);
            this->mPrefetchStatus.mStatus = SL_PREFETCHSTATUS_SUFFICIENTDATA;
            // this is the initial duration; will update when a new maximum position is detected
            this->mPlay.mDuration = (SLmillisecond) (((long long) this->mSndFile.mSfInfo.frames *
                1000LL) / this->mSndFile.mSfInfo.samplerate);
            this->mNumChannels = this->mSndFile.mSfInfo.channels;
            this->mSampleRateMilliHz = this->mSndFile.mSfInfo.samplerate * 1000;
#ifdef USE_OUTPUTMIXEXT
            this->mPlay.mFrameUpdatePeriod = ((long long) this->mPlay.mPositionUpdatePeriod *
                (long long) this->mSampleRateMilliHz) / 1000000LL;
#endif
        }
    }
#endif // USE_SNDFILE
    return result;
}


/** \brief Hook called by Object::Resume when an audio player is resumed */

SLresult CAudioPlayer_Resume(void *self, SLboolean async)
{
    return SL_RESULT_SUCCESS;
}


/** \brief Hook called by Object::Destroy when an audio player is destroyed */

void CAudioPlayer_Destroy(void *self)
{
    CAudioPlayer *this = (CAudioPlayer *) self;
    freeDataLocatorFormat(&this->mDataSource);
    freeDataLocatorFormat(&this->mDataSink);
    // Unlink the audio player from the associate output mix
#ifdef USE_OUTPUTMIXEXT
    IOutputMixExt_Destroy(this);
#endif
    this->mOutputMix = NULL;
    // Free the buffer queue, if it was larger than typical
    if (NULL != this->mBufferQueue.mArray &&
        this->mBufferQueue.mArray != this->mBufferQueue.mTypical) {
            free(this->mBufferQueue.mArray);
            this->mBufferQueue.mArray = NULL;
    }
#ifdef USE_SNDFILE
    if (NULL != this->mSndFile.mSNDFILE) {
        sf_close(this->mSndFile.mSNDFILE);
        this->mSndFile.mSNDFILE = NULL;
        int ok;
        ok = pthread_mutex_destroy(&this->mSndFile.mMutex);
        assert(0 == ok);
    }
#endif // USE_SNDFILE
#ifdef ANDROID
    android_audioPlayer_destroy(this);
#endif
}


/** \brief Hook called by Object::Destroy before an audio player is about to be destroyed */

bool CAudioPlayer_PreDestroy(void *self)
{
#ifdef USE_OUTPUTMIXEXT
    CAudioPlayer *this = (CAudioPlayer *) self;
    // Request the mixer thread to unlink this audio player's track
    this->mDestroyRequested = true;
    while (this->mDestroyRequested) {
        object_cond_wait(self);
    }
    // Mixer thread has acknowledged the request
#endif
    return true;
}
