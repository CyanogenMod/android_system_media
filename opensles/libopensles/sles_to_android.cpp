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


void android_audioPlayerUpdateStereoVolume(IVolume *pVolItf) {
    // should not be used when muted
    if (pVolItf->mMute) {
        return;
    }
    float leftVol = 1.0f, rightVol = 1.0f;

    //int muteSoloLeft, muteSoleRight;
    CAudioPlayer *ap = (CAudioPlayer *)pVolItf->mThis;
    //muteSoloLeft = (mChannelMutes & CHANNEL_OUT_FRONT_LEFT) >> 2;
    //muteSoloRight = (mChannelMutes & CHANNEL_OUT_FRONT_RIGHT) >> 3;

    // compute amplification as the combination of volume level and stereo position

    // amplification from volume level
    // FIXME use the FX Framework conversions
    pVolItf->mAmplFromVolLevel = pow(10, (float)pVolItf->mLevel/2000);
    leftVol *= pVolItf->mAmplFromVolLevel;
    rightVol *= pVolItf->mAmplFromVolLevel;

    switch(ap->mAndroidObjType) {
    case AUDIOTRACK_PUSH:
    case AUDIOTRACK_PULL:
        // amplification from stereo position
        if (pVolItf->mEnableStereoPosition) {
            // panning law depends on number of channels of content: stereo panning vs 2ch. balance
            if(ap->mAudioTrack->channelCount() == 1) {
                // stereo panning
                double theta = (1000+pVolItf->mStereoPosition)*M_PI_4/1000.0f; // 0 <= theta <= Pi/2
                pVolItf->mAmplFromStereoPos[0] = cos(theta);
                pVolItf->mAmplFromStereoPos[1] = sin(theta);
            } else {
                // stereo balance
                if (pVolItf->mStereoPosition > 0) {
                    pVolItf->mAmplFromStereoPos[0] = (1000-pVolItf->mStereoPosition)/1000.0f;
                    pVolItf->mAmplFromStereoPos[1] = 1.0f;
                } else {
                    pVolItf->mAmplFromStereoPos[0] = 1.0f;
                    pVolItf->mAmplFromStereoPos[1] = (1000+pVolItf->mStereoPosition)/1000.0f;
                }
            }
            leftVol  *= pVolItf->mAmplFromStereoPos[0];
            rightVol *= pVolItf->mAmplFromStereoPos[1];
        }
        ap->mAudioTrack->setVolume(leftVol, rightVol);
        break;
    case MEDIAPLAYER:
        ap->mMediaPlayer->setVolume(leftVol, rightVol);
        break;
    default:
        break;
    }
}


