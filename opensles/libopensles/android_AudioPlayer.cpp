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

#include "sles_allinclusive.h"
#include "utils/RefBase.h"
#include "android_prompts.h"

#define KEY_STREAM_TYPE_PARAMSIZE  sizeof(SLint32)

//-----------------------------------------------------------------------------
int android_getMinFrameCount(uint32_t sampleRate) {
    int afSampleRate;
    if (android::AudioSystem::getOutputSamplingRate(&afSampleRate,
            ANDROID_DEFAULT_OUTPUT_STREAM_TYPE) != android::NO_ERROR) {
        return ANDROID_DEFAULT_AUDIOTRACK_BUFFER_SIZE;
    }
    int afFrameCount;
    if (android::AudioSystem::getOutputFrameCount(&afFrameCount,
            ANDROID_DEFAULT_OUTPUT_STREAM_TYPE) != android::NO_ERROR) {
        return ANDROID_DEFAULT_AUDIOTRACK_BUFFER_SIZE;
    }
    uint32_t afLatency;
    if (android::AudioSystem::getOutputLatency(&afLatency,
            ANDROID_DEFAULT_OUTPUT_STREAM_TYPE) != android::NO_ERROR) {
        return ANDROID_DEFAULT_AUDIOTRACK_BUFFER_SIZE;
    }
    // minimum nb of buffers to cover output latency, given the size of each hardware audio buffer
    uint32_t minBufCount = afLatency / ((1000 * afFrameCount)/afSampleRate);
    if (minBufCount < 2) minBufCount = 2;
    // minimum number of frames to cover output latency at the sample rate of the content
    return (afFrameCount*sampleRate*minBufCount)/afSampleRate;
}


//-----------------------------------------------------------------------------
#define LEFT_CHANNEL_MASK  0x1 << 0
#define RIGHT_CHANNEL_MASK 0x1 << 1

static void android_audioPlayer_updateStereoVolume(CAudioPlayer* ap) {
    float leftVol = 1.0f, rightVol = 1.0f;

    if (NULL == ap->mAudioTrack) {
        return;
    }
    // should not be used when muted
    if (SL_BOOLEAN_TRUE == ap->mMute) {
        return;
    }

    int channelCount = ap->mNumChannels;

    // mute has priority over solo
    int leftAudibilityFactor = 1, rightAudibilityFactor = 1;

    if (channelCount >= STEREO_CHANNELS) {
        if (ap->mMuteMask & LEFT_CHANNEL_MASK) {
            // left muted
            leftAudibilityFactor = 0;
        } else {
            // left not muted
            if (ap->mSoloMask & LEFT_CHANNEL_MASK) {
                // left soloed
                leftAudibilityFactor = 1;
            } else {
                // left not soloed
                if (ap->mSoloMask & RIGHT_CHANNEL_MASK) {
                    // right solo silences left
                    leftAudibilityFactor = 0;
                } else {
                    // left and right are not soloed, and left is not muted
                    leftAudibilityFactor = 1;
                }
            }
        }

        if (ap->mMuteMask & RIGHT_CHANNEL_MASK) {
            // right muted
            rightAudibilityFactor = 0;
        } else {
            // right not muted
            if (ap->mSoloMask & RIGHT_CHANNEL_MASK) {
                // right soloed
                rightAudibilityFactor = 1;
            } else {
                // right not soloed
                if (ap->mSoloMask & LEFT_CHANNEL_MASK) {
                    // left solo silences right
                    rightAudibilityFactor = 0;
                } else {
                    // left and right are not soloed, and right is not muted
                    rightAudibilityFactor = 1;
                }
            }
        }
    }

    // compute amplification as the combination of volume level and stereo position
    //   amplification from volume level
    ap->mAmplFromVolLevel = sles_to_android_amplification(ap->mVolume.mLevel);
    leftVol  *= ap->mAmplFromVolLevel * ap->mAmplFromDirectLevel;
    rightVol *= ap->mAmplFromVolLevel * ap->mAmplFromDirectLevel;

    // amplification from stereo position
    if (ap->mVolume.mEnableStereoPosition) {
        // panning law depends on number of channels of content: stereo panning vs 2ch. balance
        if(1 == channelCount) {
            // stereo panning
            double theta = (1000+ap->mVolume.mStereoPosition)*M_PI_4/1000.0f; // 0 <= theta <= Pi/2
            ap->mAmplFromStereoPos[0] = cos(theta);
            ap->mAmplFromStereoPos[1] = sin(theta);
        } else {
            // stereo balance
            if (ap->mVolume.mStereoPosition > 0) {
                ap->mAmplFromStereoPos[0] = (1000-ap->mVolume.mStereoPosition)/1000.0f;
                ap->mAmplFromStereoPos[1] = 1.0f;
            } else {
                ap->mAmplFromStereoPos[0] = 1.0f;
                ap->mAmplFromStereoPos[1] = (1000+ap->mVolume.mStereoPosition)/1000.0f;
            }
        }
        leftVol  *= ap->mAmplFromStereoPos[0];
        rightVol *= ap->mAmplFromStereoPos[1];
    }

    ap->mAudioTrack->setVolume(leftVol * leftAudibilityFactor, rightVol * rightAudibilityFactor);
}

//-----------------------------------------------------------------------------
void audioTrack_handleMarker_lockPlay(CAudioPlayer* ap) {
    //SL_LOGV("received event EVENT_MARKER from AudioTrack");
    slPlayCallback callback = NULL;
    void* callbackPContext = NULL;

    interface_lock_shared(&ap->mPlay);
    callback = ap->mPlay.mCallback;
    callbackPContext = ap->mPlay.mContext;
    interface_unlock_shared(&ap->mPlay);

    if (NULL != callback) {
        // getting this event implies SL_PLAYEVENT_HEADATMARKER was set in the event mask
        (*callback)(&ap->mPlay.mItf, callbackPContext, SL_PLAYEVENT_HEADATMARKER);
    }
}

//-----------------------------------------------------------------------------
void audioTrack_handleNewPos_lockPlay(CAudioPlayer* ap) {
    //SL_LOGV("received event EVENT_NEW_POS from AudioTrack");
    slPlayCallback callback = NULL;
    void* callbackPContext = NULL;

    interface_lock_shared(&ap->mPlay);
    callback = ap->mPlay.mCallback;
    callbackPContext = ap->mPlay.mContext;
    interface_unlock_shared(&ap->mPlay);

    if (NULL != callback) {
        // getting this event implies SL_PLAYEVENT_HEADATNEWPOS was set in the event mask
        (*callback)(&ap->mPlay.mItf, callbackPContext, SL_PLAYEVENT_HEADATNEWPOS);
    }
}


//-----------------------------------------------------------------------------
void audioTrack_handleUnderrun_lockPlay(CAudioPlayer* ap) {
    slPlayCallback callback = NULL;
    void* callbackPContext = NULL;

    interface_lock_shared(&ap->mPlay);
    callback = ap->mPlay.mCallback;
    callbackPContext = ap->mPlay.mContext;
    bool headStalled = (ap->mPlay.mEventFlags & SL_PLAYEVENT_HEADSTALLED) != 0;
    interface_unlock_shared(&ap->mPlay);

    if ((NULL != callback) && headStalled) {
        (*callback)(&ap->mPlay.mItf, callbackPContext, SL_PLAYEVENT_HEADSTALLED);
    }
}

