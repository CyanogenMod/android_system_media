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
#include "math.h"
#include "utils/RefBase.h"


//-----------------------------------------------------------------------------
inline uint32_t sles_to_android_sampleRate(SLuint32 sampleRateMilliHertz) {
    return (uint32_t)(sampleRateMilliHertz / 1000);
}

inline int sles_to_android_sampleFormat(SLuint32 pcmFormat) {
    switch (pcmFormat) {
        case SL_PCMSAMPLEFORMAT_FIXED_16:
            return android::AudioSystem::PCM_16_BIT;
            break;
        case SL_PCMSAMPLEFORMAT_FIXED_8:
            return android::AudioSystem::PCM_8_BIT;
            break;
        case SL_PCMSAMPLEFORMAT_FIXED_20:
        case SL_PCMSAMPLEFORMAT_FIXED_24:
        case SL_PCMSAMPLEFORMAT_FIXED_28:
        case SL_PCMSAMPLEFORMAT_FIXED_32:
        default:
            return android::AudioSystem::INVALID_FORMAT;
    }
}


inline int sles_to_android_channelMask(SLuint32 nbChannels, SLuint32 channelMask) {
    // FIXME handle channel mask mapping between SL ES and Android
    return (nbChannels == 1 ?
            android::AudioSystem::CHANNEL_OUT_MONO :
            android::AudioSystem::CHANNEL_OUT_STEREO);
}


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

#define LEFT_CHANNEL_MASK  0x1 << 0
#define RIGHT_CHANNEL_MASK 0x1 << 1