//-----------------------------------------------------------------------------
SLresult sles_to_android_checkAudioPlayerSourceSink(const SLDataSource *pAudioSrc,
        const SLDataSink *pAudioSnk)
{
    //--------------------------------------
    // Sink check:
    //     currently only OutputMix sinks are supported, regardless of the data source
    if (*(SLuint32 *)pAudioSnk->pLocator != SL_DATALOCATOR_OUTPUTMIX) {
        fprintf(stderr, "Cannot create audio player: data sink is not SL_DATALOCATOR_OUTPUTMIX\n");
        return SL_RESULT_PARAMETER_INVALID;
    }
    // FIXME verify output mix is in realized state
    fprintf(stderr, "FIXME verify OutputMix is in Realized state\n");

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
// Callback associated with an AudioTrack of an SL ES AudioPlayer that gets its data
// from a buffer queue.
static void android_pullAudioTrackCallback(int event, void* user, void *info) {
    CAudioPlayer *pAudioPlayer = (CAudioPlayer *)user;
    void * callbackPContext = NULL;
    switch(event) {

    case (android::AudioTrack::EVENT_MORE_DATA) : {
        //fprintf(stdout, "received event EVENT_MORE_DATA from AudioTrack\n");
        android::AudioTrack::Buffer* pBuff = (android::AudioTrack::Buffer*)info;
        // retrieve data from the buffer queue
        interface_lock_exclusive(&pAudioPlayer->mBufferQueue);
        slBufferQueueCallback callback = NULL;
        if (pAudioPlayer->mBufferQueue.mState.count != 0) {
            //fprintf(stderr, "nbBuffers in queue = %lu\n",pAudioPlayer->mBufferQueue.mState.count);
            assert(pAudioPlayer->mBufferQueue.mFront != pAudioPlayer->mBufferQueue.mRear);

            struct BufferHeader *oldFront = pAudioPlayer->mBufferQueue.mFront;
            struct BufferHeader *newFront = &oldFront[1];

            // FIXME handle 8bit based on buffer format
            short *pSrc = (short*)((char *)oldFront->mBuffer
                    + pAudioPlayer->mBufferQueue.mSizeConsumed);
            if (pAudioPlayer->mBufferQueue.mSizeConsumed + pBuff->size < oldFront->mSize) {
                // can't consume the whole or rest of the buffer in one shot
                pAudioPlayer->mBufferQueue.mSizeConsumed += pBuff->size;
                // leave pBuff->size untouched
                // consume data
                memcpy (pBuff->i16, pSrc, pBuff->size);
            } else {
                // finish consuming the buffer or consume the buffer in one shot
                pBuff->size = oldFront->mSize - pAudioPlayer->mBufferQueue.mSizeConsumed;
                pAudioPlayer->mBufferQueue.mSizeConsumed = 0;

                if (newFront ==
                        &pAudioPlayer->mBufferQueue.mArray[pAudioPlayer->mBufferQueue.mNumBuffers])
                {
                    newFront = pAudioPlayer->mBufferQueue.mArray;
                }
                pAudioPlayer->mBufferQueue.mFront = newFront;

                pAudioPlayer->mBufferQueue.mState.count--;
                pAudioPlayer->mBufferQueue.mState.playIndex++;

                // consume data
                memcpy (pBuff->i16, pSrc, pBuff->size);

                // data has been consumed, and the buffer queue state has been updated
                // we can notify the client if applicable
                callback = pAudioPlayer->mBufferQueue.mCallback;
                if (NULL != callback) {
                    // save callback data
                    callbackPContext = pAudioPlayer->mBufferQueue.mContext;
                }
            }
        } else {
            // no data available
            pBuff->size = 0;
        }
        interface_unlock_exclusive(&pAudioPlayer->mBufferQueue);
        if (NULL != callback) {
            (*callback)(&pAudioPlayer->mBufferQueue.mItf, callbackPContext);
        }
    }
    break;

    case (android::AudioTrack::EVENT_MARKER) : {
        //fprintf(stdout, "received event EVENT_MARKER from AudioTrack\n");
        slPlayCallback callback = NULL;
        interface_lock_shared(&pAudioPlayer->mPlay);
        callback = pAudioPlayer->mPlay.mCallback;
        callbackPContext = pAudioPlayer->mPlay.mContext;
        interface_unlock_shared(&pAudioPlayer->mPlay);
        if (NULL != callback) {
            // getting this event implies SL_PLAYEVENT_HEADATMARKER was set in the event mask
            (*callback)(&pAudioPlayer->mPlay.mItf, callbackPContext, SL_PLAYEVENT_HEADATMARKER);
        }
    }
    break;

    case (android::AudioTrack::EVENT_NEW_POS) : {
        //fprintf(stdout, "received event EVENT_NEW_POS from AudioTrack\n");
        slPlayCallback callback = NULL;
        interface_lock_shared(&pAudioPlayer->mPlay);
        callback = pAudioPlayer->mPlay.mCallback;
        callbackPContext = pAudioPlayer->mPlay.mContext;
        interface_unlock_shared(&pAudioPlayer->mPlay);
        if (NULL != callback) {
            // getting this event implies SL_PLAYEVENT_HEADATNEWPOS was set in the event mask
            (*callback)(&pAudioPlayer->mPlay.mItf, callbackPContext, SL_PLAYEVENT_HEADATNEWPOS);
        }
    }
    break;

    case (android::AudioTrack::EVENT_UNDERRUN) : {
        slPlayCallback callback = NULL;
        interface_lock_shared(&pAudioPlayer->mPlay);
        callback = pAudioPlayer->mPlay.mCallback;
        callbackPContext = pAudioPlayer->mPlay.mContext;
        interface_unlock_shared(&pAudioPlayer->mPlay);
        if ((NULL != callback) && (pAudioPlayer->mPlay.mEventFlags & SL_PLAYEVENT_HEADSTALLED)) {
            (*callback)(&pAudioPlayer->mPlay.mItf, callbackPContext, SL_PLAYEVENT_HEADSTALLED);
        }
    }
    break;

    default:
        // FIXME where does the notification of SL_PLAYEVENT_HEADATEND, SL_PLAYEVENT_HEADMOVING fit?
        break;
    }
}


//-----------------------------------------------------------------------------
static void android_pushAudioTrackCallback(int event, void* user, void *info) {
    if (event == android::AudioTrack::EVENT_MORE_DATA) {
        fprintf(stderr, "received event EVENT_MORE_DATA from AudioTrack\n");
        // set size to 0 to signal we're not using the callback to write more data
        android::AudioTrack::Buffer* pBuff = (android::AudioTrack::Buffer*)info;
        pBuff->size = 0;

    } else if (event == android::AudioTrack::EVENT_MARKER) {
        fprintf(stderr, "received event EVENT_MARKER from AudioTrack\n");
    } else if (event == android::AudioTrack::EVENT_NEW_POS) {
        fprintf(stderr, "received event EVENT_NEW_POS from AudioTrack\n");
    }
}


//-----------------------------------------------------------------------------
SLresult sles_to_android_audioPlayerCreate(const SLDataSource *pAudioSrc,
        const SLDataSink *pAudioSnk,
        CAudioPlayer *pAudioPlayer) {

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
        break;
    //   -----------------------------------
    //   URI to MediaPlayer
    case SL_DATALOCATOR_URI: {
        pAudioPlayer->mAndroidObjType = MEDIAPLAYER;
        // save URI
        SLDataLocator_URI *dl_uri =  (SLDataLocator_URI *) pAudioSrc->pLocator;
        pAudioPlayer->mUri = (char*) malloc(1 + sizeof(SLchar)*strlen((char*)dl_uri->URI));
        strcpy(pAudioPlayer->mUri, (char*)dl_uri->URI);
        }
        break;
    default:
        pAudioPlayer->mAndroidObjType = INVALID_TYPE;
        result = SL_RESULT_PARAMETER_INVALID;
    }

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
        }
        if (pAudioPlayer->mAudioTrack->initCheck() != android::NO_ERROR) {
            result = SL_RESULT_CONTENT_UNSUPPORTED;
        }
        break;
    //-----------------------------------
    // MediaPlayer
    case MEDIAPLAYER: {
        pAudioPlayer->mMediaPlayer = new android::MediaPlayer();
        if (pAudioPlayer->mMediaPlayer == NULL) {
            result = SL_RESULT_MEMORY_FAILURE;
            break;
        }
        pAudioPlayer->mMediaPlayer->setAudioStreamType(ANDROID_DEFAULT_OUTPUT_STREAM_TYPE);
        if (pAudioPlayer->mMediaPlayer->setDataSource(android::String8(pAudioPlayer->mUri), NULL)
                != android::NO_ERROR) {
            result = SL_RESULT_CONTENT_UNSUPPORTED;
            break;
        }

        // FIXME move the call to MediaPlayer::prepare() to the start of the prefetching
        //       i.e. in SL ES: when setting the play state of the AudioPlayer to Paused.
        if (async == SL_BOOLEAN_FALSE) {
            if (pAudioPlayer->mMediaPlayer->prepare() != android::NO_ERROR) {
                fprintf(stderr, "Failed to prepare() MediaPlayer in synchronous mode for %s\n",
                        pAudioPlayer->mUri);
                result = SL_RESULT_CONTENT_UNSUPPORTED;
            }
        } else {
            // FIXME verify whether async prepare will be handled by SL ES framework or
            //       Android-specific code (and rely on MediaPlayer::prepareAsync() )
            fprintf(stderr, "FIXME implement async realize for a MediaPlayer\n");
        }
        }
        break;
    default:
        result = SL_RESULT_CONTENT_UNSUPPORTED;
    }

    return result;
}