//-----------------------------------------------------------------------------
/**
 * post-condition: play state of AudioPlayer is SL_PLAYSTATE_PAUSED if setPlayStateToPaused is true
 *
 * note: a conditional flag, setPlayStateToPaused, is used here to specify whether the play state
 *       needs to be changed when the player reaches the end of the content to play. This is
 *       relative to what the specification describes for buffer queues vs the
 *       SL_PLAYEVENT_HEADATEND event. In the OpenSL ES specification 1.0.1:
 *        - section 8.12 SLBufferQueueItf states "In the case of starvation due to insufficient
 *          buffers in the queue, the playing of audio data stops. The player remains in the
 *          SL_PLAYSTATE_PLAYING state."
 *        - section 9.2.31 SL_PLAYEVENT states "SL_PLAYEVENT_HEADATEND Playback head is at the end
 *          of the current content and the player has paused."
 */
void audioPlayer_dispatch_headAtEnd_lockPlay(CAudioPlayer *ap, bool setPlayStateToPaused,
        bool needToLock) {
    //SL_LOGV("ap=%p, setPlayStateToPaused=%d, needToLock=%d", ap, setPlayStateToPaused,
    //        needToLock);
    slPlayCallback playCallback = NULL;
    void * playContext = NULL;
    // SLPlayItf callback or no callback?
    if (needToLock) {
        interface_lock_exclusive(&ap->mPlay);
    }
    if (ap->mPlay.mEventFlags & SL_PLAYEVENT_HEADATEND) {
        playCallback = ap->mPlay.mCallback;
        playContext = ap->mPlay.mContext;
    }
    if (setPlayStateToPaused) {
        ap->mPlay.mState = SL_PLAYSTATE_PAUSED;
    }
    if (needToLock) {
        interface_unlock_exclusive(&ap->mPlay);
    }
    // callback with no lock held
    if (NULL != playCallback) {
        (*playCallback)(&ap->mPlay.mItf, playContext, SL_PLAYEVENT_HEADATEND);
    }

}


//-----------------------------------------------------------------------------
/**
 * pre-condition: AudioPlayer has SLPrefetchStatusItf initialized
 * post-condition:
 *  - ap->mPrefetchStatus.mStatus == status
 *  - the prefetch status callback, if any, has been notified if a change occurred
 *
 */
void audioPlayer_dispatch_prefetchStatus_lockPrefetch(CAudioPlayer *ap, SLuint32 status,
        bool needToLock) {
    slPrefetchCallback prefetchCallback = NULL;
    void * prefetchContext = NULL;

    if (needToLock) {
        interface_lock_exclusive(&ap->mPrefetchStatus);
    }
    // status change?
    if (ap->mPrefetchStatus.mStatus != status) {
        ap->mPrefetchStatus.mStatus = status;
        // callback or no callback?
        if (ap->mPrefetchStatus.mCallbackEventsMask & SL_PREFETCHEVENT_STATUSCHANGE) {
            prefetchCallback = ap->mPrefetchStatus.mCallback;
            prefetchContext  = ap->mPrefetchStatus.mContext;
        }
    }
    if (needToLock) {
        interface_unlock_exclusive(&ap->mPrefetchStatus);
    }

    // callback with no lock held
    if (NULL != prefetchCallback) {
        (*prefetchCallback)(&ap->mPrefetchStatus.mItf, prefetchContext, status);
    }
}


//-----------------------------------------------------------------------------
SLresult audioPlayer_setStreamType(CAudioPlayer* ap, SLint32 type) {
    SLresult result = SL_RESULT_SUCCESS;
    SL_LOGV("type %ld", type);

    int newStreamType = ANDROID_DEFAULT_OUTPUT_STREAM_TYPE;
    switch(type) {
    case SL_ANDROID_STREAM_VOICE:
        newStreamType = android::AudioSystem::VOICE_CALL;
        break;
    case SL_ANDROID_STREAM_SYSTEM:
        newStreamType = android::AudioSystem::SYSTEM;
        break;
    case SL_ANDROID_STREAM_RING:
        newStreamType = android::AudioSystem::RING;
        break;
    case SL_ANDROID_STREAM_MEDIA:
        newStreamType = android::AudioSystem::MUSIC;
        break;
    case SL_ANDROID_STREAM_ALARM:
        newStreamType = android::AudioSystem::ALARM;
        break;
    case SL_ANDROID_STREAM_NOTIFICATION:
        newStreamType = android::AudioSystem::NOTIFICATION;
        break;
    default:
        SL_LOGE(ERROR_PLAYERSTREAMTYPE_SET_UNKNOWN_TYPE);
        result = SL_RESULT_PARAMETER_INVALID;
        break;
    }

    // stream type needs to be set before the object is realized
    // (ap->mAudioTrack is supposed to be NULL until then)
    if (SL_OBJECT_STATE_REALIZED == ap->mObject.mState) {
        SL_LOGE(ERROR_PLAYERSTREAMTYPE_REALIZED);
        result = SL_RESULT_PRECONDITIONS_VIOLATED;
    } else {
        ap->mStreamType = newStreamType;
    }

    return result;
}


//-----------------------------------------------------------------------------
SLresult audioPlayer_getStreamType(CAudioPlayer* ap, SLint32 *pType) {
    SLresult result = SL_RESULT_SUCCESS;

    switch(ap->mStreamType) {
    case android::AudioSystem::VOICE_CALL:
        *pType = SL_ANDROID_STREAM_VOICE;
        break;
    case android::AudioSystem::SYSTEM:
        *pType = SL_ANDROID_STREAM_SYSTEM;
        break;
    case android::AudioSystem::RING:
        *pType = SL_ANDROID_STREAM_RING;
        break;
    case android::AudioSystem::DEFAULT:
    case android::AudioSystem::MUSIC:
        *pType = SL_ANDROID_STREAM_MEDIA;
        break;
    case android::AudioSystem::ALARM:
        *pType = SL_ANDROID_STREAM_ALARM;
        break;
    case android::AudioSystem::NOTIFICATION:
        *pType = SL_ANDROID_STREAM_NOTIFICATION;
        break;
    default:
        result = SL_RESULT_INTERNAL_ERROR;
        *pType = SL_ANDROID_STREAM_MEDIA;
        break;
    }

    return result;
}


