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

SLresult sles_to_android_CheckAudioPlayerSourceSink(SLDataSource *pAudioSrc, SLDataSink *pAudioSnk)
{
    //--------------------------------------
    // Sink check:
    //     currently only OutputMix sinks are supported, regardless of the data source
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


SLresult sles_to_android_CreateAudioPlayer(SLDataSource *pAudioSrc,
        SLDataSink *pAudioSnk,
        AudioPlayer_class *pAudioPlayer) {

    SLresult result = SL_RESULT_SUCCESS;

    // currently only OutputMix sinks are supported
    // this has been verified in sles_to_android_CheckAudioPlayerSourceSink

    //--------------------------------------
    // Source check:
    SLuint32 locatorType = *(SLuint32 *)pAudioSrc->pLocator;
    //SLuint32 formatType = *(SLuint32 *)pAudioSrc->pFormat;
    //SLuint32 numBuffers = 0;
    switch (locatorType) {
    case SL_DATALOCATOR_BUFFERQUEUE:
        pAudioPlayer->mAndroidObjType = AUDIOTRACK_PUSH;
        pAudioPlayer->mAudioTrack = //new android::AudioTrack();
            new android::AudioTrack(
                    android::AudioSystem::MUSIC,            // streamType
                    44100,                                  // sampleRate
                    android::AudioSystem::PCM_16_BIT,       // format
                    // FIXME should be stereo, but mono gives more audio output for testing
                    android::AudioSystem::CHANNEL_OUT_MONO, // channels
                    256 * 20,                               // frameCount
                    0,                                      // flags
                    NULL,                    // cbf (callback)
                    NULL,                          // user
                    256 * 20);                              // notificationFrame
        break;
    case SL_DATALOCATOR_ADDRESS:
        pAudioPlayer->mAndroidObjType = MEDIAPLAYER;
        pAudioPlayer->mMediaPlayer = new android::MediaPlayer();
        break;
    default:
        pAudioPlayer->mAndroidObjType = INVALID_TYPE;
        result = SL_RESULT_PARAMETER_INVALID;
    }

    return result;

}

#endif