static void android_audioPlayerUpdateStereoVolume(CAudioPlayer* ap) {
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

    // amplification from volume level
    // FIXME use the FX Framework conversions
    ap->mAmplFromVolLevel = pow(10, (float)ap->mVolume.mLevel/2000);
    leftVol *= ap->mAmplFromVolLevel;
    rightVol *= ap->mAmplFromVolLevel;

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
void android_handleTrackMarker(CAudioPlayer* ap) {
    //fprintf(stdout, "received event EVENT_MARKER from AudioTrack\n");
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
void android_handleTrackNewPos(CAudioPlayer* ap) {
    //fprintf(stdout, "received event EVENT_NEW_POS from AudioTrack\n");
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
void android_handleTrackUnderrun(CAudioPlayer* ap) {
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
// Callback associated with an SfPlayer of an SL ES AudioPlayer that gets its data
// from a URI, for prefetching events
static void android_prefetchEventCallback(const int event, const int data1, void* user) {
    if (NULL == user) {
        return;
    }

    CAudioPlayer *ap = (CAudioPlayer *)user;
    //fprintf(stdout, "received event %d = %d from SfAudioPlayer\n", event, data1);
    switch(event) {

    case(android::SfPlayer::kEventPrefetchFillLevelUpdate): {
        // FIXME implement buffer filler level updates
        fprintf(stderr, "[ FIXME implement buffer filler level updates ]\n");
        //ap->mPrefetchStatus.mLevel = ;
        } break;

    case(android::SfPlayer::kEventPrefetchStatusChange): {
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
        slPlayCallback playCallback = NULL;
        void * playContext = NULL;
        // SLPlayItf callback or no callback?
        interface_lock_exclusive(&ap->mPlay);
        if (ap->mPlay.mEventFlags & SL_PLAYEVENT_HEADATEND) {
            playCallback = ap->mPlay.mCallback;
            playContext = ap->mPlay.mContext;
        }
        ap->mPlay.mState = SL_PLAYSTATE_PAUSED;
        // FIXME update play position to make sure it's at the same value
        //       as getDuration if it's known?
        interface_unlock_exclusive(&ap->mPlay);
        // callback with no lock held
        if (NULL != playCallback) {
            (*playCallback)(&ap->mPlay.mItf, playContext, SL_PLAYEVENT_HEADATEND);
        }
        ap->mAudioTrack->stop();
        } break;

    default:
        break;
    }
}

//-----------------------------------------------------------------------------
SLresult sles_to_android_checkAudioPlayerSourceSink(CAudioPlayer *pAudioPlayer)
{
    const SLDataSource *pAudioSrc = &pAudioPlayer->mDataSource.u.mSource;
    const SLDataSink *pAudioSnk = &pAudioPlayer->mDataSink.u.mSink;
    //--------------------------------------
    // Sink check:
    //     currently only OutputMix sinks are supported, regardless of the data source
    if (*(SLuint32 *)pAudioSnk->pLocator != SL_DATALOCATOR_OUTPUTMIX) {
        fprintf(stderr, "Cannot create audio player: data sink is not SL_DATALOCATOR_OUTPUTMIX\n");
        return SL_RESULT_PARAMETER_INVALID;
    }
    // FIXME verify output mix is in realized state
    fprintf(stderr, "[ FIXME verify OutputMix is in Realized state ]\n");

    //--------------------------------------
    // Source check:
    SLuint32 locatorType = *(SLuint32 *)pAudioSrc->pLocator;
    SLuint32 formatType = *(SLuint32 *)pAudioSrc->pFormat;
    SLuint32 numBuffers = 0;
    switch (locatorType) {
    //------------------
    //   Buffer Queues
    case SL_DATALOCATOR_BUFFERQUEUE: {
        SLDataLocator_BufferQueue *dl_bq =  (SLDataLocator_BufferQueue *) pAudioSrc->pLocator;
        numBuffers = dl_bq->numBuffers;
        if (0 == numBuffers) {
            fprintf(stderr, "Cannot create audio player: data source buffer queue has ");
            fprintf(stderr, "a depth of 0");
            return SL_RESULT_PARAMETER_INVALID;
        }
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
                fprintf(stderr, "Cannot create audio player: implementation doesn't ");
                fprintf(stderr, "support buffers with more than 2 channels");
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
                // others
            default:
                fprintf(stderr, "Cannot create audio player: unsupported sample rate");
                return SL_RESULT_CONTENT_UNSUPPORTED;
            }
            switch (df_pcm->bitsPerSample) {
            case SL_PCMSAMPLEFORMAT_FIXED_8:
                fprintf(stdout, "FIXME handle 8bit data\n");
            case SL_PCMSAMPLEFORMAT_FIXED_16:
                break;
                // others
            default:
                fprintf(stderr, "Cannot create audio player: unsupported sample format %lu",
                        (SLuint32)df_pcm->bitsPerSample);
                return SL_RESULT_CONTENT_UNSUPPORTED;
            }
            switch (df_pcm->containerSize) {
            case 16:
                break;
                // others
            default:
                //FIXME add error message
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
                // others esp. big and native (new not in spec)
            default:
                //FIXME add error message
                return SL_RESULT_CONTENT_UNSUPPORTED;
            }
            } //case SL_DATAFORMAT_PCM
            break;
        case SL_DATAFORMAT_MIME:
        case SL_DATAFORMAT_RESERVED3:
            fprintf(stderr, "Error: cannot create Audio Player with SL_DATALOCATOR_BUFFERQUEUE data source without SL_DATAFORMAT_PCM format\n");
            return SL_RESULT_CONTENT_UNSUPPORTED;
        default:
            fprintf(stderr, "Error: cannot create Audio Player with SL_DATALOCATOR_BUFFERQUEUE data source without SL_DATAFORMAT_PCM format\n");
            return SL_RESULT_PARAMETER_INVALID;
        } // switch (formatType)
        pAudioPlayer->mBufferQueue.mNumBuffers = numBuffers;
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
                fprintf(stderr, "Error: cannot create Audio Player with SL_DATALOCATOR_URI data source without SL_DATAFORMAT_MIME format\n");
                return SL_RESULT_CONTENT_UNSUPPORTED;
            } // switch (formatType)
        } // case SL_DATALOCATOR_URI
        break;
    //------------------
    //   Address
    case SL_DATALOCATOR_ADDRESS:
    case SL_DATALOCATOR_IODEVICE:
    case SL_DATALOCATOR_OUTPUTMIX:
    case SL_DATALOCATOR_RESERVED5:
    case SL_DATALOCATOR_MIDIBUFFERQUEUE:
    case SL_DATALOCATOR_RESERVED8:
        return SL_RESULT_CONTENT_UNSUPPORTED;
    default:
        return SL_RESULT_PARAMETER_INVALID;
    }// switch (locatorType)

    return SL_RESULT_SUCCESS;
}