//-----------------------------------------------------------------------------
#ifndef USE_BACKPORT
// Callback associated with an SfPlayer of an SL ES AudioPlayer that gets its data
// from a URI or FD, for prefetching events
static void sfplayer_handlePrefetchEvent(const int event, const int data1, void* user) {
    if (NULL == user) {
        return;
    }

    CAudioPlayer *ap = (CAudioPlayer *)user;
    //SL_LOGV("received event %d, data %d from SfAudioPlayer", event, data1);
    switch(event) {

    case(android::SfPlayer::kEventPrefetchFillLevelUpdate): {
        if (!IsInterfaceInitialized(&(ap->mObject), MPH_PREFETCHSTATUS)) {
            break;
        }
        // FIXME implement buffer filler level updates
        SL_LOGE("[ FIXME implement buffer filler level updates ]");
        //ap->mPrefetchStatus.mLevel = ;
        } break;

    case(android::SfPlayer::kEventPrefetchStatusChange): {
        if (!IsInterfaceInitialized(&(ap->mObject), MPH_PREFETCHSTATUS)) {
            break;
        }
        slPrefetchCallback callback = NULL;
        void* callbackPContext = NULL;
        // SLPrefetchStatusItf callback or no callback?
        interface_lock_exclusive(&ap->mPrefetchStatus);
        if (ap->mPrefetchStatus.mCallbackEventsMask & SL_PREFETCHEVENT_STATUSCHANGE) {
            callback = ap->mPrefetchStatus.mCallback;
            callbackPContext = ap->mPrefetchStatus.mContext;
        }
        if (data1 >= android::SfPlayer::kStatusIntermediate) {
            ap->mPrefetchStatus.mStatus = SL_PREFETCHSTATUS_SUFFICIENTDATA;
            // FIXME estimate fill level better?
            ap->mPrefetchStatus.mLevel = 1000;
            ap->mAndroidObjState = ANDROID_READY;
        } else if (data1 < android::SfPlayer::kStatusIntermediate) {
            ap->mPrefetchStatus.mStatus = SL_PREFETCHSTATUS_UNDERFLOW;
            // FIXME estimate fill level better?
            ap->mPrefetchStatus.mLevel = 0;
        }
        interface_unlock_exclusive(&ap->mPrefetchStatus);
        // callback with no lock held
        if (NULL != callback) {
            (*callback)(&ap->mPrefetchStatus.mItf, callbackPContext, SL_PREFETCHEVENT_STATUSCHANGE);
        }
        } break;

    case(android::SfPlayer::kEventEndOfStream): {
        audioPlayer_dispatch_headAtEnd_lockPlay(ap, true /*set state to paused?*/, true);
        // FIXME update play position to make sure it's at the same value
        //       as getDuration if it's known?
        ap->mAudioTrack->stop();
        } break;

    default:
        break;
    }
}
#endif


//-----------------------------------------------------------------------------
SLresult android_audioPlayer_checkSourceSink(CAudioPlayer *pAudioPlayer)
{
    const SLDataSource *pAudioSrc = &pAudioPlayer->mDataSource.u.mSource;
    const SLDataSink *pAudioSnk = &pAudioPlayer->mDataSink.u.mSink;
    //--------------------------------------
    // Sink check:
    //     currently only OutputMix sinks are supported, regardless of the data source
    if (*(SLuint32 *)pAudioSnk->pLocator != SL_DATALOCATOR_OUTPUTMIX) {
        SL_LOGE("Cannot create audio player: data sink is not SL_DATALOCATOR_OUTPUTMIX");
        return SL_RESULT_PARAMETER_INVALID;
    }

    //--------------------------------------
    // Source check:
    SLuint32 locatorType = *(SLuint32 *)pAudioSrc->pLocator;
    SLuint32 formatType = *(SLuint32 *)pAudioSrc->pFormat;

    switch (locatorType) {
    //------------------
    //   Buffer Queues
    case SL_DATALOCATOR_BUFFERQUEUE: {
        SLDataLocator_BufferQueue *dl_bq =  (SLDataLocator_BufferQueue *) pAudioSrc->pLocator;

        // Buffer format
        switch (formatType) {
        //     currently only PCM buffer queues are supported,
        case SL_DATAFORMAT_PCM: {
            SLDataFormat_PCM *df_pcm = (SLDataFormat_PCM *) pAudioSrc->pFormat;
            switch (df_pcm->numChannels) {
            case 1:
            case 2:
                break;
            default:
                // this should have already been rejected by checkDataFormat
                SL_LOGE("Cannot create audio player: unsupported " \
                    "PCM data source with %u channels", (unsigned) df_pcm->numChannels);
                return SL_RESULT_CONTENT_UNSUPPORTED;
            }
            switch (df_pcm->samplesPerSec) {
            case SL_SAMPLINGRATE_8:
            case SL_SAMPLINGRATE_11_025:
            case SL_SAMPLINGRATE_12:
            case SL_SAMPLINGRATE_16:
            case SL_SAMPLINGRATE_22_05:
            case SL_SAMPLINGRATE_24:
            case SL_SAMPLINGRATE_32:
            case SL_SAMPLINGRATE_44_1:
                break;
            case SL_SAMPLINGRATE_48:    // not 48?
            case SL_SAMPLINGRATE_64:
            case SL_SAMPLINGRATE_88_2:
            case SL_SAMPLINGRATE_96:
            case SL_SAMPLINGRATE_192:
            default:
                SL_LOGE("Cannot create audio player: unsupported sample rate %u milliHz",
                    (unsigned) df_pcm->samplesPerSec);
                return SL_RESULT_CONTENT_UNSUPPORTED;
            }
            switch (df_pcm->bitsPerSample) {
            case SL_PCMSAMPLEFORMAT_FIXED_8:
                // FIXME We should support this
                SL_LOGE("Cannot create audio player: unsupported 8-bit data");
                return SL_RESULT_CONTENT_UNSUPPORTED;
            case SL_PCMSAMPLEFORMAT_FIXED_16:
                break;
                // others
            default:
                // this should have already been rejected by checkDataFormat
                SL_LOGE("Cannot create audio player: unsupported sample bit depth %lu",
                        (SLuint32)df_pcm->bitsPerSample);
                return SL_RESULT_CONTENT_UNSUPPORTED;
            }
            switch (df_pcm->containerSize) {
            case 16:
                break;
                // others
            default:
                SL_LOGE("Cannot create audio player: unsupported container size %u",
                    (unsigned) df_pcm->containerSize);
                return SL_RESULT_CONTENT_UNSUPPORTED;
            }
            switch (df_pcm->channelMask) {
                // FIXME needs work
            default:
                break;
            }
            switch (df_pcm->endianness) {
            case SL_BYTEORDER_LITTLEENDIAN:
                break;
            case SL_BYTEORDER_BIGENDIAN:
                SL_LOGE("Cannot create audio player: unsupported big-endian byte order");
                return SL_RESULT_CONTENT_UNSUPPORTED;
                // native is proposed but not yet in spec
            default:
                SL_LOGE("Cannot create audio player: unsupported byte order %u",
                    (unsigned) df_pcm->endianness);
                return SL_RESULT_CONTENT_UNSUPPORTED;
            }
            } //case SL_DATAFORMAT_PCM
            break;
        case SL_DATAFORMAT_MIME:
        case SL_DATAFORMAT_RESERVED3:
            SL_LOGE("Cannot create audio player with SL_DATALOCATOR_BUFFERQUEUE data source "
                "without SL_DATAFORMAT_PCM format");
            return SL_RESULT_CONTENT_UNSUPPORTED;
        default:
            SL_LOGE("Cannot create audio player with SL_DATALOCATOR_BUFFERQUEUE data source "
                "without SL_DATAFORMAT_PCM format");
            return SL_RESULT_PARAMETER_INVALID;
        } // switch (formatType)
        } // case SL_DATALOCATOR_BUFFERQUEUE
        break;
    //------------------
    //   URI
    case SL_DATALOCATOR_URI:
        {
        SLDataLocator_URI *dl_uri =  (SLDataLocator_URI *) pAudioSrc->pLocator;
        if (NULL == dl_uri->URI) {
            return SL_RESULT_PARAMETER_INVALID;
        }
        // URI format
        switch (formatType) {
        case SL_DATAFORMAT_MIME:
            break;
        case SL_DATAFORMAT_PCM:
        case SL_DATAFORMAT_RESERVED3:
            SL_LOGE("Cannot create audio player with SL_DATALOCATOR_URI data source without "
                "SL_DATAFORMAT_MIME format");
            return SL_RESULT_CONTENT_UNSUPPORTED;
        } // switch (formatType)
        } // case SL_DATALOCATOR_URI
        break;
    //------------------
    //   File Descriptor
    case SL_DATALOCATOR_ANDROIDFD:
        {
        // fd is already non null
        switch (formatType) {
        case SL_DATAFORMAT_MIME:
            break;
        case SL_DATAFORMAT_PCM:
            // FIXME implement
            SL_LOGE("[ FIXME implement PCM FD data sources ]");
            break;
        case SL_DATAFORMAT_RESERVED3:
            SL_LOGE("Cannot create audio player with SL_DATALOCATOR_ANDROIDFD data source "
                "without SL_DATAFORMAT_MIME or SL_DATAFORMAT_PCM format");
            return SL_RESULT_CONTENT_UNSUPPORTED;
        } // switch (formatType)
        } // case SL_DATALOCATOR_ANDROIDFD
        break;
    //------------------
    //   Address
    case SL_DATALOCATOR_ADDRESS:
    case SL_DATALOCATOR_IODEVICE:
    case SL_DATALOCATOR_OUTPUTMIX:
    case SL_DATALOCATOR_RESERVED5:
    case SL_DATALOCATOR_MIDIBUFFERQUEUE:
    case SL_DATALOCATOR_RESERVED8:
        SL_LOGE("Cannot create audio player with data locator type 0x%x", (unsigned) locatorType);
        return SL_RESULT_CONTENT_UNSUPPORTED;
    default:
        return SL_RESULT_PARAMETER_INVALID;
    }// switch (locatorType)

    return SL_RESULT_SUCCESS;
}



