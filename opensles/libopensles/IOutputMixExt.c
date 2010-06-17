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

/* OutputMixExt implementation */

#include "sles_allinclusive.h"

#ifdef USE_OUTPUTMIXEXT

// Used by SDL but not specific to or dependent on SDL

static void IOutputMixExt_FillBuffer(SLOutputMixExtItf self, void *pBuffer, SLuint32 size)
{
    // Force to be a multiple of a frame, assumes stereo 16-bit PCM
    size &= ~3;
    IOutputMixExt *thisExt = (IOutputMixExt *) self;
    IOutputMix *this = &((COutputMix *) InterfaceToIObject(thisExt))->mOutputMix;
    unsigned activeMask = this->mActiveMask;
    SLboolean mixBufferHasData = SL_BOOLEAN_FALSE;
    while (activeMask) {
        unsigned i = ctz(activeMask);
        assert(MAX_TRACK > i);
        activeMask &= ~(1 << i);
        struct Track *track = &this->mTracks[i];
        // track is allocated
        IPlay *play = track->mPlay;
        if (NULL == play)
            continue;
        // track is initialized
        if (SL_PLAYSTATE_PLAYING != play->mState)
            continue;
        // track is playing
        void *dstWriter = pBuffer;
        unsigned desired = size;
        SLboolean trackContributedToMix = SL_BOOLEAN_FALSE;
        IBufferQueue *bufferQueue = track->mBufferQueue;
        while (desired > 0) {
            const BufferHeader *oldFront, *newFront, *rear;
            unsigned actual = desired;
            if (track->mAvail < actual)
                actual = track->mAvail;
            // force actual to be a frame multiple
            if (actual > 0) {
                // FIXME check for either mute or volume 0
                // in which case skip the input buffer processing
                assert(NULL != track->mReader);
                // FIXME && gain == 1.0
                if (mixBufferHasData) {
                    stereo *mixBuffer = (stereo *) dstWriter;
                    const stereo *source = (const stereo *) track->mReader;
                    unsigned j;
                    for (j = 0; j < actual; j += sizeof(stereo), ++mixBuffer,
                        ++source) {
                        // apply gain here
                        mixBuffer->left += source->left;
                        mixBuffer->right += source->right;
                    }
                } else {
                    // apply gain during copy
                    memcpy(dstWriter, track->mReader, actual);
                    trackContributedToMix = SL_BOOLEAN_TRUE;
                }
                dstWriter = (char *) dstWriter + actual;
                desired -= actual;
                track->mReader = (char *) track->mReader + actual;
                track->mAvail -= actual;
                if (track->mAvail == 0) {
                    if (NULL != bufferQueue) {
                        interface_lock_exclusive(bufferQueue);
                        oldFront = bufferQueue->mFront;
                        rear = bufferQueue->mRear;
                        assert(oldFront != rear);
                        newFront = oldFront;
                        if (++newFront == &bufferQueue->mArray[bufferQueue->mNumBuffers + 1])
                            newFront = bufferQueue->mArray;
                        bufferQueue->mFront = (BufferHeader *) newFront;
                        assert(0 < bufferQueue->mState.count);
                        --bufferQueue->mState.count;
                        // FIXME here or in Enqueue?
                        ++bufferQueue->mState.playIndex;
                        interface_unlock_exclusive(bufferQueue);
                        // FIXME a good time to do an early warning
                        // callback depending on buffer count
                    }
                }
                continue;
            }
            // actual == 0
            if (NULL != bufferQueue) {
                interface_lock_shared(bufferQueue);
                oldFront = bufferQueue->mFront;
                rear = bufferQueue->mRear;
                if (oldFront != rear) {
got_one:
                    assert(0 < bufferQueue->mState.count);
                    track->mReader = oldFront->mBuffer;
                    track->mAvail = oldFront->mSize;
                    interface_unlock_shared(bufferQueue);
                    continue;
                }
                // FIXME should be able to configure when to
                // kick off the callback e.g. high/low water-marks etc.
                // need data but none available, attempt a desperate callback
                slBufferQueueCallback callback = bufferQueue->mCallback;
                void *context = bufferQueue->mContext;
                interface_unlock_shared(bufferQueue);
                if (NULL != callback) {
                    (*callback)((SLBufferQueueItf) bufferQueue, context);
                    // if lucky, the callback enqueued a buffer
                    interface_lock_shared(bufferQueue);
                    if (rear != bufferQueue->mRear)
                        goto got_one;
                    interface_unlock_shared(bufferQueue);
                    // unlucky, queue still empty, the callback failed
                }
                // here on underflow due to no callback, or failed callback
                // FIXME underflow, send silence (or previous buffer?)
                // we did a callback to try to kick start again but failed
                // should log this
            }
            // no buffer queue or underflow, clear out rest of partial buffer
            if (!mixBufferHasData && trackContributedToMix)
                memset(dstWriter, 0, actual);
            break;
        }
        if (trackContributedToMix)
            mixBufferHasData = SL_BOOLEAN_TRUE;
    }
    // No active tracks, so output silence
    if (!mixBufferHasData)
        memset(pBuffer, 0, size);
}

static const struct SLOutputMixExtItf_ IOutputMixExt_Itf = {
    IOutputMixExt_FillBuffer
};

void IOutputMixExt_init(void *self)
{
    IOutputMixExt *this = (IOutputMixExt *) self;
    this->mItf = &IOutputMixExt_Itf;
}

SLresult IOutputMixExt_checkAudioPlayerSourceSink(CAudioPlayer *this)
{
    const SLDataSink *pAudioSnk = &this->mDataSink.u.mSink;
    struct Track *track = NULL;
    switch (*(SLuint32 *)pAudioSnk->pLocator) {
    case SL_DATALOCATOR_OUTPUTMIX:
        {
        // pAudioSnk->pFormat is ignored
        IOutputMix *om = &((COutputMix *) ((SLDataLocator_OutputMix *) pAudioSnk->pLocator)->outputMix)->mOutputMix;
        // allocate an entry within OutputMix for this track
        interface_lock_exclusive(om);
        unsigned availMask = ~om->mActiveMask;
        if (!availMask) {
            interface_unlock_exclusive(om);
            // All track slots full in output mix
            return SL_RESULT_MEMORY_FAILURE;
        }
        unsigned i = ctz(availMask);
        assert(MAX_TRACK > i);
        om->mActiveMask |= 1 << i;
        track = &om->mTracks[i];
        track->mPlay = NULL;    // only field that is accessed before full initialization
        interface_unlock_exclusive(om);
        }
        break;
    default:
        return SL_RESULT_CONTENT_UNSUPPORTED;
    }

    // FIXME Wrong place for this initialization; should first pre-allocate a track slot
    // using OutputMixExt.mTrackCount, then initialize full audio player, then do track bit
    // allocation, initialize rest of track, and doubly-link track to player (currently single).
    assert(NULL != track);
    track->mBufferQueue = &this->mBufferQueue;
    track->mPlay = &this->mPlay;
    track->mReader = NULL;
    track->mAvail = 0;
    return SL_RESULT_SUCCESS;
}

#endif // USE_OUTPUTMIXEXT
