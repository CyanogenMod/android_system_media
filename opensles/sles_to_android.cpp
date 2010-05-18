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


#include "sles_to_android.h"

#ifdef USE_ANDROID

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


//-----------------------------------------------------------------------------
SLresult sles_to_android_checkAudioPlayerSourceSink(const SLDataSource *pAudioSrc, const SLDataSink *pAudioSnk)
{
    //--------------------------------------
    // Sink check:
    //     currently only OutputMix sinks are supported, regardless of the data source
    // FIXME verify output mix is in realized state
    if (*(SLuint32 *)pAudioSnk->pLocator != SL_DATALOCATOR_OUTPUTMIX) {
        fprintf(stderr, "Cannot create audio player: data sink is not SL_DATALOCATOR_OUTPUTMIX\n");
        return SL_RESULT_PARAMETER_INVALID;
    }

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
            //FIXME add error message
            return SL_RESULT_CONTENT_UNSUPPORTED;
        default:
            //FIXME add error message
            return SL_RESULT_PARAMETER_INVALID;
        } // switch (formatType)
        } // case SL_DATALOCATOR_BUFFERQUEUE
        break;
    //------------------
    //   Address
    case SL_DATALOCATOR_ADDRESS:
        // FIXME add URI checks
        break;
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
SLresult sles_to_android_createAudioPlayer(const SLDataSource *pAudioSrc,
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
        pAudioPlayer->mAndroidObjType = AUDIOTRACK_PUSH;
        break;
    //   -----------------------------------
    //   Address to MediaPlayer
    case SL_DATALOCATOR_ADDRESS:
        pAudioPlayer->mAndroidObjType = MEDIAPLAYER;
        break;
    default:
        pAudioPlayer->mAndroidObjType = INVALID_TYPE;
        result = SL_RESULT_PARAMETER_INVALID;
    }

    return result;

}


//-----------------------------------------------------------------------------
SLresult sles_to_android_realizeAudioPlayer(CAudioPlayer *pAudioPlayer) {

    SLresult result = SL_RESULT_SUCCESS;

    switch (pAudioPlayer->mAndroidObjType) {
    //-----------------------------------
    // AudioTrack
    case AUDIOTRACK_PUSH:
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
                android_getMinFrameCount(sampleRate),                // frameCount
                0,                                                   // flags
                android_pushAudioTrackCallback,                      // callback
                (void *) pAudioPlayer,                               // user
                0);                                                  // notificationFrame
        }
        if (pAudioPlayer->mAudioTrack->initCheck() != android::NO_ERROR) {
            result = SL_RESULT_CONTENT_UNSUPPORTED;;
        }
        break;
    //-----------------------------------
    // MediaPlayer
    case MEDIAPLAYER:
        pAudioPlayer->mMediaPlayer = new android::MediaPlayer();
        // FIXME initialize MediaPlayer
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
        //FIXME implement
        break;
    default:
        break;
    }
    return SL_RESULT_SUCCESS;
}

#endif
