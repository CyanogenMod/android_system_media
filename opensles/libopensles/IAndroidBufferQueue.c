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

/* AndroidBufferQueue implementation */

#include "sles_allinclusive.h"

SLresult IAndroidBufferQueue_RegisterCallback(SLAndroidBufferQueueItf self,
        slAndroidBufferQueueCallback callback, void *pContext)
{
    SL_ENTER_INTERFACE

    IAndroidBufferQueue *this = (IAndroidBufferQueue *) self;

    interface_lock_exclusive(this);

    // verify pre-condition that media object is in the SL_PLAYSTATE_STOPPED state
    if (SL_PLAYSTATE_STOPPED == ((CAudioPlayer*) this->mThis)->mPlay.mState) {
        this->mCallback = callback;
        this->mContext = pContext;
        result = SL_RESULT_SUCCESS;
        android_audioPlayer_androidBufferQueue_registerCallback_l((CAudioPlayer*) this->mThis);
    } else {
        result = SL_RESULT_PRECONDITIONS_VIOLATED;
    }

    interface_unlock_exclusive(this);

    SL_LEAVE_INTERFACE
}


SLresult IAndroidBufferQueue_Clear(SLAndroidBufferQueueItf self)
{
    SL_ENTER_INTERFACE

    IAndroidBufferQueue *this = (IAndroidBufferQueue *) self;

    interface_lock_exclusive(this);

    SL_LOGE("[ FIXME implement IAndroidBufferQueue_Clear ]");

    interface_unlock_exclusive(this);

    SL_LEAVE_INTERFACE
}


SLresult IAndroidBufferQueue_Enqueue(SLAndroidBufferQueueItf self,
        SLuint32 bufferId,
        SLuint32 length,
        SLAbufferQueueEvent event,
        void *pData)
{
    SL_ENTER_INTERFACE

    IAndroidBufferQueue *this = (IAndroidBufferQueue *) self;

    interface_lock_exclusive(this);

    SL_LOGE("[ FIXME implement IAndroidBufferQueue_Enqueue ]");

    interface_unlock_exclusive(this);

    SL_LEAVE_INTERFACE
}


static const struct SLAndroidBufferQueueItf_ IAndroidBufferQueue_Itf = {
    IAndroidBufferQueue_RegisterCallback,
    IAndroidBufferQueue_Clear,
    IAndroidBufferQueue_Enqueue
};

void IAndroidBufferQueue_init(void *self)
{
    IAndroidBufferQueue *this = (IAndroidBufferQueue *) self;
    this->mItf = &IAndroidBufferQueue_Itf;

    this->mCallback = NULL;
    this->mContext = NULL;
}
