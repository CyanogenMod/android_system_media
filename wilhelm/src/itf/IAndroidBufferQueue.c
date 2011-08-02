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


/**
 * Determine the state of the audio player or audio recorder associated with a buffer queue.
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


/**
 * parse and set the items associated with the given buffer, based on the buffer type,
 * which determines the set of authorized items and format
 */
static void setItems(const SLAndroidBufferItem *pItems, SLuint32 itemsLength,
        SLuint16 bufferType, AdvancedBufferHeader *pBuff)
{
    if ((NULL == pItems) || (0 == itemsLength)) {
        // no item data, reset item structure based on type
        switch (bufferType) {
          case kAndroidBufferTypeMpeg2Ts:
            pBuff->mItems.mTsCmdData.mTsCmdCode = ANDROID_MP2TSEVENT_NONE;
            pBuff->mItems.mTsCmdData.mPts = 0;
            break;
          case kAndroidBufferTypeInvalid:
          default:
            return;
        }
    } else {
        // parse item data based on type
        switch (bufferType) {

          case kAndroidBufferTypeMpeg2Ts: {
            SLuint32 index = 0;
            // supported Mpeg2Ts commands are mutually exclusive
            switch (pItems->itemKey) {

              case SL_ANDROID_ITEMKEY_EOS:
                pBuff->mItems.mTsCmdData.mTsCmdCode |= ANDROID_MP2TSEVENT_EOS;
                //SL_LOGD("Found EOS event=%d", pBuff->mItems.mTsCmdData.mTsCmdCode);
                break;

              case SL_ANDROID_ITEMKEY_DISCONTINUITY:
                if (pItems->itemSize == 0) {
                    pBuff->mItems.mTsCmdData.mTsCmdCode |= ANDROID_MP2TSEVENT_DISCONTINUITY;
                    //SL_LOGD("Found DISCONTINUITYevent=%d", pBuff->mItems.mTsCmdData.mTsCmdCode);
                } else if (pItems->itemSize == sizeof(SLAuint64)) {
                    pBuff->mItems.mTsCmdData.mTsCmdCode |= ANDROID_MP2TSEVENT_DISCON_NEWPTS;
                    pBuff->mItems.mTsCmdData.mPts = *((SLAuint64*)pItems->itemData);
                    //SL_LOGD("Found PTS=%lld", pBuff->mItems.mTsCmdData.mPts);
                } else {
                    SL_LOGE("Invalid size for MPEG-2 PTS, ignoring value");
                    pBuff->mItems.mTsCmdData.mTsCmdCode |= ANDROID_MP2TSEVENT_DISCONTINUITY;
                }
                break;

              case SL_ANDROID_ITEMKEY_FORMAT_CHANGE:
                pBuff->mItems.mTsCmdData.mTsCmdCode |= ANDROID_MP2TSEVENT_FORMAT_CHANGE;
                if (pItems->itemSize != 0) {
                    SL_LOGE("Invalid item parameter size for format change, ignoring value");
                }
                break;

              default:
                // no event with this buffer
                pBuff->mItems.mTsCmdData.mTsCmdCode = ANDROID_MP2TSEVENT_NONE;
                break;
            }// switch (pItems->itemKey)
          } break;

          default:
            return;
        }// switch (bufferType)
    }
}


