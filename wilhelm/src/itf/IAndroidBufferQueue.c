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

//#define USE_LOG SLAndroidLogLevel_Verbose

#include "sles_allinclusive.h"
// for AAC ADTS verification on enqueue:
#include "android/include/AacBqToPcmCbRenderer.h"

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
    // reset item structure based on type
    switch (bufferType) {
      case kAndroidBufferTypeMpeg2Ts:
        pBuff->mItems.mTsCmdData.mTsCmdCode = ANDROID_MP2TSEVENT_NONE;
        pBuff->mItems.mTsCmdData.mPts = 0;
        break;
      case kAndroidBufferTypeAacadts:
        pBuff->mItems.mAdtsCmdData.mAdtsCmdCode = ANDROID_ADTSEVENT_NONE;
        break;
      case kAndroidBufferTypeInvalid:
      default:
        // shouldn't happen, but just in case clear out the item structure
        memset(&pBuff->mItems, 0, sizeof(AdvancedBufferItems));
        return;
    }

    // process all items in the array; if no items then we break out of loop immediately
    while (itemsLength > 0) {

        // remaining length must be large enough for one full item without any associated data
        if (itemsLength < sizeof(SLAndroidBufferItem)) {
            SL_LOGE("Partial item at end of array ignored");
            break;
        }
        itemsLength -= sizeof(SLAndroidBufferItem);

        // remaining length must be large enough for data with current item and alignment padding
        SLuint32 itemDataSizeWithAlignmentPadding = (pItems->itemSize + 3) & ~3;
        if (itemsLength < itemDataSizeWithAlignmentPadding) {
            SL_LOGE("Partial item data at end of array ignored");
            break;
        }
        itemsLength -= itemDataSizeWithAlignmentPadding;

        // parse item data based on type
        switch (bufferType) {

          case kAndroidBufferTypeMpeg2Ts: {
            switch (pItems->itemKey) {

              case SL_ANDROID_ITEMKEY_EOS:
                pBuff->mItems.mTsCmdData.mTsCmdCode |= ANDROID_MP2TSEVENT_EOS;
                //SL_LOGD("Found EOS event=%d", pBuff->mItems.mTsCmdData.mTsCmdCode);
                if (pItems->itemSize != 0) {
                    SL_LOGE("Invalid item parameter size %u for EOS, ignoring value",
                            pItems->itemSize);
                }
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
                    SL_LOGE("Invalid item parameter size %u for MPEG-2 PTS, ignoring value",
                            pItems->itemSize);
                    pBuff->mItems.mTsCmdData.mTsCmdCode |= ANDROID_MP2TSEVENT_DISCONTINUITY;
                }
                break;

              case SL_ANDROID_ITEMKEY_FORMAT_CHANGE:
                pBuff->mItems.mTsCmdData.mTsCmdCode |= ANDROID_MP2TSEVENT_FORMAT_CHANGE;
                if (pItems->itemSize != 0) {
                    SL_LOGE("Invalid item parameter size %u for format change, ignoring value",
                            pItems->itemSize);
                }
                break;

              default:
                // unknown item key
                SL_LOGE("Unknown item key %u with size %u ignored", pItems->itemKey,
                        pItems->itemSize);
                break;

            }// switch (pItems->itemKey)
          } break;

          case kAndroidBufferTypeAacadts: {
            switch (pItems->itemKey) {

              case SL_ANDROID_ITEMKEY_EOS:
                pBuff->mItems.mAdtsCmdData.mAdtsCmdCode |= ANDROID_ADTSEVENT_EOS;
                if (pItems->itemSize != 0) {
                    SL_LOGE("Invalid item parameter size %u for EOS, ignoring value",
                            pItems->itemSize);
                }
                break;

              default:
                // unknown item key
                SL_LOGE("Unknown item key %u with size %u ignored", pItems->itemKey,
                        pItems->itemSize);
                break;

            }// switch (pItems->itemKey)
          } break;

          case kAndroidBufferTypeInvalid:
          default:
            // not reachable as we checked this earlier
            return;

        }// switch (bufferType)

        // skip past this item, including data with alignment padding
        pItems = (SLAndroidBufferItem *) ((char *) pItems +
                sizeof(SLAndroidBufferItem) + itemDataSizeWithAlignmentPadding);
    }

    // now check for invalid combinations of items
    switch (bufferType) {

      case kAndroidBufferTypeMpeg2Ts: {
        // supported Mpeg2Ts commands are mutually exclusive
        switch (pBuff->mItems.mTsCmdData.mTsCmdCode) {
          // single items are allowed
          case ANDROID_MP2TSEVENT_NONE:
          case ANDROID_MP2TSEVENT_EOS:
          case ANDROID_MP2TSEVENT_DISCONTINUITY:
          case ANDROID_MP2TSEVENT_DISCON_NEWPTS:
          case ANDROID_MP2TSEVENT_FORMAT_CHANGE:
            break;
          // no combinations are allowed
          default:
            SL_LOGE("Invalid combination of items; all ignored");
            pBuff->mItems.mTsCmdData.mTsCmdCode = ANDROID_MP2TSEVENT_NONE;
            break;
        }
      } break;

      case kAndroidBufferTypeAacadts: {
        // only one item supported, and thus no combination check needed
      } break;

      case kAndroidBufferTypeInvalid:
      default:
        // not reachable as we checked this earlier
        return;
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

        // FIXME investigate why these two cases are not handled symmetrically any more
        switch (InterfaceToObjectID(thiz)) {
          case SL_OBJECTID_AUDIOPLAYER:
            result = android_audioPlayer_androidBufferQueue_registerCallback_l(
                    (CAudioPlayer*) thiz->mThis);
            break;
          case XA_OBJECTID_MEDIAPLAYER:
            result = SL_RESULT_SUCCESS;
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
          case kAndroidBufferTypeAacadts:
            thiz->mBufferArray[i].mItems.mAdtsCmdData.mAdtsCmdCode = ANDROID_ADTSEVENT_NONE;
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
    SL_LOGD("IAndroidBufferQueue_Enqueue pData=%p dataLength=%d", pData, dataLength);

    if ((dataLength > 0) && (NULL == pData)) {
        SL_LOGE("Enqueue failure: non-zero data length %u but NULL data pointer", dataLength);
        result = SL_RESULT_PARAMETER_INVALID;
    } else if ((itemsLength > 0) && (NULL == pItems)) {
        SL_LOGE("Enqueue failure: non-zero items length %u but NULL items pointer", itemsLength);
        result = SL_RESULT_PARAMETER_INVALID;
    } else if ((0 == dataLength) && (0 == itemsLength)) {
        // no data and no msg
        SL_LOGE("Enqueue failure: trying to enqueue buffer with no data and no items.");
        result = SL_RESULT_PARAMETER_INVALID;
    // Note that a non-NULL data pointer with zero data length is allowed.
    // We track that data pointer as it moves through the queue
    // to assist the application in accounting for data buffers.
    // A non-NULL items pointer with zero items length is also allowed, but has no value.
    } else {
        IAndroidBufferQueue *thiz = (IAndroidBufferQueue *) self;

        // buffer size check, can be done outside of lock because buffer type can't change
        switch (thiz->mBufferType) {
          case kAndroidBufferTypeMpeg2Ts:
            if (dataLength % MPEG2_TS_BLOCK_SIZE == 0) {
                break;
            }
            SL_LOGE("Error enqueueing MPEG-2 TS data: size must be a multiple of %d (block size)",
                    MPEG2_TS_BLOCK_SIZE);
            result = SL_RESULT_PARAMETER_INVALID;
            SL_LEAVE_INTERFACE
            break;
          case kAndroidBufferTypeAacadts:
            // non-zero dataLength is permitted in case of EOS command only
            if (dataLength > 0) {
                result = android::AacBqToPcmCbRenderer::validateBufferStartEndOnFrameBoundaries(
                    pData, dataLength);
                if (SL_RESULT_SUCCESS != result) {
                    SL_LOGE("Error enqueueing ADTS data: data must start and end on frame "
                            "boundaries");
                    SL_LEAVE_INTERFACE
                }
            }
            break;
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
            // set oldRear->mItems based on items
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
        interface_lock_shared(thiz);
        SLuint32 callbackEventsMask = thiz->mCallbackEventsMask;
        interface_unlock_shared(thiz);
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