//-----------------------------------------------------------------------------
static void android_uriAudioTrackCallback(int event, void* user, void *info) {
    // EVENT_MORE_DATA needs to be handled with priority over the other events
    // because it will be called the most often during playback
    if (event == android::AudioTrack::EVENT_MORE_DATA) {
        //fprintf(stderr, "received event EVENT_MORE_DATA from AudioTrack\n");
        // set size to 0 to signal we're not using the callback to write more data
        android::AudioTrack::Buffer* pBuff = (android::AudioTrack::Buffer*)info;
        pBuff->size = 0;
    } else if (NULL != user) {
        switch (event) {
            case (android::AudioTrack::EVENT_MARKER) :
                android_handleTrackMarker((CAudioPlayer *)user);
                break;
            case (android::AudioTrack::EVENT_NEW_POS) :
                android_handleTrackNewPos((CAudioPlayer *)user);
                break;
            case (android::AudioTrack::EVENT_UNDERRUN) :
                android_handleTrackUnderrun((CAudioPlayer *)user);
        }
    }
}

//-----------------------------------------------------------------------------
// Callback associated with an AudioTrack of an SL ES AudioPlayer that gets its data
// from a buffer queue.
static void android_pullAudioTrackCallback(int event, void* user, void *info) {
    CAudioPlayer *pAudioPlayer = (CAudioPlayer *)user;
    void * callbackPContext = NULL;
    switch(event) {

    case (android::AudioTrack::EVENT_MORE_DATA) : {
        //fprintf(stdout, "received event EVENT_MORE_DATA from AudioTrack\n");
        slBufferQueueCallback callback = NULL;
        android::AudioTrack::Buffer* pBuff = (android::AudioTrack::Buffer*)info;
        // retrieve data from the buffer queue
        interface_lock_exclusive(&pAudioPlayer->mBufferQueue);
        if (pAudioPlayer->mBufferQueue.mState.count != 0) {
            //fprintf(stderr, "nbBuffers in queue = %lu\n",pAudioPlayer->mBufferQueue.mState.count);
            assert(pAudioPlayer->mBufferQueue.mFront != pAudioPlayer->mBufferQueue.mRear);

            BufferHeader *oldFront = pAudioPlayer->mBufferQueue.mFront;
            BufferHeader *newFront = &oldFront[1];

            // FIXME handle 8bit based on buffer format
            short *pSrc = (short*)((char *)oldFront->mBuffer
                    + pAudioPlayer->mBufferQueue.mSizeConsumed);
            if (pAudioPlayer->mBufferQueue.mSizeConsumed + pBuff->size < oldFront->mSize) {
                // can't consume the whole or rest of the buffer in one shot
                pAudioPlayer->mBufferQueue.mSizeConsumed += pBuff->size;
                // leave pBuff->size untouched
                // consume data
                // FIXME can we avoid holding the lock during the copy?
                memcpy (pBuff->i16, pSrc, pBuff->size);
            } else {
                // finish consuming the buffer or consume the buffer in one shot
                pBuff->size = oldFront->mSize - pAudioPlayer->mBufferQueue.mSizeConsumed;
                pAudioPlayer->mBufferQueue.mSizeConsumed = 0;

                if (newFront ==
                        &pAudioPlayer->mBufferQueue.mArray[pAudioPlayer->mBufferQueue.mNumBuffers + 1])
                {
                    newFront = pAudioPlayer->mBufferQueue.mArray;
                }
                pAudioPlayer->mBufferQueue.mFront = newFront;

                pAudioPlayer->mBufferQueue.mState.count--;
                pAudioPlayer->mBufferQueue.mState.playIndex++;

                // consume data
                // FIXME can we avoid holding the lock during the copy?
                memcpy (pBuff->i16, pSrc, pBuff->size);

                // data has been consumed, and the buffer queue state has been updated
                // we will notify the client if applicable
                callback = pAudioPlayer->mBufferQueue.mCallback;
                // save callback data
                callbackPContext = pAudioPlayer->mBufferQueue.mContext;
            }
        } else {
            // no data available
            pBuff->size = 0;
        }
        interface_unlock_exclusive(&pAudioPlayer->mBufferQueue);
        // notify client
        if (NULL != callback) {
            (*callback)(&pAudioPlayer->mBufferQueue.mItf, callbackPContext);
        }
    }
    break;

    case (android::AudioTrack::EVENT_MARKER) :
        android_handleTrackMarker(pAudioPlayer);
        break;

    case (android::AudioTrack::EVENT_NEW_POS) :
        android_handleTrackNewPos(pAudioPlayer);
        break;

    case (android::AudioTrack::EVENT_UNDERRUN) :
        android_handleTrackUnderrun(pAudioPlayer);
        break;

    default:
        // FIXME where does the notification of SL_PLAYEVENT_HEADATEND, SL_PLAYEVENT_HEADMOVING fit?
        break;
    }
}


