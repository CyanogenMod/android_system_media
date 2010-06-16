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


void android_audioPlayerUpdateStereoVolume(IVolume *pVolItf) {
    // should not be used when muted
    if (pVolItf->mMute) {
        return;
    }
    float leftVol = 1.0f, rightVol = 1.0f;

    CAudioPlayer *ap = (CAudioPlayer *)pVolItf->mThis;
    //FIXME cache channel count?
    int channelCount = 0;
    switch (ap->mAndroidObjType) {
        case AUDIOTRACK_PUSH:
        case AUDIOTRACK_PULL:
            channelCount = ap->mAudioTrackData.mAudioTrack->channelCount();
            break;
        case MEDIAPLAYER:
            //FIXME add support for querying channel count from a MediaPlayer
            fprintf(stderr, "FIXME add support for querying channel count from a MediaPlayer");
            channelCount = 1;
        default:
            fprintf(stderr, "Error in android_audioPlayerUpdateStereoVolume(): shouldn't hit this");
            channelCount = 1;
            break;
    }
    //int muteSoloLeft, muteSoleRight;
    //muteSoloLeft = (mChannelMutes & CHANNEL_OUT_FRONT_LEFT) >> 2;
    //muteSoloRight = (mChannelMutes & CHANNEL_OUT_FRONT_RIGHT) >> 3;

    // compute amplification as the combination of volume level and stereo position

    // amplification from volume level
    // FIXME use the FX Framework conversions
    pVolItf->mAmplFromVolLevel = pow(10, (float)pVolItf->mLevel/2000);
    leftVol *= pVolItf->mAmplFromVolLevel;
    rightVol *= pVolItf->mAmplFromVolLevel;

    // amplification from stereo position
    if (pVolItf->mEnableStereoPosition) {
        // panning law depends on number of channels of content: stereo panning vs 2ch. balance
        if(1 == channelCount) {
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

    switch(ap->mAndroidObjType) {
    case AUDIOTRACK_PUSH:
    case AUDIOTRACK_PULL:
        ap->mAudioTrackData.mAudioTrack->setVolume(leftVol, rightVol);
        break;
    case MEDIAPLAYER:
        ap->mMediaPlayerData.mMediaPlayer->setVolume(leftVol, rightVol);
        break;
    default:
        break;
    }
}


//-----------------------------------------------------------------------------
// Used to handle MediaPlayer callback
class MediaPlayerEventListener: public android::MediaPlayerListener
{
public:
    MediaPlayerEventListener(CAudioPlayer *pPlayer);
    ~MediaPlayerEventListener();
    void notify(int msg, int ext1, int ext2);
private:
    CAudioPlayer *mPlayer;
};

MediaPlayerEventListener::MediaPlayerEventListener(CAudioPlayer *pPlayer)
{
    //fprintf(stderr, "MediaPlayerEventListener constructor called for %p\n", pPlayer);
    mPlayer = pPlayer;
}

MediaPlayerEventListener::~MediaPlayerEventListener()
{
    //fprintf(stderr, "MediaPlayerEventListener destructor called with player %p\n", mPlayer);
}

void MediaPlayerEventListener::notify(int msg, int ext1, int ext2)
{
    fprintf(stderr, "MediaPlayerEventListener::notify(%d, %d, %d) called\n", msg, ext1, ext2);
    switch(msg) {
    case android::MEDIA_PREPARED: {
        object_lock_exclusive(&mPlayer->mObject);
        mPlayer->mAndroidObjState = ANDROID_READY;
        object_unlock_exclusive(&mPlayer->mObject);
        slPrefetchCallback callback = NULL;
        void * callbackPContext = NULL;
        interface_lock_exclusive(&mPlayer->mPrefetchStatus);
        mPlayer->mPrefetchStatus.mLevel = 1000;
        mPlayer->mPrefetchStatus.mStatus = SL_PREFETCHSTATUS_SUFFICIENTDATA;
        // FIXME send status change, and fill level change
        SLuint32 mask = mPlayer->mPrefetchStatus.mCallbackEventsMask;
        if (mask) {
            callback = mPlayer->mPrefetchStatus.mCallback;
            callbackPContext = mPlayer->mPrefetchStatus.mContext;
        }
        interface_unlock_exclusive(&mPlayer->mPrefetchStatus);
        if (NULL != callback) {
            (*callback)(&mPlayer->mPrefetchStatus.mItf, callbackPContext,
                    SL_PREFETCHEVENT_STATUSCHANGE | SL_PREFETCHEVENT_FILLLEVELCHANGE);
        }
        }
        break;
    case android::MEDIA_PLAYBACK_COMPLETE: {
        // FIXME update playstate?
        fprintf(stderr, "FIXME set playstate to PAUSED\n");
        slPlayCallback callback = NULL;
        void * callbackPContext = NULL;
        interface_lock_shared(&mPlayer->mPlay);
        if (mPlayer->mPlay.mEventFlags & SL_PLAYEVENT_HEADATEND) {
            callback = mPlayer->mPlay.mCallback;
            callbackPContext = mPlayer->mPlay.mContext;
        }
        interface_unlock_shared(&mPlayer->mPlay);
        if (NULL != callback) {
            (*callback)(&mPlayer->mPlay.mItf, callbackPContext, SL_PLAYEVENT_HEADATEND);
        }
        }
        break;
    case android::MEDIA_BUFFERING_UPDATE: {
        slPrefetchCallback callback = NULL;
        void * callbackPContext = NULL;
        interface_lock_exclusive(&mPlayer->mPrefetchStatus);
        mPlayer->mPrefetchStatus.mLevel = ext1 > 100 ? 1000 : ext1 * 10;
        if (mPlayer->mPrefetchStatus.mCallbackEventsMask & SL_PREFETCHEVENT_FILLLEVELCHANGE) {
            callback = mPlayer->mPrefetchStatus.mCallback;
            callbackPContext = mPlayer->mPrefetchStatus.mContext;
        }
        interface_unlock_exclusive(&mPlayer->mPrefetchStatus);
        if (NULL != callback) {
                    (*callback)(&mPlayer->mPrefetchStatus.mItf, callbackPContext,
                            SL_PREFETCHEVENT_FILLLEVELCHANGE);
        }
        }
        break;
    case android::MEDIA_ERROR: {
        // An error was encountered, there is no real way in the spec to signal a prefetch error.
        // Simulate this by signaling a 0 fill level, and a status change to UNDERFLOW, regardless
        // of the mask
        object_lock_exclusive(&mPlayer->mObject);
        mPlayer->mAndroidObjState = ANDROID_UNINITIALIZED;
        object_unlock_exclusive(&mPlayer->mObject);
        slPrefetchCallback callback = NULL;
        void * callbackPContext = NULL;
        interface_lock_exclusive(&mPlayer->mPrefetchStatus);
        mPlayer->mPrefetchStatus.mLevel = 0;
        mPlayer->mPrefetchStatus.mStatus = SL_PREFETCHSTATUS_UNDERFLOW;
        SLuint32 mask = mPlayer->mPrefetchStatus.mCallbackEventsMask;
        if (mask) {
            callback = mPlayer->mPrefetchStatus.mCallback;
            callbackPContext = mPlayer->mPrefetchStatus.mContext;
        }
        interface_unlock_exclusive(&mPlayer->mPrefetchStatus);
        if (NULL != callback) {
            (*callback)(&mPlayer->mPrefetchStatus.mItf, callbackPContext,
                    SL_PREFETCHEVENT_STATUSCHANGE | SL_PREFETCHEVENT_FILLLEVELCHANGE);
        }
        }
        break;
    case android::MEDIA_INFO:
    case android::MEDIA_SEEK_COMPLETE:
        break;
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
        bool headStalled = (pAudioPlayer->mPlay.mEventFlags & SL_PLAYEVENT_HEADSTALLED) != 0;
        interface_unlock_shared(&pAudioPlayer->mPlay);
        if ((NULL != callback) && headStalled) {
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
        pAudioPlayer->mAudioTrackData.mAudioTrack = NULL;
        pAudioPlayer->mPlaybackRate.mCapabilities = SL_RATEPROP_NOPITCHCORAUDIO;
        break;
    //   -----------------------------------
    //   URI to MediaPlayer
    case SL_DATALOCATOR_URI:
        pAudioPlayer->mAndroidObjType = MEDIAPLAYER;
        pAudioPlayer->mpLock = new android::Mutex();
        pAudioPlayer->mAndroidObjState = ANDROID_UNINITIALIZED;
        pAudioPlayer->mMediaPlayerData.mMediaPlayer = NULL;
        pAudioPlayer->mPlaybackRate.mCapabilities = 0;
        break;
    default:
        pAudioPlayer->mAndroidObjType = INVALID_TYPE;
        pAudioPlayer->mpLock = NULL;
        pAudioPlayer->mPlaybackRate.mCapabilities = 0;
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

        pAudioPlayer->mAudioTrackData.mAudioTrack = new android::AudioTrack(
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
        if (pAudioPlayer->mAudioTrackData.mAudioTrack->initCheck() != android::NO_ERROR) {
            result = SL_RESULT_CONTENT_UNSUPPORTED;
        }
        break;
    //-----------------------------------
    // MediaPlayer
    case MEDIAPLAYER: {
        object_lock_exclusive(&pAudioPlayer->mObject);
        pAudioPlayer->mAndroidObjState = ANDROID_PREPARING;
        pAudioPlayer->mMediaPlayerData.mMediaPlayer = new android::MediaPlayer();
        if (pAudioPlayer->mMediaPlayerData.mMediaPlayer == NULL) {
            result = SL_RESULT_MEMORY_FAILURE;
            break;
        }

        pAudioPlayer->mMediaPlayerData.mMediaPlayer->setAudioStreamType(
                ANDROID_DEFAULT_OUTPUT_STREAM_TYPE);

        if (pAudioPlayer->mMediaPlayerData.mMediaPlayer->setDataSource(
                android::String8((const char *) pAudioPlayer->mDataSource.mLocator.mURI.URI), NULL)
                    != android::NO_ERROR) {
            result = SL_RESULT_CONTENT_UNSUPPORTED;
            break;
        }

        // create event listener
        android::sp<MediaPlayerEventListener> listener = new MediaPlayerEventListener(pAudioPlayer);
        pAudioPlayer->mMediaPlayerData.mMediaPlayer->setListener(listener);

        object_unlock_exclusive(&pAudioPlayer->mObject);
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
        // FIXME group in one function?
        if (pAudioPlayer->mAudioTrackData.mAudioTrack != NULL) {
            pAudioPlayer->mAudioTrackData.mAudioTrack->stop();
            delete pAudioPlayer->mAudioTrackData.mAudioTrack;
            pAudioPlayer->mAudioTrackData.mAudioTrack = NULL;
            pAudioPlayer->mAndroidObjType = INVALID_TYPE;
        }
        break;
    //-----------------------------------
    // MediaPlayer
    case MEDIAPLAYER:
        // FIXME group in one function?
        if (pAudioPlayer->mMediaPlayerData.mMediaPlayer != NULL) {
            pAudioPlayer->mMediaPlayerData.mMediaPlayer->setListener(0);
            pAudioPlayer->mMediaPlayerData.mMediaPlayer->stop();
            pAudioPlayer->mMediaPlayerData.mMediaPlayer->disconnect();
            // FIXME what's going wrong when deleting the MediaPlayer?
            fprintf(stderr, "FIXME destroy MediaPlayer\n");
            //delete pAudioPlayer->mMediaPlayerData.mMediaPlayer;
            pAudioPlayer->mMediaPlayerData.mMediaPlayer = NULL;
        }
        break;
    default:
        result = SL_RESULT_CONTENT_UNSUPPORTED;
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
    case AUDIOTRACK_PULL: {
        // get the content sample rate
        object_lock_peek(ap);
        uint32_t contentRate =
            sles_to_android_sampleRate(ap->mDataSource.mFormat.mPCM.samplesPerSec);
        object_unlock_peek(ap);
        // apply the SL ES playback rate on the AudioTrack as a factor of its content sample rate
        ap->mpLock->lock();
        if (ap->mAudioTrackData.mAudioTrack != NULL) {
            ap->mAudioTrackData.mAudioTrack->setSampleRate(contentRate * (rate/1000.0f));
        }
        ap->mpLock->unlock();
        }
        break;
    case MEDIAPLAYER:
        result = SL_RESULT_FEATURE_UNSUPPORTED;
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
        if (constraints != (constraints & SL_RATEPROP_NOPITCHCORAUDIO)) {
            result = SL_RESULT_FEATURE_UNSUPPORTED;
        }
        break;
    case MEDIAPLAYER:
        result = SL_RESULT_FEATURE_UNSUPPORTED;
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
        *pCapabilities = SL_RATEPROP_NOPITCHCORAUDIO;
        break;
    case MEDIAPLAYER:
        *pCapabilities = 0;
        break;
    default:
        *pCapabilities = 0;
        break;
    }
    return SL_RESULT_SUCCESS;
}


//-----------------------------------------------------------------------------
SLresult sles_to_android_audioPlayerSetPlayState(IPlay *pPlayItf, SLuint32 state) {
    CAudioPlayer *ap = (CAudioPlayer *)pPlayItf->mThis;
    SLresult result = SL_RESULT_SUCCESS;
    switch(ap->mAndroidObjType) {
    case AUDIOTRACK_PUSH:
    case AUDIOTRACK_PULL:
        switch (state) {
        case SL_PLAYSTATE_STOPPED:
            fprintf(stdout, "setting AudioPlayer to SL_PLAYSTATE_STOPPED\n");
            ap->mAudioTrackData.mAudioTrack->stop();
            break;
        case SL_PLAYSTATE_PAUSED:
            fprintf(stdout, "setting AudioPlayer to SL_PLAYSTATE_PAUSED\n");
            ap->mAudioTrackData.mAudioTrack->pause();
            break;
        case SL_PLAYSTATE_PLAYING:
            fprintf(stdout, "setting AudioPlayer to SL_PLAYSTATE_PLAYING\n");
            ap->mAudioTrackData.mAudioTrack->start();
            break;
        default:
            result = SL_RESULT_PARAMETER_INVALID;
        }
        break;
    case MEDIAPLAYER:
        switch (state) {
        case SL_PLAYSTATE_STOPPED:
            fprintf(stdout, "setting AudioPlayer to SL_PLAYSTATE_STOPPED\n");
            ap->mMediaPlayerData.mMediaPlayer->stop();
            break;
        case SL_PLAYSTATE_PAUSED: {
            fprintf(stdout, "setting AudioPlayer to SL_PLAYSTATE_PAUSED\n");
            object_lock_peek(&ap);
            AndroidObject_state state = ap->mAndroidObjState;
            object_unlock_peek(&ap);
            switch(state) {
                case(ANDROID_UNINITIALIZED):
                    break;
                case(ANDROID_PREPARING):
                    if (ap->mMediaPlayerData.mMediaPlayer->prepareAsync() != android::NO_ERROR) {
                        fprintf(stderr, "Failed to prepareAsync() MediaPlayer for %s\n",
                                ap->mDataSource.mLocator.mURI.URI);
                        result = SL_RESULT_CONTENT_UNSUPPORTED;
                        ap->mMediaPlayerData.mMediaPlayer->setListener(0);
                        // should update state but this is going away
                    }
                    break;
                case(ANDROID_PREPARED):
                case(ANDROID_PREFETCHING):
                case(ANDROID_READY):
                    // FIXME pause the actual playback
                    fprintf(stderr, "FIXME implement pause()");
                    //ap->mMediaPlayerData.mMediaPlayer->pause();
                    break;
                default:
                    break;
            }

            /*else {
                if (ap->mMediaPlayerData.mMediaPlayer->prepareAsync() != android::NO_ERROR) {
                    fprintf(stderr, "Failed to prepareAsync() MediaPlayer for %s\n",
                            ap->mDataSource.mLocator.mURI.URI);
                    result = SL_RESULT_CONTENT_UNSUPPORTED;
                    ap->mMediaPlayerData.mMediaPlayer->setListener(0);
                }
                ap->mMediaPlayerData.mPrepared = true;
                */
            } break;
        case SL_PLAYSTATE_PLAYING: {
            fprintf(stdout, "setting AudioPlayer to SL_PLAYSTATE_PLAYING\n");
            object_lock_peek(&ap);
            AndroidObject_state state = ap->mAndroidObjState;
            object_unlock_peek(&ap);
            if (state >= ANDROID_READY) {
                ap->mMediaPlayerData.mMediaPlayer->start();
            }
            } break;
        default:
            result = SL_RESULT_PARAMETER_INVALID;
        }
        break;
    default:
        break;
    }
    return result;
}


//-----------------------------------------------------------------------------
SLresult sles_to_android_audioPlayerUseEventMask(IPlay *pPlayItf, SLuint32 eventFlags) {
    CAudioPlayer *ap = (CAudioPlayer *)pPlayItf->mThis;
    switch(ap->mAndroidObjType) {
    case AUDIOTRACK_PUSH:
    case AUDIOTRACK_PULL:
        if (eventFlags & SL_PLAYEVENT_HEADATMARKER) {
            //FIXME getSampleRate() returns the current playback sample rate, not the content
            //      sample rate, verify if formula valid
            fprintf(stderr, "FIXME: fix marker computation due to sample rate reporting behavior");
            ap->mAudioTrackData.mAudioTrack->setMarkerPosition((uint32_t)((pPlayItf->mMarkerPosition
                    * ap->mAudioTrackData.mAudioTrack->getSampleRate())/1000));
        } else {
            // clear marker
            ap->mAudioTrackData.mAudioTrack->setMarkerPosition(0);
        }
        if (eventFlags & SL_PLAYEVENT_HEADATNEWPOS) {
            //FIXME getSampleRate() returns the current playback sample rate, not the content
            //      sample rate, verify if formula valid
            fprintf(stderr, "FIXME: fix marker computation due to sample rate reporting behavior");
            ap->mAudioTrackData.mAudioTrack->setPositionUpdatePeriod(
                    (uint32_t)((pPlayItf->mPositionUpdatePeriod
                            * ap->mAudioTrackData.mAudioTrack->getSampleRate())/1000));
        } else {
            // clear periodic update
            ap->mAudioTrackData.mAudioTrack->setPositionUpdatePeriod(0);
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
        // nothing to do for SL_PLAYEVENT_HEADATEND, callback event will be checked against mask
        if (eventFlags & SL_PLAYEVENT_HEADATMARKER) {
            // FIXME support SL_PLAYEVENT_HEADATMARKER
            fprintf(stderr, "FIXME: IPlay_SetCallbackEventsMask(SL_PLAYEVENT_HEADATMARKER) on an SL_OBJECTID_AUDIOPLAYER to be implemented\n");
        }
        if (eventFlags & SL_PLAYEVENT_HEADATNEWPOS) {
            // FIXME support SL_PLAYEVENT_HEADATNEWPOS
            fprintf(stderr, "FIXME: IPlay_SetCallbackEventsMask(SL_PLAYEVENT_HEADATNEWPOS) on an SL_OBJECTID_AUDIOPLAYER to be implemented\n");
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
        ap->mMediaPlayerData.mMediaPlayer->getDuration((int*)pDurMsec);
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
        ap->mAudioTrackData.mAudioTrack->getPosition(&positionInFrames);
        //FIXME getSampleRate() returns the current playback sample rate, not the content
        //      sample rate, verify if formula valid
        fprintf(stderr, "FIXME: fix marker computation due to sample rate reporting behavior");
        *pPosMsec = positionInFrames * 1000 / ap->mAudioTrackData.mAudioTrack->getSampleRate();
        break;
    case MEDIAPLAYER:
        ap->mMediaPlayerData.mMediaPlayer->getCurrentPosition((int*)pPosMsec);
        break;
    default:
        break;
    }
    return SL_RESULT_SUCCESS;
}

//-----------------------------------------------------------------------------
SLresult sles_to_android_audioPlayerVolumeUpdate(IVolume *pVolItf) {
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
        ap->mAudioTrackData.mAudioTrack->mute(mute == SL_BOOLEAN_TRUE);
        break;
    case MEDIAPLAYER:
        if (mute == SL_BOOLEAN_TRUE) {
            ap->mMediaPlayerData.mMediaPlayer->setVolume(0.0f, 0.0f);
        }
        // when unmuting: volume levels have already been updated in IVolume_SetMute which causes
        // the MediaPlayer to receive non 0 amplification values
        break;
    default:
        break;
    }
    return SL_RESULT_SUCCESS;
}