//-----------------------------------------------------------------------------
SLresult sles_to_android_audioPlayerDestroy(CAudioPlayer *pAudioPlayer) {
    SLresult result = SL_RESULT_SUCCESS;
    //fprintf(stdout, "sles_to_android_audioPlayerDestroy\n");
    switch (pAudioPlayer->mAndroidObjType) {
    //-----------------------------------
    // AudioTrack
    case AUDIOTRACK_PUSH:
    case AUDIOTRACK_PULL:
        pAudioPlayer->mAudioTrack->stop();
        delete pAudioPlayer->mAudioTrack;
        pAudioPlayer->mAudioTrack = NULL;
        pAudioPlayer->mAndroidObjType = INVALID_TYPE;
        break;
    //-----------------------------------
    // MediaPlayer
    case MEDIAPLAYER:
        // FIXME destroy MediaPlayer
        if (pAudioPlayer->mMediaPlayer != NULL) {
            pAudioPlayer->mMediaPlayer->stop();
            pAudioPlayer->mMediaPlayer->setListener(0);
            pAudioPlayer->mMediaPlayer->disconnect();
            fprintf(stderr, "FIXME destroy MediaPlayer\n");
            //delete pAudioPlayer->mMediaPlayer;
            pAudioPlayer->mMediaPlayer = NULL;
            free(pAudioPlayer->mUri);
        }
        break;
    default:
        result = SL_RESULT_CONTENT_UNSUPPORTED;
    }

    return result;
}


