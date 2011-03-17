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
static void player_handleMediaPlayerEventNotifications(int event, int data1, int data2, void* user)
{
    if (NULL == user) {
        return;
    }

    CMediaPlayer* mp = (CMediaPlayer*) user;
    //SL_LOGV("received event %d, data %d from AVPlayer", event, data1);

    switch(event) {

      case android::GenericPlayer::kEventPrepared: {
        if (PLAYER_SUCCESS == data1) {
            object_lock_exclusive(&mp->mObject);
            SL_LOGV("Received AVPlayer::kEventPrepared from AVPlayer for CMediaPlayer %p", mp);
            mp->mAndroidObjState = ANDROID_READY;
            object_unlock_exclusive(&mp->mObject);
        }
        break;
      }

      case android::GenericPlayer::kEventHasVideoSize: {
        SL_LOGV("Received AVPlayer::kEventHasVideoSize (%d,%d) for CMediaPlayer %p",
                data1, data2, mp);

        object_lock_exclusive(&mp->mObject);

        // remove an existing video info entry (here we only have one video stream)
        for(size_t i=0 ; i < mp->mStreamInfo.mStreamInfoTable.size() ; i++) {
            if (XA_DOMAINTYPE_VIDEO == mp->mStreamInfo.mStreamInfoTable.itemAt(i).domain) {
                mp->mStreamInfo.mStreamInfoTable.removeAt(i);
                break;
            }
        }
        // update the stream information with a new video info entry
        StreamInfo streamInfo;
        streamInfo.domain = XA_DOMAINTYPE_VIDEO;
        streamInfo.videoInfo.codecId = 0;// unknown, we don't have that info FIXME
        streamInfo.videoInfo.width = (XAuint32)data1;
        streamInfo.videoInfo.height = (XAuint32)data2;
        streamInfo.videoInfo.bitRate = 0;// unknown, we don't have that info FIXME
        streamInfo.videoInfo.duration = XA_TIME_UNKNOWN;
        StreamInfo &contInfo = mp->mStreamInfo.mStreamInfoTable.editItemAt(0);
        contInfo.containerInfo.numStreams = 1;
        ssize_t index = mp->mStreamInfo.mStreamInfoTable.add(streamInfo);

        xaStreamEventChangeCallback callback = mp->mStreamInfo.mCallback;
        void* callbackPContext = mp->mStreamInfo.mContext;

        object_unlock_exclusive(&mp->mObject);

        // notify (outside of lock) that the stream information has been updated
        if ((NULL != callback) && (index >= 0)) {
            (*callback)(&mp->mStreamInfo.mItf, XA_STREAMCBEVENT_PROPERTYCHANGE /*eventId*/,
                    1 /*streamIndex, only one stream supported here, 0 is reserved*/,
                    NULL /*pEventData, always NULL in OpenMAX AL 1.0.1*/,
                    callbackPContext /*pContext*/);
        }
        break;
      }

    default:
        SL_LOGE("Received unknown event %d, data %d from AVPlayer", event, data1);
        break;
    }
}


//-----------------------------------------------------------------------------
XAresult android_Player_checkSourceSink(CMediaPlayer *mp) {

    XAresult result = XA_RESULT_SUCCESS;

    const SLDataSource *pSrc    = &mp->mDataSource.u.mSource;
    const SLDataSink *pAudioSnk = &mp->mAudioSink.u.mSink;

    // format check:
    const SLuint32 sourceLocatorType = *(SLuint32 *)pSrc->pLocator;
    const SLuint32 sourceFormatType  = *(SLuint32 *)pSrc->pFormat;
    const SLuint32 audioSinkLocatorType = *(SLuint32 *)pAudioSnk->pLocator;
    //const SLuint32 sinkFormatType = *(SLuint32 *)pAudioSnk->pFormat;

    // Source check
    switch(sourceLocatorType) {

    case XA_DATALOCATOR_ANDROIDBUFFERQUEUE: {
        switch (sourceFormatType) {
        case XA_DATAFORMAT_MIME: {
            SLDataFormat_MIME *df_mime = (SLDataFormat_MIME *) pSrc->pFormat;
            if (SL_CONTAINERTYPE_MPEG_TS != df_mime->containerType) {
                SL_LOGE("Cannot create player with XA_DATALOCATOR_ANDROIDBUFFERQUEUE data source "
                        "that is not fed MPEG-2 TS data");
                return SL_RESULT_CONTENT_UNSUPPORTED;
            }
        } break;
        default:
            SL_LOGE("Cannot create player with XA_DATALOCATOR_ANDROIDBUFFERQUEUE data source "
                    "without SL_DATAFORMAT_MIME format");
            return XA_RESULT_CONTENT_UNSUPPORTED;
        }
    } break;

    case XA_DATALOCATOR_URI: // intended fall-through
    case XA_DATALOCATOR_ANDROIDFD:
        break;

    default:
        SL_LOGE("Cannot create media player with data locator type 0x%x",
                (unsigned) sourceLocatorType);
        return SL_RESULT_PARAMETER_INVALID;
    }// switch (locatorType)

    // Audio sink check: only playback is supported here
    switch(audioSinkLocatorType) {

    case XA_DATALOCATOR_OUTPUTMIX:
        break;

    default:
        SL_LOGE("Cannot create media player with audio sink data locator of type 0x%x",
                (unsigned) audioSinkLocatorType);
        return XA_RESULT_PARAMETER_INVALID;
    }// switch (locaaudioSinkLocatorTypeorType)

    return result;
}


