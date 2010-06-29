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
#include <math.h>

#ifdef USE_OUTPUTMIXEXT

// Used by SDL but not specific to or dependent on SDL

// Summary of the gain, as an optimization for the mixer

typedef enum {
    GAIN_MUTE  = 0,  // mValue == 0.0f within epsilon
    GAIN_UNITY = 1,  // mValue == 1.0f within epsilon
    GAIN_OTHER = 2   // 0.0f < mValue < 1.0f
} Summary;

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
        CAudioPlayer *audioPlayer = track->mAudioPlayer;
        if (NULL == audioPlayer)
            continue;
        // track is initialized
        if (SL_PLAYSTATE_PLAYING != audioPlayer->mPlay.mState)
            continue;
        // track is playing
        void *dstWriter = pBuffer;
        unsigned desired = size;
        SLboolean trackContributedToMix = SL_BOOLEAN_FALSE;
        IBufferQueue *bufferQueue = track->mBufferQueue;
        float gains[STEREO_CHANNELS];
        Summary summaries[STEREO_CHANNELS];
        unsigned channel;
        for (channel = 0; channel < STEREO_CHANNELS; ++channel) {
            float gain = track->mGains[channel];
            gains[channel] = gain;
            Summary summary;
            if (gain <= 0.001)
                summary = GAIN_MUTE;
            else if (gain >= 0.999)
                summary = GAIN_UNITY;
            else
                summary = GAIN_OTHER;
            summaries[channel] = summary;
        }
        while (desired > 0) {
            const BufferHeader *oldFront, *newFront, *rear;
            unsigned actual = desired;
            if (track->mAvail < actual)
                actual = track->mAvail;
            // force actual to be a frame multiple
            if (actual > 0) {
                assert(NULL != track->mReader);
                stereo *mixBuffer = (stereo *) dstWriter;
                const stereo *source = (const stereo *) track->mReader;
                unsigned j;
                if (GAIN_MUTE != summaries[0] || GAIN_MUTE != summaries[1]) {
                    if (mixBufferHasData) {
                        // apply gain during add
                        if (GAIN_UNITY != summaries[0] || GAIN_UNITY != summaries[1]) {
                            for (j = 0; j < actual; j += sizeof(stereo), ++mixBuffer, ++source) {
                                mixBuffer->left += (short) (source->left * track->mGains[0]);
                                mixBuffer->right += (short) (source->right * track->mGains[1]);
                            }
                        // no gain adjustment needed, so do a simple add
                        } else {
                            for (j = 0; j < actual; j += sizeof(stereo), ++mixBuffer, ++source) {
                                mixBuffer->left += source->left;
                                mixBuffer->right += source->right;
                            }
                        }
                    } else {
                        // apply gain during copy
                        if (GAIN_UNITY != summaries[0] || GAIN_UNITY != summaries[1]) {
                            for (j = 0; j < actual; j += sizeof(stereo), ++mixBuffer, ++source) {
                                mixBuffer->left = (short) (source->left * track->mGains[0]);
                                mixBuffer->right = (short) (source->right * track->mGains[1]);
                            }
                        // no gain adjustment needed, so do a simple copy
                        } else {
                            memcpy(dstWriter, track->mReader, actual);
                        }
                    }
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
                        ++bufferQueue->mState.playIndex;
                        interface_unlock_exclusive(bufferQueue);
                        // A good place to do an early warning callback depending on buffer count
                    }
                }
                // no lock, but safe b/c noone else updates this field
                track->mFrameCounter += actual >> 2;    // sizeof(short) * STEREO_CHANNELS
                // FIXME Calling the callback too often, should depend on requested update period
                if (audioPlayer->mPlay.mCallback)
                    (*audioPlayer->mPlay.mCallback)(&audioPlayer->mPlay.mItf,
                        audioPlayer->mPlay.mContext, SL_PLAYEVENT_HEADMOVING);
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
    this->mTrack = NULL;
    const SLDataSink *pAudioSnk = &this->mDataSink.u.mSink;
    struct Track *track = NULL;
    switch (*(SLuint32 *)pAudioSnk->pLocator) {
    case SL_DATALOCATOR_OUTPUTMIX:
        {
        // pAudioSnk->pFormat is ignored
        IOutputMix *om = &((COutputMix *) ((SLDataLocator_OutputMix *)
            pAudioSnk->pLocator)->outputMix)->mOutputMix;
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
        track->mAudioPlayer = NULL;    // only field that is accessed before full initialization
        interface_unlock_exclusive(om);
        this->mTrack = track;
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
    track->mAudioPlayer = this;
    track->mReader = NULL;
    track->mAvail = 0;
    track->mGains[0] = 1.0f;
    track->mGains[1] = 1.0f;
    track->mFrameCounter = 0;
    return SL_RESULT_SUCCESS;
}

void audioPlayerGainUpdate(CAudioPlayer *audioPlayer)
{
    // FIXME need a lock on the track while updating gain
    struct Track *track = audioPlayer->mTrack;

    if (NULL == track)
        return;

    SLboolean mute = audioPlayer->mVolume.mMute;
    SLuint8 muteMask = audioPlayer->mMuteMask;
    SLuint8 soloMask = audioPlayer->mSoloMask;
    SLmillibel level = audioPlayer->mVolume.mLevel;
    SLboolean enableStereoPosition = audioPlayer->mVolume.mEnableStereoPosition;
    SLpermille stereoPosition = audioPlayer->mVolume.mStereoPosition;

    if (soloMask)
        muteMask |= ~soloMask;
    if (mute || !(~muteMask & 3)) {
        track->mGains[0] = 0.0;
        track->mGains[1] = 0.0;
    } else {
        float playerGain = pow(10.0, level / 2000.0);
        unsigned channel;
        for (channel = 0; channel < STEREO_CHANNELS; ++channel) {
            float gain;
            if (muteMask & (1 << channel))
                gain = 0.0;
            else {
                gain = playerGain;
                if (enableStereoPosition) {
                    switch (channel) {
                    case 0:
                        if (stereoPosition > 0)
                            gain *= (1000 - stereoPosition) / 1000.0;
                        break;
                    case 1:
                        if (stereoPosition < 0)
                            gain *= (1000 + stereoPosition) / 1000.0;
                        break;
                    default:
                        assert(SL_BOOLEAN_FALSE);
                        break;
                    }
                }
            }
            track->mGains[channel] = gain;
        }
    }
}

#endif // USE_OUTPUTMIXEXT