//-----------------------------------------------------------------------------
SLresult sles_to_android_audioPlayerSetPlayState(IPlay *pPlayItf, SLuint32 state) {
    CAudioPlayer *ap = (CAudioPlayer *)pPlayItf->mThis;
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
            return SL_RESULT_PARAMETER_INVALID;
        }
        break;
    case MEDIAPLAYER:
        switch (state) {
        case SL_PLAYSTATE_STOPPED:
            fprintf(stdout, "setting AudioPlayer to SL_PLAYSTATE_STOPPED\n");
            ap->mMediaPlayer->stop();
            break;
        case SL_PLAYSTATE_PAUSED:
            fprintf(stdout, "setting AudioPlayer to SL_PLAYSTATE_PAUSED\n");
            //FIXME implement start of prefetching when transitioning from stopped to paused
            ap->mMediaPlayer->pause();
            break;
        case SL_PLAYSTATE_PLAYING:
            fprintf(stdout, "setting AudioPlayer to SL_PLAYSTATE_PLAYING\n");
            ap->mMediaPlayer->start();
            break;
        default:
            return SL_RESULT_PARAMETER_INVALID;
        }
        break;
    default:
        break;
    }
    return SL_RESULT_SUCCESS;
}


//-----------------------------------------------------------------------------
SLresult sles_to_android_audioPlayerUseEventMask(IPlay *pPlayItf, SLuint32 eventFlags) {
    CAudioPlayer *ap = (CAudioPlayer *)pPlayItf->mThis;
    switch(ap->mAndroidObjType) {
    case AUDIOTRACK_PUSH:
    case AUDIOTRACK_PULL:
        if (eventFlags & SL_PLAYEVENT_HEADATMARKER) {
            ap->mAudioTrack->setMarkerPosition( (uint32_t)((pPlayItf->mMarkerPosition
                    * ap->mAudioTrack->getSampleRate())/1000));
        } else {
            // clear marker
            ap->mAudioTrack->setMarkerPosition(0);
        }
        if (eventFlags & SL_PLAYEVENT_HEADATNEWPOS) {
            ap->mAudioTrack->setPositionUpdatePeriod( (uint32_t)((pPlayItf->mPositionUpdatePeriod
                    * ap->mAudioTrack->getSampleRate())/1000));
        } else {
            // clear periodic update
            ap->mAudioTrack->setPositionUpdatePeriod(0);
        }
        if (eventFlags & SL_PLAYEVENT_HEADATEND) {
            // FIXME support SL_PLAYEVENT_HEADATEND
            fprintf(stderr, "FIXME: IPlay_SetCallbackEventsMask(SL_PLAYEVENT_HEADATEND) on an SL_OBJECTID_AUDIOPLAYER to be implemented\n");
        }
        if (eventFlags & SL_PLAYEVENT_HEADMOVING) {
            // FIXME support SL_PLAYEVENT_HEADMOVING
            fprintf(stderr, "FIXME: IPlay_SetCallbackEventsMask(SL_PLAYEVENT_HEADMOVING) on an SL_OBJECTID_AUDIOPLAYER to be implemented\n");
        }
        if (eventFlags & SL_PLAYEVENT_HEADSTALLED) {
            // FIXME support SL_PLAYEVENT_HEADSTALLED
            fprintf(stderr, "FIXME: IPlay_SetCallbackEventsMask(SL_PLAYEVENT_HEADSTALLED) on an SL_OBJECTID_AUDIOPLAYER to be implemented\n");
        }
        break;
    case MEDIAPLAYER:
        //FIXME implement
        fprintf(stderr, "FIXME: IPlay_SetCallbackEventsMask() mapped to a MediaPlayer to be implemented\n");
        break;
    default:
        break;
    }
    return SL_RESULT_SUCCESS;

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
    case MEDIAPLAYER:
        ap->mMediaPlayer->getDuration((int*)pDurMsec);
        break;
    default:
        break;
    }
    return SL_RESULT_SUCCESS;
}