//-----------------------------------------------------------------------------
static void audioTrack_callBack_uri(int event, void* user, void *info) {
    // EVENT_MORE_DATA needs to be handled with priority over the other events
    // because it will be called the most often during playback
    if (event == android::AudioTrack::EVENT_MORE_DATA) {
        //SL_LOGV("received event EVENT_MORE_DATA from AudioTrack");
        // set size to 0 to signal we're not using the callback to write more data
        android::AudioTrack::Buffer* pBuff = (android::AudioTrack::Buffer*)info;
        pBuff->size = 0;
    } else if (NULL != user) {
        switch (event) {
            case (android::AudioTrack::EVENT_MARKER) :
                audioTrack_handleMarker_lockPlay((CAudioPlayer *)user);
                break;
            case (android::AudioTrack::EVENT_NEW_POS) :
                audioTrack_handleNewPos_lockPlay((CAudioPlayer *)user);
                break;
            case (android::AudioTrack::EVENT_UNDERRUN) :
                audioTrack_handleUnderrun_lockPlay((CAudioPlayer *)user);
                break;
            default:
                SL_LOGE("Encountered unknown AudioTrack event %d for CAudioPlayer %p", event,
                        (CAudioPlayer *)user);
                break;
        }
    }
}

//-----------------------------------------------------------------------------
// Callback associated with an AudioTrack of an SL ES AudioPlayer that gets its data
// from a buffer queue.
static void audioTrack_callBack_pullFromBuffQueue(int event, void* user, void *info) {
    CAudioPlayer *ap = (CAudioPlayer *)user;
    void * callbackPContext = NULL;
    switch(event) {

    case (android::AudioTrack::EVENT_MORE_DATA) : {
        //SL_LOGV("received event EVENT_MORE_DATA from AudioTrack");
        slBufferQueueCallback callback = NULL;
        android::AudioTrack::Buffer* pBuff = (android::AudioTrack::Buffer*)info;
        // retrieve data from the buffer queue
        interface_lock_exclusive(&ap->mBufferQueue);
        if (ap->mBufferQueue.mState.count != 0) {
            //SL_LOGV("nbBuffers in queue = %lu",ap->mBufferQueue.mState.count);
            assert(ap->mBufferQueue.mFront != ap->mBufferQueue.mRear);

            BufferHeader *oldFront = ap->mBufferQueue.mFront;
            BufferHeader *newFront = &oldFront[1];

            // FIXME handle 8bit based on buffer format
            short *pSrc = (short*)((char *)oldFront->mBuffer
                    + ap->mBufferQueue.mSizeConsumed);
            if (ap->mBufferQueue.mSizeConsumed + pBuff->size < oldFront->mSize) {
                // can't consume the whole or rest of the buffer in one shot
                ap->mBufferQueue.mSizeConsumed += pBuff->size;
                // leave pBuff->size untouched
                // consume data
                // FIXME can we avoid holding the lock during the copy?
                memcpy (pBuff->i16, pSrc, pBuff->size);
            } else {
                // finish consuming the buffer or consume the buffer in one shot
                pBuff->size = oldFront->mSize - ap->mBufferQueue.mSizeConsumed;
                ap->mBufferQueue.mSizeConsumed = 0;

                if (newFront ==
                        &ap->mBufferQueue.mArray
                            [ap->mBufferQueue.mNumBuffers + 1])
                {
                    newFront = ap->mBufferQueue.mArray;
                }
                ap->mBufferQueue.mFront = newFront;

                ap->mBufferQueue.mState.count--;
                ap->mBufferQueue.mState.playIndex++;

                // consume data
                // FIXME can we avoid holding the lock during the copy?
                memcpy (pBuff->i16, pSrc, pBuff->size);

                // data has been consumed, and the buffer queue state has been updated
                // we will notify the client if applicable
                callback = ap->mBufferQueue.mCallback;
                // save callback data
                callbackPContext = ap->mBufferQueue.mContext;
            }
        } else { // empty queue
            // signal no data available
            pBuff->size = 0;

            // signal we're at the end of the content, but don't pause (see note in function)
            audioPlayer_dispatch_headAtEnd_lockPlay(ap, false /*set state to paused?*/, false);

            // signal underflow to prefetch status itf
            if (IsInterfaceInitialized(&(ap->mObject), MPH_PREFETCHSTATUS)) {
                audioPlayer_dispatch_prefetchStatus_lockPrefetch(ap, SL_PREFETCHSTATUS_UNDERFLOW,
                    false);
            }

            // stop the track so it restarts playing faster when new data is enqueued
            ap->mAudioTrack->stop();
        }
        interface_unlock_exclusive(&ap->mBufferQueue);
        // notify client
        if (NULL != callback) {
            (*callback)(&ap->mBufferQueue.mItf, callbackPContext);
        }
    }
    break;

    case (android::AudioTrack::EVENT_MARKER) :
        audioTrack_handleMarker_lockPlay(ap);
        break;

    case (android::AudioTrack::EVENT_NEW_POS) :
        audioTrack_handleNewPos_lockPlay(ap);
        break;

    case (android::AudioTrack::EVENT_UNDERRUN) :
        audioTrack_handleUnderrun_lockPlay(ap);
        break;

    default:
        // FIXME where does the notification of SL_PLAYEVENT_HEADMOVING fit?
        SL_LOGE("Encountered unknown AudioTrack event %d for CAudioPlayer %p", event,
                (CAudioPlayer *)user);
        break;
    }
}


