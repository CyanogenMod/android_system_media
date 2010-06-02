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
    IOutputMix *this = &((COutputMix *) thisExt->mThis)->mOutputMix;
    unsigned activeMask = this->mActiveMask;
    struct Track *track = this->mTracks;
    unsigned i;
    SLboolean mixBufferHasData = SL_BOOLEAN_FALSE;
    // FIXME O(32) loop even when few tracks are active.
    // To avoid loop, use activeMask to check for active track(s)
    // and decide whether we actually need to copy or mix.
    for (i = 0; 0 != activeMask; ++i, ++track, activeMask >>= 1) {
        assert(i < 32);
        if (!(activeMask & 1))
            continue;
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
        while (desired > 0) {
            IBufferQueue *bufferQueue;
            const struct BufferHeader *oldFront, *newFront, *rear;
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
                    bufferQueue = track->mBufferQueue;
                    if (NULL != bufferQueue) {
                        oldFront = bufferQueue->mFront;
                        rear = bufferQueue->mRear;
                        assert(oldFront != rear);
                        newFront = oldFront;
                        if (++newFront == &bufferQueue->mArray[bufferQueue->mNumBuffers])
                            newFront = bufferQueue->mArray;
                        bufferQueue->mFront = (struct BufferHeader *) newFront;
                        assert(0 < bufferQueue->mState.count);
                        --bufferQueue->mState.count;
                        // FIXME here or in Enqueue?
                        ++bufferQueue->mState.playIndex;
                        // FIXME a good time to do an early warning
                        // callback depending on buffer count
                    }
                }
                continue;
            }
            // actual == 0
            bufferQueue = track->mBufferQueue;
            if (NULL != bufferQueue) {
                oldFront = bufferQueue->mFront;
                rear = bufferQueue->mRear;
                if (oldFront != rear) {
got_one:
                    assert(0 < bufferQueue->mState.count);
                    track->mReader = oldFront->mBuffer;
                    track->mAvail = oldFront->mSize;
                    continue;
                }
                // FIXME should be able to configure when to
                // kick off the callback e.g. high/low water-marks etc.
                // need data but none available, attempt a desperate callback
                slBufferQueueCallback callback = bufferQueue->mCallback;
                if (NULL != callback) {
                    (*callback)((SLBufferQueueItf) bufferQueue, bufferQueue->mContext);
                    // if lucky, the callback enqueued a buffer
                    if (rear != bufferQueue->mRear)
                        goto got_one;
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
    //const SLDataSource *pAudioSrc = &this->mDataSource.u.mSource;
    const SLDataSink *pAudioSnk = &this->mDataSink.u.mSink;
    struct Track *track = NULL;
    switch (*(SLuint32 *)pAudioSnk->pLocator) {
    case SL_DATALOCATOR_OUTPUTMIX:
        {
        // pAudioSnk->pFormat is ignored
        IOutputMix *om = &((COutputMix *) ((SLDataLocator_OutputMix *) pAudioSnk->pLocator)->outputMix)->mOutputMix;
        // allocate an entry within OutputMix for this track
        // FIXME O(n)
        unsigned i;
        for (i = 0, track = &om->mTracks[0]; i < 32; ++i, ++track) {
            if (om->mActiveMask & (1 << i))
                continue;
            om->mActiveMask |= 1 << i;
            break;
        }
        if (32 <= i) {
            // FIXME Need a better error code for all slots full in output mix
            return SL_RESULT_MEMORY_FAILURE;
        }
        }
        break;
    default:
        return SL_RESULT_CONTENT_UNSUPPORTED;
    }

    track->mBufferQueue = &this->mBufferQueue;
    track->mPlay = &this->mPlay;
    // next 2 fields must be initialized explicitly (not part of this)
    track->mReader = NULL;
    track->mAvail = 0;
    return SL_RESULT_SUCCESS;
}

#endif // USE_OUTPUTMIXEXT