//-----------------------------------------------------------------------------
SLresult sles_to_android_audioPlayerCreate(
        CAudioPlayer *pAudioPlayer) {

    const SLDataSource *pAudioSrc = &pAudioPlayer->mDataSource.u.mSource;
    const SLDataSink *pAudioSnk = &pAudioPlayer->mDataSink.u.mSink;
    SLresult result = SL_RESULT_SUCCESS;

    //--------------------------------------
    // Output check:
    // currently only OutputMix sinks are supported
    // this has been verified in sles_to_android_CheckAudioPlayerSourceSink

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
    //   URI to MediaPlayer
    case SL_DATALOCATOR_URI:
        pAudioPlayer->mAndroidObjType = MEDIAPLAYER;
        pAudioPlayer->mpLock = new android::Mutex();
        pAudioPlayer->mPlaybackRate.mCapabilities = SL_RATEPROP_NOPITCHCORAUDIO;
        break;
    default:
        pAudioPlayer->mAndroidObjType = INVALID_TYPE;
        pAudioPlayer->mpLock = NULL;
        pAudioPlayer->mPlaybackRate.mCapabilities = 0;
        result = SL_RESULT_PARAMETER_INVALID;
    }

    pAudioPlayer->mAndroidObjState = ANDROID_UNINITIALIZED;
    pAudioPlayer->mAudioTrack = NULL;
    pAudioPlayer->mSfPlayer.clear();
    pAudioPlayer->mRenderLooper.clear();

    pAudioPlayer->mAmplFromVolLevel = 1.0f;
    pAudioPlayer->mAmplFromStereoPos[0] = 1.0f;
    pAudioPlayer->mAmplFromStereoPos[1] = 1.0f;

    return result;

}