static SLresult IAndroidBufferQueue_RegisterCallback(SLAndroidBufferQueueItf self,
        slAndroidBufferQueueCallback callback, void *pContext)
{
    SL_ENTER_INTERFACE

    IAndroidBufferQueue *thiz = (IAndroidBufferQueue *) self;

    interface_lock_exclusive(thiz);

    // verify pre-condition that media object is in the SL_PLAYSTATE_STOPPED state
    if (SL_PLAYSTATE_STOPPED == getAssociatedState(thiz)) {
        thiz->mCallback = callback;
        thiz->mContext = pContext;

        switch (InterfaceToObjectID(thiz)) {
          case SL_OBJECTID_AUDIOPLAYER:
            result = SL_RESULT_SUCCESS;
            android_audioPlayer_androidBufferQueue_registerCallback_l((CAudioPlayer*) thiz->mThis);
            break;
          case XA_OBJECTID_MEDIAPLAYER:
            SL_LOGV("IAndroidBufferQueue_RegisterCallback()");
            result = SL_RESULT_SUCCESS;
            android_Player_androidBufferQueue_registerCallback_l((CMediaPlayer*) thiz->mThis);
            break;
          default:
            result = SL_RESULT_PARAMETER_INVALID;
        }

    } else {
        result = SL_RESULT_PRECONDITIONS_VIOLATED;
    }

    interface_unlock_exclusive(thiz);

    SL_LEAVE_INTERFACE
}


static SLresult IAndroidBufferQueue_Clear(SLAndroidBufferQueueItf self)
{
    SL_ENTER_INTERFACE
    result = SL_RESULT_SUCCESS;

    IAndroidBufferQueue *thiz = (IAndroidBufferQueue *) self;

    interface_lock_exclusive(thiz);

    // reset the queue pointers
    thiz->mFront = &thiz->mBufferArray[0];
    thiz->mRear = &thiz->mBufferArray[0];
    // reset the queue state
    thiz->mState.count = 0;
    thiz->mState.index = 0;
    // reset the individual buffers
    for (XAuint16 i=0 ; i<(thiz->mNumBuffers + 1) ; i++) {
        thiz->mBufferArray[i].mDataBuffer = NULL;
        thiz->mBufferArray[i].mDataSize = 0;
        thiz->mBufferArray[i].mDataSizeConsumed = 0;
        thiz->mBufferArray[i].mBufferContext = NULL;
        thiz->mBufferArray[i].mBufferState = SL_ANDROIDBUFFERQUEUEEVENT_NONE;
        switch (thiz->mBufferType) {
          case kAndroidBufferTypeMpeg2Ts:
            thiz->mBufferArray[i].mItems.mTsCmdData.mTsCmdCode = ANDROID_MP2TSEVENT_NONE;
            thiz->mBufferArray[i].mItems.mTsCmdData.mPts = 0;
            break;
          default:
            result = SL_RESULT_CONTENT_UNSUPPORTED;
        }
    }

    if (SL_RESULT_SUCCESS == result) {
        // object-specific behavior for a clear
        switch (InterfaceToObjectID(thiz)) {
        case SL_OBJECTID_AUDIOPLAYER:
            result = SL_RESULT_SUCCESS;
            android_audioPlayer_androidBufferQueue_clear_l((CAudioPlayer*) thiz->mThis);
            break;
        case XA_OBJECTID_MEDIAPLAYER:
            result = SL_RESULT_SUCCESS;
            android_Player_androidBufferQueue_clear_l((CMediaPlayer*) thiz->mThis);
            break;
        default:
            result = SL_RESULT_PARAMETER_INVALID;
        }
    }

    interface_unlock_exclusive(thiz);

    SL_LEAVE_INTERFACE
}


static SLresult IAndroidBufferQueue_Enqueue(SLAndroidBufferQueueItf self,
        void *pBufferContext,
        void *pData,
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

        // buffer size check, can be done outside of lock because buffer type can't change
        switch (thiz->mBufferType) {
          case kAndroidBufferTypeMpeg2Ts:
            if (dataLength % MPEG2_TS_BLOCK_SIZE == 0) {
                break;
            }
            // intended fall-through if test failed
            SL_LOGE("Error enqueueing MPEG-2 TS data: size must be a multiple of %d (block size)",
                    MPEG2_TS_BLOCK_SIZE);
          case kAndroidBufferTypeInvalid:
          default:
            result = SL_RESULT_PARAMETER_INVALID;
            SL_LEAVE_INTERFACE
        }

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
            oldRear->mBufferContext = pBufferContext;
            oldRear->mBufferState = SL_ANDROIDBUFFERQUEUEEVENT_NONE;
            thiz->mRear = newRear;
            ++thiz->mState.count;
            setItems(pItems, itemsLength, thiz->mBufferType, oldRear);
            result = SL_RESULT_SUCCESS;
        }
        // set enqueue attribute if state is PLAYING and the first buffer is enqueued
        interface_unlock_exclusive_attributes(thiz, ((SL_RESULT_SUCCESS == result) &&
                (1 == thiz->mState.count) && (SL_PLAYSTATE_PLAYING == getAssociatedState(thiz))) ?
                        ATTR_ABQ_ENQUEUE : ATTR_NONE);
    }

    SL_LEAVE_INTERFACE
}


