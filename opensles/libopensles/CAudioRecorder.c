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

/* AudioRecorder class */

#include "sles_allinclusive.h"

// IObject hooks

/** Hook called by Object::Realize when an audio recorder is realized */

SLresult CAudioRecorder_Realize(void *self, SLboolean async)
{
    SLresult result = SL_RESULT_SUCCESS;

#ifdef ANDROID
    CAudioRecorder *this = (CAudioRecorder *) self;
    result = android_audioRecorder_realize(this, async);
#endif

    return result;
}


/** Hook called by Object::Resume when an audio recorder is resumed */
SLresult CAudioRecorder_Resume(void *self, SLboolean async)
{
    //CAudioRecorder *this = (CAudioRecorder *) self;
    SLresult result = SL_RESULT_SUCCESS;

    // FIXME implement resume on an AudioRecorder

    return result;
}


/** Hook called by Object::Destroy when an audio recorder is destroyed */

void CAudioRecorder_Destroy(void *self)
{
    CAudioRecorder *this = (CAudioRecorder *) self;
    freeDataLocatorFormat(&this->mDataSource);
    freeDataLocatorFormat(&this->mDataSink);
#ifdef ANDROID
    // Free the buffer queue, if it was larger than typical
    if (NULL != this->mBufferQueue.mArray &&
        this->mBufferQueue.mArray != this->mBufferQueue.mTypical) {
            free(this->mBufferQueue.mArray);
            this->mBufferQueue.mArray = NULL;
    }
#endif

#ifdef ANDROID
    android_audioRecorder_destroy(this);
#endif
}
