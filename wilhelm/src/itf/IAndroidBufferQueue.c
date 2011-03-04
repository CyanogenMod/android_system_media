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


/** Determine the state of the audio player or audio recorder associated with a buffer queue.
 *  Note that PLAYSTATE and RECORDSTATE values are equivalent (where PLAYING == RECORDING).
 */

static SLuint32 getAssociatedState(IAndroidBufferQueue *thiz)
{
    SLuint32 state;
    switch (InterfaceToObjectID(thiz)) {
    case XA_OBJECTID_MEDIAPLAYER:
        state = ((CMediaPlayer *) thiz->mThis)->mPlay.mState;
        break;
    case SL_OBJECTID_AUDIOPLAYER:
        state = ((CAudioPlayer *) thiz->mThis)->mPlay.mState;
        break;
    case SL_OBJECTID_AUDIORECORDER:
        state = ((CAudioRecorder *) thiz->mThis)->mRecord.mState;
        break;
    default:
        // unreachable, but just in case we will assume it is stopped
        assert(SL_BOOLEAN_FALSE);
        state = SL_PLAYSTATE_STOPPED;
        break;
    }
    return state;
}


SLresult IAndroidBufferQueue_RegisterCallback(SLAndroidBufferQueueItf self,
        slAndroidBufferQueueCallback callback, void *pContext)
{
    SL_ENTER_INTERFACE

    IAndroidBufferQueue *thiz = (IAndroidBufferQueue *) self;

    interface_lock_exclusive(thiz);

    // verify pre-condition that media object is in the SL_PLAYSTATE_STOPPED state
    // FIXME PRIORITY 1 check play state
    //if (SL_PLAYSTATE_STOPPED == ((CAudioPlayer*) thiz->mThis)->mPlay.mState) {
        thiz->mCallback = callback;
        thiz->mContext = pContext;

        switch (InterfaceToObjectID(thiz)) {
        case SL_OBJECTID_AUDIOPLAYER:
            result = SL_RESULT_SUCCESS;
            android_audioPlayer_androidBufferQueue_registerCallback_l((CAudioPlayer*) thiz->mThis);
            break;
        case XA_OBJECTID_MEDIAPLAYER:
            SL_LOGI("IAndroidBufferQueue_RegisterCallback()");
            result = SL_RESULT_SUCCESS;
            android_Player_androidBufferQueue_registerCallback_l((CMediaPlayer*) thiz->mThis);
            break;
        default:
            result = SL_RESULT_PARAMETER_INVALID;
            break;
        }

    //} else {
    //    result = SL_RESULT_PRECONDITIONS_VIOLATED;
    //}

    interface_unlock_exclusive(thiz);

    SL_LEAVE_INTERFACE
}


SLresult IAndroidBufferQueue_Clear(SLAndroidBufferQueueItf self)
{
    SL_ENTER_INTERFACE

    IAndroidBufferQueue *thiz = (IAndroidBufferQueue *) self;

    interface_lock_exclusive(thiz);

    // FIXME return value?
    result = SL_RESULT_SUCCESS;
    android_audioPlayer_androidBufferQueue_clear_l((CAudioPlayer*) thiz->mThis);

    interface_unlock_exclusive(thiz);

    SL_LEAVE_INTERFACE
}


SLresult IAndroidBufferQueue_Enqueue(SLAndroidBufferQueueItf self,
        const void *pData,
        SLuint32 dataLength,
        const SLAndroidBufferItem *pItems,
        SLuint32 itemsLength)
{
    SL_ENTER_INTERFACE

    if ( ((NULL == pData) || (0 == dataLength))
            && ((NULL == pItems) || (0 == itemsLength))) {
        // no data and no msg
        SL_LOGE("Enqueue failure: trying to enqueue buffer with no data and no items.");
        result = SL_RESULT_PARAMETER_INVALID;
    } else {
        IAndroidBufferQueue *thiz = (IAndroidBufferQueue *) self;

        interface_lock_exclusive(thiz);

        AdvancedBufferHeader *oldRear = thiz->mRear, *newRear;
        if ((newRear = oldRear + 1) == &thiz->mBufferArray[thiz->mNumBuffers + 1]) {
            newRear = thiz->mBufferArray;
        }
        if (newRear == thiz->mFront) {
            result = SL_RESULT_BUFFER_INSUFFICIENT;
        } else {
            oldRear->mDataBuffer = pData;
            oldRear->mDataSize = dataLength;
            oldRear->mDataSizeConsumed = 0;
            thiz->mRear = newRear;
            ++thiz->mState.count;
            result = SL_RESULT_SUCCESS;
        }
        // set enqueue attribute if state is PLAYING and the first buffer is enqueued
        interface_unlock_exclusive_attributes(thiz, ((SL_RESULT_SUCCESS == result) &&
                (1 == thiz->mState.count) && (SL_PLAYSTATE_PLAYING == getAssociatedState(thiz))) ?
                        ATTR_BQ_ENQUEUE : ATTR_NONE);
    }

    SL_LEAVE_INTERFACE
}


static const struct SLAndroidBufferQueueItf_ IAndroidBufferQueue_Itf = {
    IAndroidBufferQueue_RegisterCallback,
    IAndroidBufferQueue_Clear,
    IAndroidBufferQueue_Enqueue
};


void IAndroidBufferQueue_init(void *self)
{
    IAndroidBufferQueue *thiz = (IAndroidBufferQueue *) self;
    thiz->mItf = &IAndroidBufferQueue_Itf;

    thiz->mState.count = 0;
    thiz->mState.index = 0;

    thiz->mCallback = NULL;
    thiz->mContext = NULL;

    thiz->mBufferArray = NULL;
    thiz->mFront = NULL;
    thiz->mRear = NULL;
}


void IAndroidBufferQueue_deinit(void *self)
{
    IAndroidBufferQueue *thiz = (IAndroidBufferQueue *) self;
    if (NULL != thiz->mBufferArray) {
        free(thiz->mBufferArray);
        thiz->mBufferArray = NULL;
    }
}