//-----------------------------------------------------------------------------
SLresult sles_to_android_audioPlayerRealize(CAudioPlayer *pAudioPlayer, SLboolean async) {

    SLresult result = SL_RESULT_SUCCESS;
    //fprintf(stderr, "entering sles_to_android_audioPlayerRealize\n");
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
                ANDROID_DEFAULT_OUTPUT_STREAM_TYPE,                  // streamType
                sampleRate,                                          // sampleRate
                sles_to_android_sampleFormat(df_pcm->bitsPerSample), // format
                sles_to_android_channelMask(df_pcm->numChannels, df_pcm->channelMask),//channel mask
                0,                                                   // frameCount (here min)
                0,                                                   // flags
                android_pullAudioTrackCallback,                      // callback
                (void *) pAudioPlayer,                               // user
                0);  // FIXME find appropriate frame count         // notificationFrame
        if (pAudioPlayer->mAudioTrack->initCheck() != android::NO_ERROR) {
            result = SL_RESULT_CONTENT_UNSUPPORTED;
        }

        // initialize platform-independent CAudioPlayer fields

        pAudioPlayer->mNumChannels = df_pcm->numChannels;
        } break;
    //-----------------------------------
    // MediaPlayer
    case MEDIAPLAYER: {
        object_lock_exclusive(&pAudioPlayer->mObject);
        pAudioPlayer->mAndroidObjState = ANDROID_PREPARING;

        pAudioPlayer->mRenderLooper = new android::ALooper();
        pAudioPlayer->mSfPlayer = new android::SfPlayer(pAudioPlayer->mRenderLooper);
        pAudioPlayer->mSfPlayer->setNotifListener(android_prefetchEventCallback,
                (void*)pAudioPlayer /*notifUSer*/);
        pAudioPlayer->mRenderLooper->registerHandler(pAudioPlayer->mSfPlayer);
        pAudioPlayer->mRenderLooper->start(false /*runOnCallingThread*/);
        object_unlock_exclusive(&pAudioPlayer->mObject);

        int res = pAudioPlayer->mSfPlayer->prepare_sync(
                (const char*)pAudioPlayer->mDataSource.mLocator.mURI.URI);

        object_lock_exclusive(&pAudioPlayer->mObject);
        if (SFPLAYER_SUCCESS != res) {
            pAudioPlayer->mAndroidObjState = ANDROID_UNINITIALIZED;
            pAudioPlayer->mNumChannels = 0;
        } else {
            // create audio track based on parameters retrieved from Stagefright
            pAudioPlayer->mAudioTrack = new android::AudioTrack(
                    ANDROID_DEFAULT_OUTPUT_STREAM_TYPE,                  // streamType
                    pAudioPlayer->mSfPlayer->getSampleRateHz(),          // sampleRate
                    android::AudioSystem::PCM_16_BIT,                    // format
                    pAudioPlayer->mSfPlayer->getNumChannels() == 1 ?     //channel mask
                            android::AudioSystem::CHANNEL_OUT_MONO :
                            android::AudioSystem::CHANNEL_OUT_STEREO,
                    0,                                                   // frameCount (here min)
                    0,                                                   // flags
                    android_uriAudioTrackCallback,                       // callback
                    (void *) pAudioPlayer,                               // user
                    0);                                                  // notificationFrame
            pAudioPlayer->mNumChannels = pAudioPlayer->mSfPlayer->getNumChannels();
            pAudioPlayer->mSfPlayer->useAudioTrack(pAudioPlayer->mAudioTrack);

            if (pAudioPlayer->mSfPlayer->wantPrefetch()) {
                pAudioPlayer->mAndroidObjState = ANDROID_PREPARED;
            } else {
                pAudioPlayer->mAndroidObjState = ANDROID_READY;
            }
        }
        object_unlock_exclusive(&pAudioPlayer->mObject);

        } break;
    default:
        result = SL_RESULT_CONTENT_UNSUPPORTED;
    }

    return result;
}


//-----------------------------------------------------------------------------
SLresult sles_to_android_audioPlayerDestroy(CAudioPlayer *pAudioPlayer) {
    SLresult result = SL_RESULT_SUCCESS;
    //fprintf(stdout, "sles_to_android_audioPlayerDestroy\n");

    pAudioPlayer->mAndroidObjType = INVALID_TYPE;

    switch (pAudioPlayer->mAndroidObjType) {
    //-----------------------------------
    // AudioTrack
    case AUDIOTRACK_PUSH:
    case AUDIOTRACK_PULL:
        break;
    //-----------------------------------
    // MediaPlayer
    case MEDIAPLAYER:
        // FIXME group in one function?
        if (pAudioPlayer->mSfPlayer != NULL) {
            pAudioPlayer->mSfPlayer->stop();
            pAudioPlayer->mSfPlayer.clear();
            pAudioPlayer->mRenderLooper->stop();
            pAudioPlayer->mRenderLooper.clear();
        }
        break;
    default:
        result = SL_RESULT_CONTENT_UNSUPPORTED;
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

    return result;
}


//-----------------------------------------------------------------------------
// called with no lock held
SLresult sles_to_android_audioPlayerSetPlayRate(IPlaybackRate *pRateItf, SLpermille rate) {
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
        result = SL_RESULT_CONTENT_UNSUPPORTED;
        break;
    }
    return result;
}


//-----------------------------------------------------------------------------
// called with no lock held
SLresult sles_to_android_audioPlayerSetPlaybackRateBehavior(IPlaybackRate *pRateItf,
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
        result = SL_RESULT_CONTENT_UNSUPPORTED;
        break;
    }
    return result;
}