//-----------------------------------------------------------------------------
SLresult android_audioPlayer_create(
        CAudioPlayer *pAudioPlayer) {

    const SLDataSource *pAudioSrc = &pAudioPlayer->mDataSource.u.mSource;
    const SLDataSink *pAudioSnk = &pAudioPlayer->mDataSink.u.mSink;
    SLresult result = SL_RESULT_SUCCESS;

    //--------------------------------------
    // Sink check:
    // currently only OutputMix sinks are supported
    // this has already been verified in sles_to_android_CheckAudioPlayerSourceSink
    // SLuint32 locatorType = *(SLuint32 *)pAudioSnk->pLocator;
    // if (SL_DATALOCATOR_OUTPUTMIX == locatorType) {
    // }

    //--------------------------------------
    // Source check:
    SLuint32 locatorType = *(SLuint32 *)pAudioSrc->pLocator;
    switch (locatorType) {
    //   -----------------------------------
    //   Buffer Queue to AudioTrack
    case SL_DATALOCATOR_BUFFERQUEUE:
        pAudioPlayer->mAndroidObjType = AUDIOTRACK_PULL;
        pAudioPlayer->mpLock = new android::Mutex();
        pAudioPlayer->mPlaybackRate.mCapabilities = SL_RATEPROP_NOPITCHCORAUDIO;
        break;
    //   -----------------------------------
    //   URI or FD to MediaPlayer
    case SL_DATALOCATOR_URI:
    case SL_DATALOCATOR_ANDROIDFD:
        pAudioPlayer->mAndroidObjType = MEDIAPLAYER;
        pAudioPlayer->mpLock = new android::Mutex();
        pAudioPlayer->mPlaybackRate.mCapabilities = SL_RATEPROP_NOPITCHCORAUDIO;
        break;
    default:
        pAudioPlayer->mAndroidObjType = INVALID_TYPE;
        pAudioPlayer->mpLock = NULL;
        pAudioPlayer->mPlaybackRate.mCapabilities = 0;
        result = SL_RESULT_PARAMETER_INVALID;
        break;
    }

    pAudioPlayer->mAndroidObjState = ANDROID_UNINITIALIZED;
    pAudioPlayer->mStreamType = ANDROID_DEFAULT_OUTPUT_STREAM_TYPE;
    pAudioPlayer->mAudioTrack = NULL;
#ifndef USE_BACKPORT
    pAudioPlayer->mSfPlayer.clear();
#endif

#ifndef USE_BACKPORT
    pAudioPlayer->mSessionId = android::AudioSystem::newAudioSessionId();
#endif

    pAudioPlayer->mAmplFromVolLevel = 1.0f;
    pAudioPlayer->mAmplFromStereoPos[0] = 1.0f;
    pAudioPlayer->mAmplFromStereoPos[1] = 1.0f;
    pAudioPlayer->mDirectLevel = 0; // no attenuation
    pAudioPlayer->mAmplFromDirectLevel = 1.0f; // matches initial mDirectLevel value

    pAudioPlayer->mAndroidEffect.mEffects =
            new android::KeyedVector<SLuint32, android::AudioEffect* >();

    return result;

}


//-----------------------------------------------------------------------------
SLresult android_audioPlayer_setConfig(CAudioPlayer *ap, const SLchar *configKey,
        const void *pConfigValue, SLuint32 valueSize) {

    SLresult result = SL_RESULT_SUCCESS;

    if (NULL == ap) {
        result = SL_RESULT_INTERNAL_ERROR;
    } else if (NULL == pConfigValue) {
        SL_LOGE(ERROR_CONFIG_NULL_PARAM);
        result = SL_RESULT_PARAMETER_INVALID;

    } else if(strcmp((const char*)configKey, (const char*)SL_ANDROID_KEY_STREAM_TYPE) == 0) {

        // stream type
        if (KEY_STREAM_TYPE_PARAMSIZE > valueSize) {
            SL_LOGE(ERROR_CONFIG_VALUESIZE_TOO_LOW);
            result = SL_RESULT_PARAMETER_INVALID;
        } else {
            result = audioPlayer_setStreamType(ap, *(SLuint32*)pConfigValue);
        }

    } else {
        SL_LOGE(ERROR_CONFIG_UNKNOWN_KEY);
        result = SL_RESULT_PARAMETER_INVALID;
    }

    return result;
}


//-----------------------------------------------------------------------------
SLresult android_audioPlayer_getConfig(CAudioPlayer* ap, const SLchar *configKey,
        SLuint32* pValueSize, void *pConfigValue) {

    SLresult result = SL_RESULT_SUCCESS;

    if (NULL == ap) {
        return SL_RESULT_INTERNAL_ERROR;
    } else if (NULL == pValueSize) {
        SL_LOGE(ERROR_CONFIG_NULL_PARAM);
        result = SL_RESULT_PARAMETER_INVALID;

    } else if(strcmp((const char*)configKey, (const char*)SL_ANDROID_KEY_STREAM_TYPE) == 0) {

        // stream type
        if (KEY_STREAM_TYPE_PARAMSIZE > *pValueSize) {
            SL_LOGE(ERROR_CONFIG_VALUESIZE_TOO_LOW);
            result = SL_RESULT_PARAMETER_INVALID;
        } else {
            *pValueSize = KEY_STREAM_TYPE_PARAMSIZE;
            if (NULL != pConfigValue) {
                result = audioPlayer_getStreamType(ap, (SLint32*)pConfigValue);
            }
        }

    } else {
        SL_LOGE(ERROR_CONFIG_UNKNOWN_KEY);
        result = SL_RESULT_PARAMETER_INVALID;
    }

    return result;
}


