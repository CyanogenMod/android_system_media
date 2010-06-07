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

/* ThreadSync implementation */

#include "sles_allinclusive.h"

static SLresult IThreadSync_EnterCriticalSection(SLThreadSyncItf self)
{
    IThreadSync *this = (IThreadSync *) self;
    SLresult result;
    interface_lock_exclusive(this);
    for (;;) {
        if (this->mInCriticalSection) {
            if (this->mOwner != pthread_self()) {
                this->mWaiting = SL_BOOLEAN_TRUE;
                interface_cond_wait(this);
                continue;
            }
            result = SL_RESULT_PRECONDITIONS_VIOLATED;
            break;
        }
        this->mInCriticalSection = SL_BOOLEAN_TRUE;
        this->mOwner = pthread_self();
        result = SL_RESULT_SUCCESS;
        break;
    }
    interface_unlock_exclusive(this);
    return result;
}

static SLresult IThreadSync_ExitCriticalSection(SLThreadSyncItf self)
{
    IThreadSync *this = (IThreadSync *) self;
    SLresult result;
    interface_lock_exclusive(this);
    if (!this->mInCriticalSection || this->mOwner != pthread_self()) {
        result = SL_RESULT_PRECONDITIONS_VIOLATED;
    } else {
        this->mInCriticalSection = SL_BOOLEAN_FALSE;
        this->mOwner = (pthread_t) NULL;
        result = SL_RESULT_SUCCESS;
        if (this->mWaiting) {
            this->mWaiting = SL_BOOLEAN_FALSE;
            interface_cond_signal(this);
        }
    }
    interface_unlock_exclusive(this);
    return result;
}

static const struct SLThreadSyncItf_ IThreadSync_Itf = {
    IThreadSync_EnterCriticalSection,
    IThreadSync_ExitCriticalSection
};

void IThreadSync_init(void *self)
{
    IThreadSync *this =
        (IThreadSync *) self;
    this->mItf = &IThreadSync_Itf;
    this->mInCriticalSection = SL_BOOLEAN_FALSE;
    this->mWaiting = SL_BOOLEAN_FALSE;
    this->mOwner = (pthread_t) NULL;
}
