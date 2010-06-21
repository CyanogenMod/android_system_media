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

/* BufferQueue implementation */

#include "sles_allinclusive.h"

static SLresult IBufferQueue_Enqueue(SLBufferQueueItf self, const void *pBuffer,
    SLuint32 size)
{
    if (NULL == pBuffer || 0 == size)
        return SL_RESULT_PARAMETER_INVALID;
    IBufferQueue *this = (IBufferQueue *) self;
    SLresult result;
    interface_lock_exclusive(this);
    BufferHeader *oldRear = this->mRear, *newRear;
    if ((newRear = oldRear + 1) == &this->mArray[this->mNumBuffers + 1])
        newRear = this->mArray;
    if (newRear == this->mFront) {
        result = SL_RESULT_BUFFER_INSUFFICIENT;
    } else {
        oldRear->mBuffer = pBuffer;
        oldRear->mSize = size;
        this->mRear = newRear;
        ++this->mState.count;
        result = SL_RESULT_SUCCESS;
    }
    //fprintf(stderr, "Enqueue: nbBuffers in queue = %lu\n", this->mState.count);
    interface_unlock_exclusive(this);
    return result;
}

static SLresult IBufferQueue_Clear(SLBufferQueueItf self)
{
    IBufferQueue *this = (IBufferQueue *) self;
    interface_lock_exclusive(this);
    this->mFront = &this->mArray[0];
    this->mRear = &this->mArray[0];
    this->mSizeConsumed = 0;
    this->mState.count = 0;
#ifdef ANDROID
    // FIXME must flush associated player
    fprintf(stderr, "FIXME: IBufferQueue_Clear must flush associated player, not implemented\n");
#endif
    interface_unlock_exclusive(this);
    return SL_RESULT_SUCCESS;
}

static SLresult IBufferQueue_GetState(SLBufferQueueItf self,
    SLBufferQueueState *pState)
{
    if (NULL == pState)
        return SL_RESULT_PARAMETER_INVALID;
    IBufferQueue *this = (IBufferQueue *) self;
    SLBufferQueueState state;
    interface_lock_shared(this);
#ifdef __cplusplus // FIXME Is this a compiler bug?
    state.count = this->mState.count;
    state.playIndex = this->mState.playIndex;
#else
    state = this->mState;
#endif
    interface_unlock_shared(this);
    *pState = state;
    return SL_RESULT_SUCCESS;
}

static SLresult IBufferQueue_RegisterCallback(SLBufferQueueItf self,
    slBufferQueueCallback callback, void *pContext)
{
    // FIXME verify conditions media object is in the SL_PLAYSTATE_STOPPED state
    fprintf(stderr, "FIXME: verify RegisterCallback is called on a player in SL_PLAYSTATE_STOPPED state, not implemented\n");
    IBufferQueue *this = (IBufferQueue *) self;
    interface_lock_exclusive(this);
    this->mCallback = callback;
    this->mContext = pContext;
    interface_unlock_exclusive(this);
    return SL_RESULT_SUCCESS;
}

static const struct SLBufferQueueItf_ IBufferQueue_Itf = {
    IBufferQueue_Enqueue,
    IBufferQueue_Clear,
    IBufferQueue_GetState,
    IBufferQueue_RegisterCallback
};

void IBufferQueue_init(void *self)
{
    IBufferQueue *this = (IBufferQueue *) self;
    this->mItf = &IBufferQueue_Itf;
    this->mState.count = 0;
    this->mState.playIndex = 0;
    this->mCallback = NULL;
    this->mContext = NULL;
    this->mNumBuffers = 0;
    this->mArray = NULL;
    this->mFront = NULL;
    this->mRear = NULL;
    this->mSizeConsumed = 0;
    BufferHeader *bufferHeader = this->mTypical;
    unsigned i;
    for (i = 0; i < BUFFER_HEADER_TYPICAL+1; ++i, ++bufferHeader) {
        bufferHeader->mBuffer = NULL;
        bufferHeader->mSize = 0;
    }
}