//-----------------------------------------------------------------------------
SLresult android_audioPlayer_realize(CAudioPlayer *pAudioPlayer, SLboolean async) {

    SLresult result = SL_RESULT_SUCCESS;
    SL_LOGV("pAudioPlayer=%p", pAudioPlayer);

    switch (pAudioPlayer->mAndroidObjType) {
    //-----------------------------------
    // AudioTrack
    case AUDIOTRACK_PUSH:
    case AUDIOTRACK_PULL:
        {
        // initialize platform-specific CAudioPlayer fields

        SLDataLocator_BufferQueue *dl_bq =  (SLDataLocator_BufferQueue *)
                pAudioPlayer->mDynamicSource.mDataSource;
        SLDataFormat_PCM *df_pcm = (SLDataFormat_PCM *)
                pAudioPlayer->mDynamicSource.mDataSource->pFormat;

        uint32_t sampleRate = sles_to_android_sampleRate(df_pcm->samplesPerSec);

        pAudioPlayer->mAudioTrack = new android::AudioTrack(
                pAudioPlayer->mStreamType,                           // streamType
                sampleRate,                                          // sampleRate
                sles_to_android_sampleFormat(df_pcm->bitsPerSample), // format
                sles_to_android_channelMask(df_pcm->numChannels, df_pcm->channelMask),//channel mask
                0,                                                   // frameCount (here min)
                0,                                                   // flags
                audioTrack_callBack_pullFromBuffQueue,               // callback
                (void *) pAudioPlayer,                               // user
                0);  // FIXME find appropriate frame count         // notificationFrame
        android::status_t status = pAudioPlayer->mAudioTrack->initCheck();
        if (status != android::NO_ERROR) {
            SL_LOGE("AudioTrack::initCheck status %u", status);
            result = SL_RESULT_CONTENT_UNSUPPORTED;
        }

        // initialize platform-independent CAudioPlayer fields

        pAudioPlayer->mNumChannels = df_pcm->numChannels;
        pAudioPlayer->mSampleRateMilliHz = df_pcm->samplesPerSec; // Note: bad field name in SL ES
        } break;
#ifndef USE_BACKPORT
    //-----------------------------------
    // MediaPlayer
    case MEDIAPLAYER: {
        object_lock_exclusive(&pAudioPlayer->mObject);
        pAudioPlayer->mAndroidObjState = ANDROID_PREPARING;

        pAudioPlayer->mSfPlayer = new android::SfPlayer();
        pAudioPlayer->mSfPlayer->setNotifListener(sfplayer_handlePrefetchEvent,
                        (void*)pAudioPlayer /*notifUSer*/);
        pAudioPlayer->mSfPlayer->armLooper();
        object_unlock_exclusive(&pAudioPlayer->mObject);

        int res;
        switch (pAudioPlayer->mDataSource.mLocator.mLocatorType) {
            case SL_DATALOCATOR_URI:
                pAudioPlayer->mSfPlayer->setDataSource(
                        (const char*)pAudioPlayer->mDataSource.mLocator.mURI.URI);
                res = pAudioPlayer->mSfPlayer->prepare_sync();
                break;
            case SL_DATALOCATOR_ANDROIDFD: {
                int64_t offset = (int64_t)pAudioPlayer->mDataSource.mLocator.mFD.offset;
                pAudioPlayer->mSfPlayer->setDataSource(
                        (int)pAudioPlayer->mDataSource.mLocator.mFD.fd,
                        offset == SL_DATALOCATOR_ANDROIDFD_USE_FILE_SIZE ?
                                (int64_t)SFPLAYER_FD_FIND_FILE_SIZE : offset,
                        (int64_t)pAudioPlayer->mDataSource.mLocator.mFD.length);
                res = pAudioPlayer->mSfPlayer->prepare_sync();
                } break;
            default:
                res = ~SFPLAYER_SUCCESS;
                break;
        }

        object_lock_exclusive(&pAudioPlayer->mObject);
        if (SFPLAYER_SUCCESS != res) {
            pAudioPlayer->mAndroidObjState = ANDROID_UNINITIALIZED;
            pAudioPlayer->mNumChannels = 0;
            pAudioPlayer->mSampleRateMilliHz = 0;
            pAudioPlayer->mAudioTrack = NULL;
        } else {
            // create audio track based on parameters retrieved from Stagefright
            pAudioPlayer->mAudioTrack = new android::AudioTrack(
                    pAudioPlayer->mStreamType,                           // streamType
                    pAudioPlayer->mSfPlayer->getSampleRateHz(),          // sampleRate
                    android::AudioSystem::PCM_16_BIT,                    // format
                    pAudioPlayer->mSfPlayer->getNumChannels() == 1 ?     //channel mask
                            android::AudioSystem::CHANNEL_OUT_MONO :
                            android::AudioSystem::CHANNEL_OUT_STEREO,
                    0,                                                   // frameCount (here min)
                    0,                                                   // flags
                    audioTrack_callBack_uri,                             // callback
                    (void *) pAudioPlayer,                               // user
                    0);                                                  // notificationFrame
            pAudioPlayer->mNumChannels = pAudioPlayer->mSfPlayer->getNumChannels();
            pAudioPlayer->mSampleRateMilliHz =
                    android_to_sles_sampleRate(pAudioPlayer->mSfPlayer->getSampleRateHz());
            pAudioPlayer->mSfPlayer->useAudioTrack(pAudioPlayer->mAudioTrack);

            if (pAudioPlayer->mSfPlayer->wantPrefetch()) {
                pAudioPlayer->mAndroidObjState = ANDROID_PREPARED;
            } else {
                pAudioPlayer->mAndroidObjState = ANDROID_READY;
            }
        }
        object_unlock_exclusive(&pAudioPlayer->mObject);

        } break;
#endif
    default:
        SL_LOGE("Unexpected object type %d", pAudioPlayer->mAndroidObjType);
        result = SL_RESULT_INTERNAL_ERROR;
        break;
    }

#ifndef USE_BACKPORT

    if (ANDROID_UNINITIALIZED == pAudioPlayer->mAndroidObjState) {
        return result;
    }

    // proceed with effect initialization
    // initialize EQ
    // FIXME use a table of effect descriptors when adding support for more effects
    if (memcmp(SL_IID_EQUALIZER, &pAudioPlayer->mEqualizer.mEqDescriptor.type,
            sizeof(effect_uuid_t)) == 0) {
        SL_LOGV("Need to initialize EQ for AudioPlayer=%p", pAudioPlayer);
        android_eq_init(pAudioPlayer->mSessionId, &pAudioPlayer->mEqualizer);
    }
    // initialize BassBoost
    if (memcmp(SL_IID_BASSBOOST, &pAudioPlayer->mBassBoost.mBassBoostDescriptor.type,
            sizeof(effect_uuid_t)) == 0) {
        SL_LOGV("Need to initialize BassBoost for AudioPlayer=%p", pAudioPlayer);
        android_bb_init(pAudioPlayer->mSessionId, &pAudioPlayer->mBassBoost);
    }
    // initialize Virtualizer
    if (memcmp(SL_IID_VIRTUALIZER, &pAudioPlayer->mVirtualizer.mVirtualizerDescriptor.type,
               sizeof(effect_uuid_t)) == 0) {
        SL_LOGV("Need to initialize Virtualizer for AudioPlayer=%p", pAudioPlayer);
        android_virt_init(pAudioPlayer->mSessionId, &pAudioPlayer->mVirtualizer);
    }

    // initialize EffectSend
    // FIXME initialize EffectSend
#endif

    return result;
}

//-----------------------------------------------------------------------------
/*
 * Called with a lock held on the CAudioPlayer
 */
SLresult android_audioPlayer_setStreamType_l(CAudioPlayer *pAudioPlayer, SLuint32 type) {
    SLresult result = SL_RESULT_SUCCESS;
    SL_LOGV("android_audioPlayer_setStreamType %lu", type);

    if (pAudioPlayer->mAudioTrack == NULL) {
        return SL_RESULT_RESOURCE_ERROR;
    }
    if (type == android_to_sles_streamType(pAudioPlayer->mAudioTrack->streamType())) {
        return SL_RESULT_SUCCESS;
    }

    int format =  pAudioPlayer->mAudioTrack->format();
    uint32_t sr = sles_to_android_sampleRate(pAudioPlayer->mSampleRateMilliHz);

    pAudioPlayer->mAudioTrack->stop();
    delete pAudioPlayer->mAudioTrack;
    pAudioPlayer->mAudioTrack = new android::AudioTrack(
                    sles_to_android_streamType(type),               // streamType
                    sr,                                             // sampleRate
                    format,                                         // format
                    pAudioPlayer->mNumChannels== 1 ?                //channel mask
                            android::AudioSystem::CHANNEL_OUT_MONO :
                            android::AudioSystem::CHANNEL_OUT_STEREO,
                    0,                                              // frameCount (here min)
                    0,                                              // flags
                    pAudioPlayer->mAndroidObjType == MEDIAPLAYER ?  // callback
                            audioTrack_callBack_uri : audioTrack_callBack_pullFromBuffQueue,
                    (void *) pAudioPlayer,                          // user
                    0    // FIXME find appropriate frame count      // notificationFrame
#ifndef USE_BACKPORT
                    , pAudioPlayer->mSessionId
#endif
                    );
#ifndef USE_BACKPORT
    if (pAudioPlayer->mAndroidObjType == MEDIAPLAYER) {
        if (pAudioPlayer->mSfPlayer != 0) {
            pAudioPlayer->mSfPlayer->useAudioTrack(pAudioPlayer->mAudioTrack);
        }
    }
#endif

    return result;
}