static SLresult IAndroidBufferQueue_GetState(SLAndroidBufferQueueItf self,
        SLAndroidBufferQueueState *pState)
{
    SL_ENTER_INTERFACE

    // Note that GetState while a Clear is pending is equivalent to GetState before the Clear

    if (NULL == pState) {
        result = SL_RESULT_PARAMETER_INVALID;
    } else {
        IAndroidBufferQueue *thiz = (IAndroidBufferQueue *) self;

        interface_lock_shared(thiz);

        pState->count = thiz->mState.count;
        pState->index = thiz->mState.index;

        interface_unlock_shared(thiz);

        result = SL_RESULT_SUCCESS;
    }

    SL_LEAVE_INTERFACE
}


static SLresult IAndroidBufferQueue_SetCallbackEventsMask(SLAndroidBufferQueueItf self,
        SLuint32 eventFlags)
{
    SL_ENTER_INTERFACE

    IAndroidBufferQueue *thiz = (IAndroidBufferQueue *) self;
    interface_lock_exclusive(thiz);
    // FIXME only supporting SL_ANDROIDBUFFERQUEUEEVENT_PROCESSED in this implementation
    if ((SL_ANDROIDBUFFERQUEUEEVENT_PROCESSED == eventFlags) ||
            (SL_ANDROIDBUFFERQUEUEEVENT_NONE == eventFlags)) {
        thiz->mCallbackEventsMask = eventFlags;
        result = SL_RESULT_SUCCESS;
    } else {
        result = SL_RESULT_FEATURE_UNSUPPORTED;
    }
    interface_unlock_exclusive(thiz);

    SL_LEAVE_INTERFACE
}


static SLresult IAndroidBufferQueue_GetCallbackEventsMask(SLAndroidBufferQueueItf self,
        SLuint32 *pEventFlags)
{
    SL_ENTER_INTERFACE

    if (NULL == pEventFlags) {
        result = SL_RESULT_PARAMETER_INVALID;
    } else {
        IAndroidBufferQueue *thiz = (IAndroidBufferQueue *) self;
        interface_lock_peek(thiz);
        SLuint32 callbackEventsMask = thiz->mCallbackEventsMask;
        interface_unlock_peek(thiz);
        *pEventFlags = callbackEventsMask;
        result = SL_RESULT_SUCCESS;
    }

    SL_LEAVE_INTERFACE
}


static const struct SLAndroidBufferQueueItf_ IAndroidBufferQueue_Itf = {
    IAndroidBufferQueue_RegisterCallback,
    IAndroidBufferQueue_Clear,
    IAndroidBufferQueue_Enqueue,
    IAndroidBufferQueue_GetState,
    IAndroidBufferQueue_SetCallbackEventsMask,
    IAndroidBufferQueue_GetCallbackEventsMask
};


void IAndroidBufferQueue_init(void *self)
{
    IAndroidBufferQueue *thiz = (IAndroidBufferQueue *) self;
    thiz->mItf = &IAndroidBufferQueue_Itf;

    thiz->mState.count = 0;
    thiz->mState.index = 0;

    thiz->mCallback = NULL;
    thiz->mContext = NULL;
    thiz->mCallbackEventsMask = SL_ANDROIDBUFFERQUEUEEVENT_PROCESSED;

    thiz->mBufferType = kAndroidBufferTypeInvalid;
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