//-----------------------------------------------------------------------------
XAresult android_Player_create(CMediaPlayer *mp) {

    XAresult result = XA_RESULT_SUCCESS;

    // FIXME verify data source
    const SLDataSource *pDataSrc = &mp->mDataSource.u.mSource;
    // FIXME verify audio data sink
    const SLDataSink *pAudioSnk = &mp->mAudioSink.u.mSink;
    // FIXME verify image data sink
    const SLDataSink *pVideoSnk = &mp->mImageVideoSink.u.mSink;

    XAuint32 sourceLocator = *(XAuint32 *)pDataSrc->pLocator;
    switch(sourceLocator) {
    // FIXME support Android simple buffer queue as well
    case XA_DATALOCATOR_ANDROIDBUFFERQUEUE:
        mp->mAndroidObjType = AV_PLR_TS_ABQ;
        break;
    case XA_DATALOCATOR_URI: // intended fall-through
    case SL_DATALOCATOR_ANDROIDFD:
        mp->mAndroidObjType = AV_PLR_URIFD;
        break;
    case XA_DATALOCATOR_ADDRESS: // intended fall-through
    default:
        SL_LOGE("Unable to create MediaPlayer for data source locator 0x%lx", sourceLocator);
        result = XA_RESULT_PARAMETER_INVALID;
        break;
    }

    mp->mAndroidObjState = ANDROID_UNINITIALIZED;
    mp->mStreamType = ANDROID_DEFAULT_OUTPUT_STREAM_TYPE;
    mp->mSessionId = android::AudioSystem::newAudioSessionId();

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
        mp->mAVPlayer = new android::StreamPlayer(&ap_params, true /*hasVideo*/);
        mp->mAVPlayer->init(player_handleMediaPlayerEventNotifications, (void*)mp);
        }
        break;
    case AV_PLR_URIFD: {
        mp->mAVPlayer = new android::LocAVPlayer(&ap_params, true /*hasVideo*/);
        mp->mAVPlayer->init(player_handleMediaPlayerEventNotifications, (void*)mp);
        switch (mp->mDataSource.mLocator.mLocatorType) {
        case XA_DATALOCATOR_URI:
            ((android::LocAVPlayer*)mp->mAVPlayer.get())->setDataSource(
                    (const char*)mp->mDataSource.mLocator.mURI.URI);
            break;
        case XA_DATALOCATOR_ANDROIDFD: {
            int64_t offset = (int64_t)mp->mDataSource.mLocator.mFD.offset;
            ((android::LocAVPlayer*)mp->mAVPlayer.get())->setDataSource(
                    (int)mp->mDataSource.mLocator.mFD.fd,
                    offset == SL_DATALOCATOR_ANDROIDFD_USE_FILE_SIZE ?
                            (int64_t)PLAYER_FD_FIND_FILE_SIZE : offset,
                    (int64_t)mp->mDataSource.mLocator.mFD.length);
            }
            break;
        default:
            SL_LOGE("Invalid or unsupported data locator type %lu for data source",
                    mp->mDataSource.mLocator.mLocatorType);
            result = XA_RESULT_PARAMETER_INVALID;
        }
        }
        break;
    case INVALID_TYPE: // intended fall-through
    default:
        SL_LOGE("Unable to realize MediaPlayer, invalid internal Android object type");
        result = XA_RESULT_PARAMETER_INVALID;
        break;
    }

    return result;
}

//-----------------------------------------------------------------------------
XAresult android_Player_destroy(CMediaPlayer *mp) {
    SL_LOGI("android_Player_destroy(%p)", mp);
    XAresult result = XA_RESULT_SUCCESS;

    if (mp->mAVPlayer != 0) {
        mp->mAVPlayer.clear();
    }

    return result;
}

//-----------------------------------------------------------------------------
/**
 * pre-conditions: avp != NULL, surface != NULL
 */
XAresult android_Player_setVideoSurface(android::GenericMediaPlayer *avp,
        const android::sp<android::Surface> &surface) {
    XAresult result = XA_RESULT_SUCCESS;

    avp->setVideoSurface(surface);

    return result;
}


/**
 * pre-conditions: avp != NULL, surfaceTexture != NULL
 */
