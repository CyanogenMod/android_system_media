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


//-----------------------------------------------------------------------------
XAresult android_Player_create(CMediaPlayer *mp) {

    XAresult result = XA_RESULT_SUCCESS;

    // FIXME verify data source
    const SLDataSource *pDataSrc = &mp->mDataSource.u.mSource;
    // FIXME verify audio data sink
    const SLDataSink *pAudioSnk = &mp->mAudioSink.u.mSink;
    // FIXME verify image data sink
    const SLDataSink *pVideoSnk = &mp->mImageVideoSink.u.mSink;

    SLuint32 sourceLocator = *(SLuint32 *)pDataSrc->pLocator;
    switch(sourceLocator) {
    case XA_DATALOCATOR_ANDROIDBUFFERQUEUE:
        mp->mAndroidObjType = AV_PLR_TS_ABQ;
        break;
    case XA_DATALOCATOR_URI: // intended fall-through
    case XA_DATALOCATOR_ADDRESS: // intended fall-through
    case SL_DATALOCATOR_ANDROIDFD: // intended fall-through
    default:
        SL_LOGE("Unable to create MediaPlayer for data source locator 0x%lx", sourceLocator);
        result = XA_RESULT_PARAMETER_INVALID;
        break;

    }

    mp->mAndroidObjState = ANDROID_UNINITIALIZED;
    mp->mStreamType = ANDROID_DEFAULT_OUTPUT_STREAM_TYPE;
    mp->mSessionId = android::AudioSystem::newAudioSessionId();

    mp->mAndroidAudioLevels.mAmplFromVolLevel = 1.0f;
    mp->mAndroidAudioLevels.mAmplFromStereoPos[0] = 1.0f;
    mp->mAndroidAudioLevels.mAmplFromStereoPos[1] = 1.0f;
    mp->mAndroidAudioLevels.mAmplFromDirectLevel = 1.0f; // matches initial mDirectLevel value
    mp->mAndroidAudioLevels.mAuxSendLevel = 0;
    mp->mDirectLevel = 0; // no attenuation

    return result;
}


//-----------------------------------------------------------------------------
// FIXME abstract out the diff between CMediaPlayer and CAudioPlayer
XAresult android_Player_realize(CMediaPlayer *mp, SLboolean async) {
    SL_LOGI("android_Player_realize_l(%p)", mp);
    XAresult result = XA_RESULT_SUCCESS;

    const SLDataSource *pDataSrc = &mp->mDataSource.u.mSource;
    const SLuint32 sourceLocator = *(SLuint32 *)pDataSrc->pLocator;

    AudioPlayback_Parameters ap_params;
    ap_params.sessionId = mp->mSessionId;
    ap_params.streamType = mp->mStreamType;
    ap_params.trackcb = NULL;
    ap_params.trackcbUser = NULL;

    switch(mp->mAndroidObjType) {
    case AV_PLR_TS_ABQ: {
        mp->mAVPlayer = new android::StreamPlayer(&ap_params);
        mp->mAVPlayer->init();
        }
        break;
    case INVALID_TYPE:
    default:
        SL_LOGE("Unable to realize MediaPlayer, invalid internal Android object type");
        result = XA_RESULT_PARAMETER_INVALID;
        break;

    }

    return result;
}


//-----------------------------------------------------------------------------
/**
 * pre-condition: avp != NULL
 */
XAresult android_Player_setPlayState(android::AVPlayer *avp, SLuint32 playState,
        AndroidObject_state objState)
{
    XAresult result = XA_RESULT_SUCCESS;

    switch (playState) {
     case SL_PLAYSTATE_STOPPED: {
         SL_LOGV("setting AVPlayer to SL_PLAYSTATE_STOPPED");
         avp->stop();
     } break;
     case SL_PLAYSTATE_PAUSED: {
         SL_LOGV("setting AVPlayer to SL_PLAYSTATE_PAUSED");
         switch(objState) {
         case(ANDROID_UNINITIALIZED):
             avp->prepare();
         break;
         case(ANDROID_PREPARING):
             break;
         case(ANDROID_READY):
             avp->pause();
         break;
         default:
             SL_LOGE("Android object in invalid state");
             break;
         }
     } break;
     case SL_PLAYSTATE_PLAYING: {
         SL_LOGV("setting AVPlayer to SL_PLAYSTATE_PLAYING");
         switch(objState) {
         case(ANDROID_UNINITIALIZED):
             // FIXME PRIORITY1 prepare should update the obj state
             //  for the moment test app sets state to PAUSED to prepare, then to PLAYING
             //avp->prepare();

         // fall through
         case(ANDROID_PREPARING):
         case(ANDROID_READY):
             avp->play();
         break;
         default:
             SL_LOGE("Android object in invalid state");
             break;
         }
     } break;

     default:
         // checked by caller, should not happen
         break;
     }

    return result;
}


//-----------------------------------------------------------------------------
// FIXME abstract out the diff between CMediaPlayer and CAudioPlayer
void android_Player_androidBufferQueue_registerCallback_l(CMediaPlayer *mp) {
    if (mp->mAVPlayer != 0) {
        SL_LOGI("android_Player_androidBufferQueue_registerCallback_l");
        android::StreamPlayer* splr = (android::StreamPlayer*)(mp->mAVPlayer.get());
        splr->registerQueueCallback(mp->mAndroidBufferQueue.mCallback,
                mp->mAndroidBufferQueue.mContext, (const void*)&(mp->mAndroidBufferQueue.mItf));
    }
}

// FIXME abstract out the diff between CMediaPlayer and CAudioPlayer
void android_Player_androidBufferQueue_enqueue_l(CMediaPlayer *mp,
        SLuint32 bufferId, SLuint32 length, SLAbufferQueueEvent event, void *pData) {
    if (mp->mAVPlayer != 0) {
        android::StreamPlayer* splr = (android::StreamPlayer*)(mp->mAVPlayer.get());
        splr->appEnqueue(bufferId, length, event, pData);
    }
}