//-----------------------------------------------------------------------------
// called with no lock held
SLresult sles_to_android_audioPlayerGetCapabilitiesOfRate(IPlaybackRate *pRateItf,
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
void sles_to_android_audioPlayerSetPlayState(CAudioPlayer *ap) {
    SLuint32 state = ap->mPlay.mState;
    switch(ap->mAndroidObjType) {
    case AUDIOTRACK_PUSH:
    case AUDIOTRACK_PULL:
        switch (state) {
        case SL_PLAYSTATE_STOPPED:
            fprintf(stdout, "setting AudioPlayer to SL_PLAYSTATE_STOPPED\n");
            ap->mAudioTrack->stop();
            break;
        case SL_PLAYSTATE_PAUSED:
            fprintf(stdout, "setting AudioPlayer to SL_PLAYSTATE_PAUSED\n");
            ap->mAudioTrack->pause();
            break;
        case SL_PLAYSTATE_PLAYING:
            fprintf(stdout, "setting AudioPlayer to SL_PLAYSTATE_PLAYING\n");
            ap->mAudioTrack->start();
            break;
        default:
            // checked by caller, should not happen
            break;
        }
        break;
    case MEDIAPLAYER:
        switch (state) {
        case SL_PLAYSTATE_STOPPED: {
            fprintf(stdout, "setting AudioPlayer to SL_PLAYSTATE_STOPPED\n");
            object_lock_peek(&ap);
            AndroidObject_state state = ap->mAndroidObjState;
            object_unlock_peek(&ap);
            if (state >= ANDROID_READY) {
                ap->mAudioTrack->stop();
            }
            ap->mSfPlayer->stop();
            } break;
        case SL_PLAYSTATE_PAUSED: {
            fprintf(stdout, "setting AudioPlayer to SL_PLAYSTATE_PAUSED\n");
            object_lock_peek(&ap);
            AndroidObject_state state = ap->mAndroidObjState;
            object_unlock_peek(&ap);
            switch(state) {
                case(ANDROID_UNINITIALIZED):
                case(ANDROID_PREPARING):
                    break;
                case(ANDROID_PREPARED):
                    ap->mSfPlayer->startPrefetch_async();
                    break;
                case(ANDROID_PREFETCHING):
                case(ANDROID_READY):
                    // FIXME pause the actual playback
                    fprintf(stderr, "[ FIXME implement pause() ]\n");
                    break;
                default:
                    break;
            }
            } break;
        case SL_PLAYSTATE_PLAYING: {
            fprintf(stdout, "setting AudioPlayer to SL_PLAYSTATE_PLAYING\n");
            object_lock_peek(&ap);
            AndroidObject_state state = ap->mAndroidObjState;
            object_unlock_peek(&ap);
            if (state >= ANDROID_READY) {
                ap->mAudioTrack->start();
                ap->mSfPlayer->play();
            }
            } break;
        default:
            // checked by caller, should not happen
            break;
        }
        break;
    default:
        break;
    }
}


//-----------------------------------------------------------------------------
void sles_to_android_audioPlayerUseEventMask(CAudioPlayer *ap) {
    IPlay *pPlayItf = &ap->mPlay;
    SLuint32 eventFlags = pPlayItf->mEventFlags;
    /*switch(ap->mAndroidObjType) {
    case AUDIOTRACK_PUSH:
    case AUDIOTRACK_PULL:*/

    if (NULL == ap->mAudioTrack) {
        return;
    }

    if (eventFlags & SL_PLAYEVENT_HEADATMARKER) {
        //FIXME getSampleRate() returns the current playback sample rate, not the content
        //      sample rate, verify if formula valid
        //      need to cache sample rate instead?
        fprintf(stderr, "[ FIXME: fix marker computation due to sample rate reporting behavior ]\n");
        ap->mAudioTrack->setMarkerPosition((uint32_t)((pPlayItf->mMarkerPosition
                * ap->mAudioTrack->getSampleRate())/1000));
    } else {
        // clear marker
        ap->mAudioTrack->setMarkerPosition(0);
    }

    if (eventFlags & SL_PLAYEVENT_HEADATNEWPOS) {
        //FIXME getSampleRate() returns the current playback sample rate, not the content
        //      sample rate, verify if formula valid
        fprintf(stderr, "[ FIXME: fix marker computation due to sample rate reporting behavior ]\n");
        ap->mAudioTrack->setPositionUpdatePeriod(
                (uint32_t)((pPlayItf->mPositionUpdatePeriod
                        * ap->mAudioTrack->getSampleRate())/1000));
    } else {
        // clear periodic update
        ap->mAudioTrack->setPositionUpdatePeriod(0);
    }

    if (eventFlags & SL_PLAYEVENT_HEADATEND) {
        if (AUDIOTRACK_PULL == ap->mAndroidObjType) {
            // FIXME support SL_PLAYEVENT_HEADATEND
            fprintf(stderr, "[ FIXME: IPlay_SetCallbackEventsMask(SL_PLAYEVENT_HEADATEND) on an SL_OBJECTID_AUDIOPLAYER to be implemented ]\n");
        } else if (MEDIAPLAYER == ap->mAndroidObjType) {
            // nothing to do for SL_PLAYEVENT_HEADATEND, callback event will be checked against mask
        }
    }

    if (eventFlags & SL_PLAYEVENT_HEADMOVING) {
        // FIXME support SL_PLAYEVENT_HEADMOVING
        fprintf(stderr, "[ FIXME: IPlay_SetCallbackEventsMask(SL_PLAYEVENT_HEADMOVING) on an SL_OBJECTID_AUDIOPLAYER to be implemented ]\n");
    }
    if (eventFlags & SL_PLAYEVENT_HEADSTALLED) {
        // FIXME support SL_PLAYEVENT_HEADSTALLED
        fprintf(stderr, "[ FIXME: IPlay_SetCallbackEventsMask(SL_PLAYEVENT_HEADSTALLED) on an SL_OBJECTID_AUDIOPLAYER to be implemented ]\n");
    }

}


//-----------------------------------------------------------------------------
SLresult sles_to_android_audioPlayerGetDuration(IPlay *pPlayItf, SLmillisecond *pDurMsec) {
    CAudioPlayer *ap = (CAudioPlayer *)pPlayItf->mThis;
    switch(ap->mAndroidObjType) {
    case AUDIOTRACK_PUSH:
    case AUDIOTRACK_PULL:
        *pDurMsec = SL_TIME_UNKNOWN;
        // FIXME if the data source is not a buffer queue, and the audio data is saved in
        //       shared memory with the mixer process, the duration is the size of the buffer
        fprintf(stderr, "FIXME: sles_to_android_audioPlayerGetDuration() verify if duration can be retrieved\n");
        break;
    case MEDIAPLAYER: {
        int64_t durationUsec = ap->mSfPlayer->getDurationUsec();
        *pDurMsec = durationUsec == -1 ? SL_TIME_UNKNOWN : durationUsec / 1000;
        } break;
    default:
        break;
    }
    return SL_RESULT_SUCCESS;
}


//-----------------------------------------------------------------------------
void sles_to_android_audioPlayerGetPosition(IPlay *pPlayItf, SLmillisecond *pPosMsec) {
    CAudioPlayer *ap = (CAudioPlayer *)pPlayItf->mThis;
    switch(ap->mAndroidObjType) {
    case AUDIOTRACK_PUSH:
    case AUDIOTRACK_PULL:
        uint32_t positionInFrames;
        ap->mAudioTrack->getPosition(&positionInFrames);
        //FIXME getSampleRate() returns the current playback sample rate, not the content
        //      sample rate, verify if formula valid
        fprintf(stderr, "FIXME: fix marker computation due to sample rate reporting behavior");
        *pPosMsec = positionInFrames * 1000 / ap->mAudioTrack->getSampleRate();
        break;
    case MEDIAPLAYER:
        // FIXME implement getPosition
        fprintf(stderr, "FIXME: implement getPosition\n");
        //ap->mMediaPlayerData.mMediaPlayer->getCurrentPosition((int*)pPosMsec);
        break;
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
 *   to sles_to_android_audioPlayerVolumeUpdate()
 */
static void android_audioPlayerSetMute(CAudioPlayer* ap) {
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
SLresult sles_to_android_audioPlayerVolumeUpdate(CAudioPlayer* ap) {
    android_audioPlayerUpdateStereoVolume(ap);
    android_audioPlayerSetMute(ap);
    return SL_RESULT_SUCCESS;
}