XAresult android_Player_setVideoSurfaceTexture(android::GenericMediaPlayer *avp,
        const android::sp<android::ISurfaceTexture> &surfaceTexture) {
    XAresult result = XA_RESULT_SUCCESS;

    avp->setVideoSurfaceTexture(surfaceTexture);

    return result;
}


XAresult android_Player_getDuration(IPlay *pPlayItf, XAmillisecond *pDurMsec) {
    XAresult result = XA_RESULT_SUCCESS;
    CMediaPlayer *avp = (CMediaPlayer *)pPlayItf->mThis;

    switch (avp->mAndroidObjType) {

    case AV_PLR_TS_ABQ: // intended fall-through
    case AV_PLR_URIFD: {
        // FIXME implement for a MediaPlayer playing on URI or FD (on LocAVPlayer, returns -1)
        int dur = -1;
        if (avp->mAVPlayer != 0) {
            avp->mAVPlayer->getDurationMsec(&dur);
        }
        if (dur < 0) {
            *pDurMsec = SL_TIME_UNKNOWN;
        } else {
            *pDurMsec = (XAmillisecond)dur;
        }
    } break;

    default:
        *pDurMsec = XA_TIME_UNKNOWN;
        break;
    }

    return result;
}


//-----------------------------------------------------------------------------
/**
 * pre-condition: avp != NULL, pVolItf != NULL
 */
XAresult android_Player_volumeUpdate(android::GenericPlayer *avp, IVolume *pVolItf)
{
    XAresult result = XA_RESULT_SUCCESS;

    avp->updateVolume((bool)pVolItf->mMute, (bool)pVolItf->mEnableStereoPosition,
            pVolItf->mStereoPosition, pVolItf->mLevel);

    return result;
}

//-----------------------------------------------------------------------------
/**
 * pre-condition: avp != NULL
 */
XAresult android_Player_setPlayState(android::GenericPlayer *avp, SLuint32 playState,
        AndroidObject_state* pObjState)
{
    XAresult result = XA_RESULT_SUCCESS;
    AndroidObject_state objState = *pObjState;

    switch (playState) {
     case SL_PLAYSTATE_STOPPED: {
         SL_LOGV("setting AVPlayer to SL_PLAYSTATE_STOPPED");
         avp->stop();
         }
         break;
     case SL_PLAYSTATE_PAUSED: {
         SL_LOGV("setting AVPlayer to SL_PLAYSTATE_PAUSED");
         switch(objState) {
         case ANDROID_UNINITIALIZED:
             *pObjState = ANDROID_PREPARING;
             avp->prepare();
             break;
         case ANDROID_PREPARING:
             break;
         case ANDROID_READY:
             avp->pause();
             break;
         default:
             SL_LOGE("Android object in invalid state");
             break;
         }
         }
         break;
     case SL_PLAYSTATE_PLAYING: {
         SL_LOGV("setting AVPlayer to SL_PLAYSTATE_PLAYING");
         switch(objState) {
         case ANDROID_UNINITIALIZED:
             *pObjState = ANDROID_PREPARING;
             avp->prepare();
             // intended fall through
         case ANDROID_PREPARING:
             // intended fall through
         case ANDROID_READY:
             avp->play();
             break;
         default:
             SL_LOGE("Android object in invalid state");
             break;
         }
         }
         break;
     default:
         // checked by caller, should not happen
         break;
     }

    return result;
}


//-----------------------------------------------------------------------------
void android_Player_androidBufferQueue_registerCallback_l(CMediaPlayer *mp) {
    if ((mp->mAndroidObjType == AV_PLR_TS_ABQ) && (mp->mAVPlayer != 0)) {
        SL_LOGD("android_Player_androidBufferQueue_registerCallback_l");
        android::StreamPlayer* splr = static_cast<android::StreamPlayer*>(mp->mAVPlayer.get());
        splr->registerQueueCallback(
                (const void*)mp, false /*userIsAudioPlayer*/,
                mp->mAndroidBufferQueue.mContext, (const void*)&(mp->mAndroidBufferQueue.mItf));

    }
}


void android_Player_androidBufferQueue_clear_l(CMediaPlayer *mp) {
    if ((mp->mAndroidObjType == AV_PLR_TS_ABQ) && (mp->mAVPlayer != 0)) {
        android::StreamPlayer* splr = static_cast<android::StreamPlayer*>(mp->mAVPlayer.get());
        splr->appClear_l();
    }
}


void android_Player_androidBufferQueue_onRefilled_l(CMediaPlayer *mp) {
    if ((mp->mAndroidObjType == AV_PLR_TS_ABQ) && (mp->mAVPlayer != 0)) {
        android::StreamPlayer* splr = static_cast<android::StreamPlayer*>(mp->mAVPlayer.get());
        splr->queueRefilled_l();
    }
}