//-----------------------------------------------------------------------------
SLresult sles_to_android_audioPlayerGetPosition(IPlay *pPlayItf, SLmillisecond *pPosMsec) {
    CAudioPlayer *ap = (CAudioPlayer *)pPlayItf->mThis;
    switch(ap->mAndroidObjType) {
    case AUDIOTRACK_PUSH:
    case AUDIOTRACK_PULL:
        uint32_t positionInFrames;
        ap->mAudioTrack->getPosition(&positionInFrames);
        *pPosMsec = positionInFrames * 1000 / ap->mAudioTrack->getSampleRate();
        break;
    case MEDIAPLAYER:
        ap->mMediaPlayer->getCurrentPosition((int*)pPosMsec);
        break;
    default:
        break;
    }
    return SL_RESULT_SUCCESS;
}

//-----------------------------------------------------------------------------
SLresult sles_to_android_audioPlayerVolumeUpdate(IVolume *pVolItf) {
    CAudioPlayer *ap = (CAudioPlayer *)pVolItf->mThis;
    // FIXME use the FX Framework conversions
    android_audioPlayerUpdateStereoVolume(pVolItf);
    return SL_RESULT_SUCCESS;
}


//-----------------------------------------------------------------------------
SLresult sles_to_android_audioPlayerSetMute(IVolume *pVolItf, SLboolean mute) {
    CAudioPlayer *ap = (CAudioPlayer *)pVolItf->mThis;
    switch(ap->mAndroidObjType) {
    case AUDIOTRACK_PUSH:
    case AUDIOTRACK_PULL:
        // when unmuting: volume levels have already been updated in IVolume_SetMute
        ap->mAudioTrack->mute(mute == SL_BOOLEAN_TRUE);
        break;
    case MEDIAPLAYER:
        if (mute == SL_BOOLEAN_TRUE) {
            ap->mMediaPlayer->setVolume(0.0f, 0.0f);
        }
        // when unmuting: volume levels have already been updated in IVolume_SetMute which causes
        // the MediaPlayer to receive non 0 amplification values
        break;
    default:
        break;
    }
    return SL_RESULT_SUCCESS;
}