//-----------------------------------------------------------------------------
SLresult android_audioPlayer_destroy(CAudioPlayer *pAudioPlayer) {
    SLresult result = SL_RESULT_SUCCESS;
    SL_LOGV("android_audioPlayer_destroy(%p)", pAudioPlayer);
    switch (pAudioPlayer->mAndroidObjType) {
    //-----------------------------------
    // AudioTrack
    case AUDIOTRACK_PUSH:
    case AUDIOTRACK_PULL:
        break;
#ifndef USE_BACKPORT
    //-----------------------------------
    // MediaPlayer
    case MEDIAPLAYER:
        if (pAudioPlayer->mSfPlayer != 0) {
            pAudioPlayer->mSfPlayer.clear();
        }
        break;
#endif
    default:
        SL_LOGE("Unexpected object type %d", pAudioPlayer->mAndroidObjType);
        result = SL_RESULT_INTERNAL_ERROR;
        break;
    }

    if (pAudioPlayer->mAudioTrack != NULL) {
        pAudioPlayer->mAudioTrack->stop();
        delete pAudioPlayer->mAudioTrack;
        pAudioPlayer->mAudioTrack = NULL;
    }

    if (pAudioPlayer->mpLock != NULL) {
        delete pAudioPlayer->mpLock;
        pAudioPlayer->mpLock = NULL;
    }

    pAudioPlayer->mAndroidObjType = INVALID_TYPE;

#ifndef USE_BACKPORT
    // FIXME this shouldn't have to be done here, there should be an "interface destroy" hook,
    //       just like there is an interface init hook, to avoid memory leaks.
    pAudioPlayer->mEqualizer.mEqEffect.clear();
    pAudioPlayer->mBassBoost.mBassBoostEffect.clear();
    pAudioPlayer->mVirtualizer.mVirtualizerEffect.clear();
    if (!pAudioPlayer->mAndroidEffect.mEffects->isEmpty()) {
        for (size_t i = 0 ; i < pAudioPlayer->mAndroidEffect.mEffects->size() ; i++) {
            delete pAudioPlayer->mAndroidEffect.mEffects->valueAt(i);
        }
        pAudioPlayer->mAndroidEffect.mEffects->clear();
    }
    delete pAudioPlayer->mAndroidEffect.mEffects;
#endif

    return result;
}


//-----------------------------------------------------------------------------
// called with no lock held
SLresult android_audioPlayer_setPlayRate(IPlaybackRate *pRateItf, SLpermille rate) {
    SLresult result = SL_RESULT_SUCCESS;
    CAudioPlayer *ap = (CAudioPlayer *)pRateItf->mThis;
    switch(ap->mAndroidObjType) {
    case AUDIOTRACK_PUSH:
    case AUDIOTRACK_PULL:
    case MEDIAPLAYER: {
        // get the content sample rate
        object_lock_peek(ap);
        uint32_t contentRate =
            sles_to_android_sampleRate(ap->mDataSource.mFormat.mPCM.samplesPerSec);
        object_unlock_peek(ap);
        // apply the SL ES playback rate on the AudioTrack as a factor of its content sample rate
        ap->mpLock->lock();
        if (ap->mAudioTrack != NULL) {
            ap->mAudioTrack->setSampleRate(contentRate * (rate/1000.0f));
        }
        ap->mpLock->unlock();
        }
        break;

    default:
        SL_LOGE("Unexpected object type %d", ap->mAndroidObjType);
        result = SL_RESULT_INTERNAL_ERROR;
        break;
    }
    return result;
}


//-----------------------------------------------------------------------------
// called with no lock held
SLresult android_audioPlayer_setPlaybackRateBehavior(IPlaybackRate *pRateItf,
        SLuint32 constraints) {
    SLresult result = SL_RESULT_SUCCESS;
    CAudioPlayer *ap = (CAudioPlayer *)pRateItf->mThis;
    switch(ap->mAndroidObjType) {
    case AUDIOTRACK_PUSH:
    case AUDIOTRACK_PULL:
    case MEDIAPLAYER:
        if (constraints != (constraints & SL_RATEPROP_NOPITCHCORAUDIO)) {
            result = SL_RESULT_FEATURE_UNSUPPORTED;
        }
        break;
    default:
        SL_LOGE("Unexpected object type %d", ap->mAndroidObjType);
        result = SL_RESULT_INTERNAL_ERROR;
        break;
    }
    return result;
}


//-----------------------------------------------------------------------------
// called with no lock held
SLresult android_audioPlayer_getCapabilitiesOfRate(IPlaybackRate *pRateItf,
        SLuint32 *pCapabilities) {
    CAudioPlayer *ap = (CAudioPlayer *)pRateItf->mThis;
    switch(ap->mAndroidObjType) {
    case AUDIOTRACK_PUSH:
    case AUDIOTRACK_PULL:
    case MEDIAPLAYER:
        *pCapabilities = SL_RATEPROP_NOPITCHCORAUDIO;
        break;
    default:
        *pCapabilities = 0;
        break;
    }
    return SL_RESULT_SUCCESS;
}


//-----------------------------------------------------------------------------
void android_audioPlayer_setPlayState(CAudioPlayer *ap) {
    SLuint32 state = ap->mPlay.mState;
    switch(ap->mAndroidObjType) {
    case AUDIOTRACK_PUSH:
    case AUDIOTRACK_PULL:
        switch (state) {
        case SL_PLAYSTATE_STOPPED:
            SL_LOGV("setting AudioPlayer to SL_PLAYSTATE_STOPPED");
            ap->mAudioTrack->stop();
            break;
        case SL_PLAYSTATE_PAUSED:
            SL_LOGV("setting AudioPlayer to SL_PLAYSTATE_PAUSED");
            ap->mAudioTrack->pause();
            break;
        case SL_PLAYSTATE_PLAYING:
            SL_LOGV("setting AudioPlayer to SL_PLAYSTATE_PLAYING");
            ap->mAudioTrack->start();
            break;
        default:
            // checked by caller, should not happen
            break;
        }
        break;
#ifndef USE_BACKPORT
    case MEDIAPLAYER:
        switch (state) {
        case SL_PLAYSTATE_STOPPED: {
            SL_LOGV("setting AudioPlayer to SL_PLAYSTATE_STOPPED");
            if (ap->mSfPlayer != 0) {
                ap->mSfPlayer->stop();
            }
            } break;
        case SL_PLAYSTATE_PAUSED: {
            SL_LOGV("setting AudioPlayer to SL_PLAYSTATE_PAUSED");
            object_lock_peek(&ap);
            AndroidObject_state state = ap->mAndroidObjState;
            object_unlock_peek(&ap);
            switch(state) {
                case(ANDROID_UNINITIALIZED):
                case(ANDROID_PREPARING):
                    if (ap->mSfPlayer != 0) {
                        ap->mSfPlayer->pause();
                    }
                    break;
                case(ANDROID_PREPARED):
                    if (ap->mSfPlayer != 0) {
                        ap->mSfPlayer->startPrefetch_async();
                    }
                case(ANDROID_PREFETCHING):
                case(ANDROID_READY):
                    if (ap->mSfPlayer != 0) {
                        ap->mSfPlayer->pause();
                    }
                    break;
                default:
                    break;
            }
            } break;
        case SL_PLAYSTATE_PLAYING: {
            SL_LOGV("setting AudioPlayer to SL_PLAYSTATE_PLAYING");
            object_lock_peek(&ap);
            AndroidObject_state state = ap->mAndroidObjState;
            object_unlock_peek(&ap);
            // FIXME check in spec when playback is allowed to start in another state
            if ((state >= ANDROID_READY) && (ap->mSfPlayer != 0)) {
                ap->mSfPlayer->play();
            }
            } break;
        default:
            // checked by caller, should not happen
            break;
        }
        break;
#endif
    default:
        break;
    }
}


