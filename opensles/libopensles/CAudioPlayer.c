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

/* AudioPlayer class */

#include "sles_allinclusive.h"

// IObject hooks

SLresult CAudioPlayer_Realize(void *self, SLboolean async)
{
    CAudioPlayer *this = (CAudioPlayer *) self;
    SLresult result = SL_RESULT_SUCCESS;

    // initialize cached data, to be overwritten by platform-specific initialization
    this->mNumChannels = 0;

#ifdef ANDROID
    result = sles_to_android_audioPlayerRealize(this, async);
#endif

#ifdef USE_SNDFILE
    if (NULL != this->mSndFile.mPathname) {
        SF_INFO sfinfo;
        sfinfo.format = 0;
        this->mSndFile.mSNDFILE = sf_open(
            (const char *) this->mSndFile.mPathname, SFM_READ, &sfinfo);
        if (NULL == this->mSndFile.mSNDFILE) {
            result = SL_RESULT_CONTENT_NOT_FOUND;
        } else if (!SndFile_IsSupported(&sfinfo)) {
            sf_close(this->mSndFile.mSNDFILE);
            this->mSndFile.mSNDFILE = NULL;
            result = SL_RESULT_CONTENT_UNSUPPORTED;
        } else {
            int ok;
            ok = pthread_mutex_init(&this->mSndFile.mMutex, (const pthread_mutexattr_t *) NULL);
            assert(0 == ok);
            SLBufferQueueItf bufferQueue = &this->mBufferQueue.mItf;
            IBufferQueue *thisBQ = (IBufferQueue *) bufferQueue;
            thisBQ->mCallback = SndFile_Callback;
            thisBQ->mContext = this;
            this->mPrefetchStatus.mStatus = SL_PREFETCHSTATUS_SUFFICIENTDATA;
            this->mPlay.mDuration = (SLmillisecond)
                (((long long) sfinfo.frames * 1000LL) / sfinfo.samplerate);
            this->mNumChannels = sfinfo.channels;
        }
    }
#endif // USE_SNDFILE
    return result;
}

void CAudioPlayer_Destroy(void *self)
{
    CAudioPlayer *this = (CAudioPlayer *) self;
    freeDataLocatorFormat(&this->mDataSource);
    freeDataLocatorFormat(&this->mDataSink);
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
    sles_to_android_audioPlayerDestroy(this);
#endif
}
