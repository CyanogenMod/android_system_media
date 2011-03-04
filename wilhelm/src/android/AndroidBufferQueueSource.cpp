/*
 * Copyright (C) 2011 The Android Open Source Project
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

#include "sles_allinclusive.h"


//-----------------------------------------------------------------------------
// Callback associated with a StreamPlayer object that gets its data
//  from an AndroidBufferQueue. Signifies a buffer is available to receive data from
//  the queue.
void abqSrc_callBack_pullFromBuffQueue(const void* user, bool userIsAudioPlayer,
        size_t bufferId,
        void* bufferLoc,
        size_t buffSize) {

    if (NULL == user) {
        return;
    }

//    SL_LOGV("abqSrc_callBack_pullFromBuffQueue() called, isAudioplayer=%d", userIsAudioPlayer);

    slAndroidBufferQueueCallback callback = NULL;
    void* callbackPContext = NULL;

    IAndroidBufferQueue* abq;
    android::StreamPlayer* streamer;
    AdvancedBufferHeader *oldFront = NULL;


    if (userIsAudioPlayer) {
        abq = &((CAudioPlayer*)user)->mAndroidBufferQueue;
    } else {
        abq = &((CMediaPlayer*)user)->mAndroidBufferQueue;
    }

    // retrieve data from the buffer queue
    interface_lock_exclusive(abq);

    if (userIsAudioPlayer) {
        CAudioPlayer* ap = (CAudioPlayer*)user;
        streamer = static_cast<android::StreamPlayer*>(ap->mAPlayer.get());
    } else {
        CMediaPlayer* mp = (CMediaPlayer*)user;
        streamer = static_cast<android::StreamPlayer*>(mp->mAVPlayer.get());
    }

    if (abq->mState.count != 0) {
// SL_LOGV("nbBuffers in queue = %lu, buffSize=%lu",abq->mState.count, buffSize);
        assert(abq->mFront != abq->mRear);

        oldFront = abq->mFront;
        AdvancedBufferHeader *newFront = &oldFront[1];

        char *pSrc = ((char*)oldFront->mDataBuffer) + oldFront->mDataSizeConsumed;
        if (oldFront->mDataSizeConsumed + buffSize < oldFront->mDataSize) {
            // more available than requested, copy as much as requested
            // consume data: 1/ copy to given destination
            memcpy(bufferLoc, pSrc, buffSize);
            //               2/ keep track of how much has been consumed
            oldFront->mDataSizeConsumed += buffSize;
            //               3/ notify StreamPlayer that new data is available
            // FIXME pass events
// SL_LOGE("enqueueing: enqueueing=%lu, dataSize=%lu", buffSize, oldFront->mDataSize);
            streamer->appEnqueue(bufferId, buffSize, SL_ANDROID_ITEMKEY_NONE, NULL);
        } else {
            // requested as much available or more: consume the whole of the current
            //   buffer and move to the next
            size_t consumed = oldFront->mDataSize - oldFront->mDataSizeConsumed;
// SL_LOGE("consuming rest of buffer: enqueueing=%ld", consumed);
            oldFront->mDataSizeConsumed = oldFront->mDataSize;

            // move queue to next
            if (newFront == &abq->mBufferArray[abq->mNumBuffers + 1]) {
                // reached the end, circle back
                newFront = abq->mBufferArray;
            }
            abq->mFront = newFront;
            abq->mState.count--;
            abq->mState.index++;

            // consume data: 1/ copy to given destination
            memcpy(bufferLoc, pSrc, consumed);
            //               2/ keep track of how much has been consumed
            // here nothing to do because we are done with this buffer
            //               3/ notify StreamPlayer that new data is available
            streamer->appEnqueue(bufferId, consumed, SL_ANDROID_ITEMKEY_NONE, NULL);

            // data has been consumed, and the buffer queue state has been updated
            // we will notify the client if applicable
            callback = abq->mCallback;
            // save callback data
            callbackPContext = abq->mContext;
        }
    } else { // empty queue
        SL_LOGV("ABQ empty, starving!");
        // signal we're at the end of the content, but don't pause (see note in function)
        if (userIsAudioPlayer) {
            // FIXME declare this external
            //audioPlayer_dispatch_headAtEnd_lockPlay((CAudioPlayer*)user,
            //        false /*set state to paused?*/, false);
        } else {
            // FIXME implement headAtEnd dispatch for CMediaPlayer
        }
    }

    interface_unlock_exclusive(abq);

    // notify client
    if (NULL != callback) {
        // oldFront was only initialized in the code path where callback is initialized
        //    so no need to check if it's valid
        (*callback)(&abq->mItf, callbackPContext,
                (void *)oldFront->mDataBuffer,/* pBufferData  */
                oldFront->mDataSize, /* dataSize  */
                // here a buffer is only dequeued when fully consumed
                oldFront->mDataSize, /* dataUsed  */
                // no messages during playback
                0, /* itemsLength */
                NULL /* pItems */);
    }
}