//-----------------------------------------------------------------------------
void android_audioPlayer_useEventMask(CAudioPlayer *ap) {
    IPlay *pPlayItf = &ap->mPlay;
    SLuint32 eventFlags = pPlayItf->mEventFlags;
    /*switch(ap->mAndroidObjType) {
    case AUDIOTRACK_PUSH:
    case AUDIOTRACK_PULL:*/

    if (NULL == ap->mAudioTrack) {
        return;
    }

    if (eventFlags & SL_PLAYEVENT_HEADATMARKER) {
        ap->mAudioTrack->setMarkerPosition((uint32_t)((((int64_t)pPlayItf->mMarkerPosition
                * sles_to_android_sampleRate(ap->mSampleRateMilliHz)))/1000));
    } else {
        // clear marker
        ap->mAudioTrack->setMarkerPosition(0);
    }

    if (eventFlags & SL_PLAYEVENT_HEADATNEWPOS) {
         ap->mAudioTrack->setPositionUpdatePeriod(
                (uint32_t)((((int64_t)pPlayItf->mPositionUpdatePeriod
                * sles_to_android_sampleRate(ap->mSampleRateMilliHz)))/1000));
    } else {
        // clear periodic update
        ap->mAudioTrack->setPositionUpdatePeriod(0);
    }

    if (eventFlags & SL_PLAYEVENT_HEADATEND) {
        // nothing to do for SL_PLAYEVENT_HEADATEND, callback event will be checked against mask
    }

    if (eventFlags & SL_PLAYEVENT_HEADMOVING) {
        // FIXME support SL_PLAYEVENT_HEADMOVING
        SL_LOGE("[ FIXME: IPlay_SetCallbackEventsMask(SL_PLAYEVENT_HEADMOVING) on an "
            "SL_OBJECTID_AUDIOPLAYER to be implemented ]");
    }
    if (eventFlags & SL_PLAYEVENT_HEADSTALLED) {
        // nothing to do for SL_PLAYEVENT_HEADSTALLED, callback event will be checked against mask
    }

}


//-----------------------------------------------------------------------------
SLresult android_audioPlayer_getDuration(IPlay *pPlayItf, SLmillisecond *pDurMsec) {
    CAudioPlayer *ap = (CAudioPlayer *)pPlayItf->mThis;
    switch(ap->mAndroidObjType) {
    case AUDIOTRACK_PUSH:
    case AUDIOTRACK_PULL:
        *pDurMsec = SL_TIME_UNKNOWN;
        // FIXME if the data source is not a buffer queue, and the audio data is saved in
        //       shared memory with the mixer process, the duration is the size of the buffer
        SL_LOGE("FIXME: android_audioPlayer_getDuration() verify if duration can be retrieved");
        break;
#ifndef USE_BACKPORT
    case MEDIAPLAYER: {
        int64_t durationUsec = SL_TIME_UNKNOWN;
        if (ap->mSfPlayer != 0) {
            durationUsec = ap->mSfPlayer->getDurationUsec();
            *pDurMsec = durationUsec == -1 ? SL_TIME_UNKNOWN : durationUsec / 1000;
        }
        } break;
#endif
    default:
        break;
    }
    return SL_RESULT_SUCCESS;
}


//-----------------------------------------------------------------------------
void android_audioPlayer_getPosition(IPlay *pPlayItf, SLmillisecond *pPosMsec) {
    CAudioPlayer *ap = (CAudioPlayer *)pPlayItf->mThis;
    switch(ap->mAndroidObjType) {
    case AUDIOTRACK_PUSH:
    case AUDIOTRACK_PULL:
        uint32_t positionInFrames;
        ap->mAudioTrack->getPosition(&positionInFrames);
        if (ap->mSampleRateMilliHz == 0) {
            *pPosMsec = 0;
        } else {
            *pPosMsec = ((int64_t)positionInFrames * 1000) /
                    sles_to_android_sampleRate(ap->mSampleRateMilliHz);
        }
        break;
    case MEDIAPLAYER:
        if (ap->mSfPlayer != 0) {
            *pPosMsec = ap->mSfPlayer->getPositionMsec();
        } else {
            *pPosMsec = 0;
        }
        break;
    default:
        break;
    }
}


//-----------------------------------------------------------------------------
void android_audioPlayer_seek(CAudioPlayer *ap, SLmillisecond posMsec) {

    switch(ap->mAndroidObjType) {
    case AUDIOTRACK_PUSH:
    case AUDIOTRACK_PULL:
        break;
#ifndef USE_BACKPORT
    case MEDIAPLAYER:
        if (ap->mSfPlayer != 0) {
            ap->mSfPlayer->seek(posMsec);
        }
        break;
#endif
    default:
        break;
    }
}


//-----------------------------------------------------------------------------
/*
 * Mutes or unmutes the Android media framework object associated with the CAudioPlayer that carries
 * the IVolume interface.
 * Pre-condition:
 *   if ap->mMute is SL_BOOLEAN_FALSE, a call to this function was preceded by a call
 *   to android_audioPlayer_volumeUpdate()
 */
static void android_audioPlayer_setMute(CAudioPlayer* ap) {
    android::AudioTrack *t = NULL;
    switch(ap->mAndroidObjType) {
    case AUDIOTRACK_PUSH:
    case AUDIOTRACK_PULL:
    case MEDIAPLAYER:
        t = ap->mAudioTrack;
        break;
    default:
        break;
    }
    // when unmuting: volume levels have already been updated in IVolume_SetMute
    if (NULL != t) {
        t->mute(ap->mMute);
    }
}


//-----------------------------------------------------------------------------
SLresult android_audioPlayer_volumeUpdate(CAudioPlayer* ap) {
    android_audioPlayer_updateStereoVolume(ap);
    android_audioPlayer_setMute(ap);
    return SL_RESULT_SUCCESS;
}


//-----------------------------------------------------------------------------
void android_audioPlayer_bufferQueueRefilled(CAudioPlayer *ap) {
    // the AudioTrack associated with the AudioPlayer receiving audio from a PCM buffer
    // queue was stopped when the queue become empty, we restart as soon as a new buffer
    // has been enqueued since we're in playing state
    if (NULL != ap->mAudioTrack) {
        ap->mAudioTrack->start();
    }

    // when the queue became empty, an underflow on the prefetch status itf was sent. Now the queue
    // has received new data, signal it has sufficient data
    if (IsInterfaceInitialized(&(ap->mObject), MPH_PREFETCHSTATUS)) {
        audioPlayer_dispatch_prefetchStatus_lockPrefetch(ap, SL_PREFETCHSTATUS_SUFFICIENTDATA,
            true);
    }
}


//-----------------------------------------------------------------------------
/*
 * BufferQueue::Clear
 */
SLresult android_audioPlayerClear(CAudioPlayer *pAudioPlayer) {
    SLresult result = SL_RESULT_SUCCESS;

    switch (pAudioPlayer->mAndroidObjType) {
    //-----------------------------------
    // AudioTrack
    case AUDIOTRACK_PULL:
    case AUDIOTRACK_PUSH:
        pAudioPlayer->mAudioTrack->flush();
        break;
    default:
        result = SL_RESULT_INTERNAL_ERROR;
        break;
    }

    return result;
}

