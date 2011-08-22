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
// LocAVPlayer and StreamPlayer derive from GenericMediaPlayer,
//    so no need to #include "android_GenericMediaPlayer.h"
#include "android_LocAVPlayer.h"
#include "android_StreamPlayer.h"


//-----------------------------------------------------------------------------
static void player_handleMediaPlayerEventNotifications(int event, int data1, int data2, void* user)
{
    if (NULL == user) {
        return;
    }

    CMediaPlayer* mp = (CMediaPlayer*) user;
    SL_LOGV("received event %d, data %d from AVPlayer", event, data1);

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

        // enqueue notification (outside of lock) that the stream information has been updated
        if ((NULL != callback) && (index >= 0)) {
#ifdef XA_SYNCHRONOUS_STREAMCBEVENT_PROPERTYCHANGE
            (*callback)(&mp->mStreamInfo.mItf, XA_STREAMCBEVENT_PROPERTYCHANGE /*eventId*/,
                    1 /*streamIndex, only one stream supported here, 0 is reserved*/,
                    NULL /*pEventData, always NULL in OpenMAX AL 1.0.1*/,
                    callbackPContext /*pContext*/);
#else
            SLresult res = EnqueueAsyncCallback_piipp(mp, callback,
                    /*p1*/ &mp->mStreamInfo.mItf,
                    /*i1*/ XA_STREAMCBEVENT_PROPERTYCHANGE /*eventId*/,
                    /*i2*/ 1 /*streamIndex, only one stream supported here, 0 is reserved*/,
                    /*p2*/ NULL /*pEventData, always NULL in OpenMAX AL 1.0.1*/,
                    /*p3*/ callbackPContext /*pContext*/);
#endif
        }
        break;
      }

      case android::GenericPlayer::kEventEndOfStream: {
        SL_LOGV("Received AVPlayer::kEventEndOfStream for CMediaPlayer %p", mp);

        object_lock_exclusive(&mp->mObject);
        // should be xaPlayCallback but we're sharing the itf between SL and AL
        slPlayCallback playCallback = NULL;
        void * playContext = NULL;
        // XAPlayItf callback or no callback?
        if (mp->mPlay.mEventFlags & XA_PLAYEVENT_HEADATEND) {
            playCallback = mp->mPlay.mCallback;
            playContext = mp->mPlay.mContext;
        }
        object_unlock_exclusive(&mp->mObject);

        // enqueue callback with no lock held
        if (NULL != playCallback) {
#ifdef XA_SYNCHRONOUS_PLAYEVENT_HEADATEND
            (*playCallback)(&mp->mPlay.mItf, playContext, XA_PLAYEVENT_HEADATEND);
#else
            SLresult res = EnqueueAsyncCallback_ppi(mp, playCallback, &mp->mPlay.mItf, playContext,
                    XA_PLAYEVENT_HEADATEND);
            LOGW_IF(SL_RESULT_SUCCESS != res,
                    "Callback %p(%p, %p, XA_PLAYEVENT_HEADATEND) dropped", playCallback,
                    &mp->mPlay.mItf, playContext);
#endif
        }
        break;
      }

      case android::GenericPlayer::kEventChannelCount: {
        SL_LOGV("kEventChannelCount channels = %d", data1);
        object_lock_exclusive(&mp->mObject);
        if (UNKNOWN_NUMCHANNELS == mp->mNumChannels && UNKNOWN_NUMCHANNELS != data1) {
            mp->mNumChannels = data1;
            android_Player_volumeUpdate(mp);
        }
        object_unlock_exclusive(&mp->mObject);
      }
      break;

      case android::GenericPlayer::kEventPrefetchFillLevelUpdate: {
        SL_LOGV("kEventPrefetchFillLevelUpdate");
      }
      break;

      case android::GenericPlayer::kEventPrefetchStatusChange: {
        SL_LOGV("kEventPrefetchStatusChange");
      }
      break;


      default: {
        SL_LOGE("Received unknown event %d, data %d from AVPlayer", event, data1);
      }
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
        mp->mAndroidObjType = AUDIOVIDEOPLAYER_FROM_TS_ANDROIDBUFFERQUEUE;
        break;
    case XA_DATALOCATOR_URI: // intended fall-through
    case SL_DATALOCATOR_ANDROIDFD:
        mp->mAndroidObjType = AUDIOVIDEOPLAYER_FROM_URIFD;
        break;
    case XA_DATALOCATOR_ADDRESS: // intended fall-through
    default:
        SL_LOGE("Unable to create MediaPlayer for data source locator 0x%x", sourceLocator);
        result = XA_RESULT_PARAMETER_INVALID;
        break;
    }

    // FIXME duplicates an initialization also done by higher level
    mp->mAndroidObjState = ANDROID_UNINITIALIZED;
    mp->mStreamType = ANDROID_DEFAULT_OUTPUT_STREAM_TYPE;
    mp->mSessionId = android::AudioSystem::newAudioSessionId();

    return result;
}


//-----------------------------------------------------------------------------
// FIXME abstract out the diff between CMediaPlayer and CAudioPlayer
XAresult android_Player_realize(CMediaPlayer *mp, SLboolean async) {
    SL_LOGV("android_Player_realize_l(%p)", mp);
    XAresult result = XA_RESULT_SUCCESS;

    const SLDataSource *pDataSrc = &mp->mDataSource.u.mSource;
    const SLuint32 sourceLocator = *(SLuint32 *)pDataSrc->pLocator;

    AudioPlayback_Parameters ap_params;
    ap_params.sessionId = mp->mSessionId;
    ap_params.streamType = mp->mStreamType;
    ap_params.trackcb = NULL;
    ap_params.trackcbUser = NULL;

    switch(mp->mAndroidObjType) {
    case AUDIOVIDEOPLAYER_FROM_TS_ANDROIDBUFFERQUEUE: {
        mp->mAVPlayer = new android::StreamPlayer(&ap_params, true /*hasVideo*/);
        mp->mAVPlayer->init(player_handleMediaPlayerEventNotifications, (void*)mp);
        }
        break;
    case AUDIOVIDEOPLAYER_FROM_URIFD: {
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
            SL_LOGE("Invalid or unsupported data locator type %u for data source",
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

    if (XA_RESULT_SUCCESS == result) {

        // if there is a video sink
        if (XA_DATALOCATOR_NATIVEDISPLAY ==
                mp->mImageVideoSink.mLocator.mLocatorType) {
            ANativeWindow *nativeWindow = (ANativeWindow *)
                    mp->mImageVideoSink.mLocator.mNativeDisplay.hWindow;
            // we already verified earlier that hWindow is non-NULL
            assert(nativeWindow != NULL);
            result = android_Player_setNativeWindow(mp, nativeWindow);
        }

    }

    return result;
}

//-----------------------------------------------------------------------------
XAresult android_Player_destroy(CMediaPlayer *mp) {
    SL_LOGV("android_Player_destroy(%p)", mp);
    XAresult result = XA_RESULT_SUCCESS;

    if (mp->mAVPlayer != 0) {
        mp->mAVPlayer.clear();
    }

    return result;
}


XAresult android_Player_getDuration(IPlay *pPlayItf, XAmillisecond *pDurMsec) {
    CMediaPlayer *avp = (CMediaPlayer *)pPlayItf->mThis;

    switch (avp->mAndroidObjType) {

    case AUDIOVIDEOPLAYER_FROM_URIFD: {
        int dur = ANDROID_UNKNOWN_TIME;
        if (avp->mAVPlayer != 0) {
            avp->mAVPlayer->getDurationMsec(&dur);
        }
        if (dur == ANDROID_UNKNOWN_TIME) {
            *pDurMsec = XA_TIME_UNKNOWN;
        } else {
            *pDurMsec = (XAmillisecond)dur;
        }
    } break;

    case AUDIOVIDEOPLAYER_FROM_TS_ANDROIDBUFFERQUEUE: // intended fall-through
    default:
        *pDurMsec = XA_TIME_UNKNOWN;
        break;
    }

    return XA_RESULT_SUCCESS;
}


XAresult android_Player_getPosition(IPlay *pPlayItf, XAmillisecond *pPosMsec) {
    SL_LOGD("android_Player_getPosition()");
    XAresult result = XA_RESULT_SUCCESS;
    CMediaPlayer *avp = (CMediaPlayer *)pPlayItf->mThis;

    switch (avp->mAndroidObjType) {

    case AUDIOVIDEOPLAYER_FROM_TS_ANDROIDBUFFERQUEUE: // intended fall-through
    case AUDIOVIDEOPLAYER_FROM_URIFD: {
        int pos = -1;
        if (avp->mAVPlayer != 0) {
            avp->mAVPlayer->getPositionMsec(&pos);
        }
        if (pos == ANDROID_UNKNOWN_TIME) {
            *pPosMsec = XA_TIME_UNKNOWN;
        } else {
            *pPosMsec = (XAmillisecond)pos;
        }
    } break;

    default:
        // we shouldn't be here
        assert(false);
        break;
    }

    return result;
}


//-----------------------------------------------------------------------------
/**
 * pre-condition: mp != NULL
 */
void android_Player_volumeUpdate(CMediaPlayer* mp)
{
    android::GenericPlayer* avp = mp->mAVPlayer.get();
    if (avp != NULL) {
        float volumes[2];
        // MediaPlayer does not currently support EffectSend or MuteSolo
        android_player_volumeUpdate(volumes, &mp->mVolume, mp->mNumChannels, 1.0f, NULL);
        float leftVol = volumes[0], rightVol = volumes[1];
        avp->setVolume(leftVol, rightVol);
    }
}

//-----------------------------------------------------------------------------
/**
 * pre-condition: gp != 0
 */
XAresult android_Player_setPlayState(const android::sp<android::GenericPlayer> &gp,
        SLuint32 playState,
        AndroidObjectState* pObjState)
{
    XAresult result = XA_RESULT_SUCCESS;
    AndroidObjectState objState = *pObjState;

    switch (playState) {
     case SL_PLAYSTATE_STOPPED: {
         SL_LOGV("setting AVPlayer to SL_PLAYSTATE_STOPPED");
         gp->stop();
         }
         break;
     case SL_PLAYSTATE_PAUSED: {
         SL_LOGV("setting AVPlayer to SL_PLAYSTATE_PAUSED");
         switch(objState) {
         case ANDROID_UNINITIALIZED:
             *pObjState = ANDROID_PREPARING;
             gp->prepare();
             break;
         case ANDROID_PREPARING:
             break;
         case ANDROID_READY:
             gp->pause();
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
             gp->prepare();
             // intended fall through
         case ANDROID_PREPARING:
             // intended fall through
         case ANDROID_READY:
             gp->play();
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


/**
 * pre-condition: mp != NULL
 */
XAresult android_Player_seek(CMediaPlayer *mp, SLmillisecond posMsec) {
    XAresult result = XA_RESULT_SUCCESS;
    switch (mp->mAndroidObjType) {
      case AUDIOVIDEOPLAYER_FROM_URIFD:
        if (mp->mAVPlayer !=0) {
            mp->mAVPlayer->seek(posMsec);
        }
        break;
      case AUDIOVIDEOPLAYER_FROM_TS_ANDROIDBUFFERQUEUE: // intended fall-through
      default: {
          result = XA_RESULT_PARAMETER_INVALID;
      }
    }
    return result;
}


/**
 * pre-condition: mp != NULL
 */
XAresult android_Player_loop(CMediaPlayer *mp, SLboolean loopEnable) {
    XAresult result = XA_RESULT_SUCCESS;
    switch (mp->mAndroidObjType) {
      case AUDIOVIDEOPLAYER_FROM_URIFD:
        if (mp->mAVPlayer !=0) {
            mp->mAVPlayer->loop(loopEnable);
        }
        break;
      case AUDIOVIDEOPLAYER_FROM_TS_ANDROIDBUFFERQUEUE: // intended fall-through
      default: {
          result = XA_RESULT_PARAMETER_INVALID;
      }
    }
    return result;
}


//-----------------------------------------------------------------------------
void android_Player_androidBufferQueue_registerCallback_l(CMediaPlayer *mp) {
    if ((mp->mAndroidObjType == AUDIOVIDEOPLAYER_FROM_TS_ANDROIDBUFFERQUEUE)
            && (mp->mAVPlayer != 0)) {
        SL_LOGD("android_Player_androidBufferQueue_registerCallback_l");
        android::StreamPlayer* splr = static_cast<android::StreamPlayer*>(mp->mAVPlayer.get());
        splr->registerQueueCallback(
                (const void*)mp, false /*userIsAudioPlayer*/,
                mp->mAndroidBufferQueue.mContext, (const void*)&(mp->mAndroidBufferQueue.mItf));

    }
}


void android_Player_androidBufferQueue_clear_l(CMediaPlayer *mp) {
    if ((mp->mAndroidObjType == AUDIOVIDEOPLAYER_FROM_TS_ANDROIDBUFFERQUEUE)
            && (mp->mAVPlayer != 0)) {
        android::StreamPlayer* splr = static_cast<android::StreamPlayer*>(mp->mAVPlayer.get());
        splr->appClear_l();
    }
}


void android_Player_androidBufferQueue_onRefilled_l(CMediaPlayer *mp) {
    if ((mp->mAndroidObjType == AUDIOVIDEOPLAYER_FROM_TS_ANDROIDBUFFERQUEUE)
            && (mp->mAVPlayer != 0)) {
        android::StreamPlayer* splr = static_cast<android::StreamPlayer*>(mp->mAVPlayer.get());
        splr->queueRefilled_l();
    }
}


/*
 *  pre-conditions:
 *      mp != NULL
 *      mp->mAVPlayer != 0 (player is realized)
 *      nativeWindow can be NULL, but if NULL it is treated as an error
 */
SLresult android_Player_setNativeWindow(CMediaPlayer *mp, ANativeWindow *nativeWindow)
{
    assert(mp != NULL);
    assert(mp->mAVPlayer != 0);
    if (nativeWindow == NULL) {
        SL_LOGE("ANativeWindow is NULL");
        return SL_RESULT_PARAMETER_INVALID;
    }
    SLresult result;
    int err;
    int value;
    // this could crash if app passes in a bad parameter, but that's OK
    err = (*nativeWindow->query)(nativeWindow, NATIVE_WINDOW_CONCRETE_TYPE, &value);
    if (0 != err) {
        SL_LOGE("Query NATIVE_WINDOW_CONCRETE_TYPE on ANativeWindow * %p failed; "
                "errno %d", nativeWindow, err);
        result = SL_RESULT_PARAMETER_INVALID;
    } else {
        switch (value) {
        case NATIVE_WINDOW_SURFACE: {                // Surface
            SL_LOGV("Displaying on ANativeWindow of type NATIVE_WINDOW_SURFACE");
            android::sp<android::Surface> nativeSurface(
                    static_cast<android::Surface *>(nativeWindow));
            mp->mAVPlayer->setVideoSurface(nativeSurface);
            result = SL_RESULT_SUCCESS;
            } break;
        case NATIVE_WINDOW_SURFACE_TEXTURE_CLIENT: { // SurfaceTextureClient
            SL_LOGV("Displaying on ANativeWindow of type NATIVE_WINDOW_SURFACE_TEXTURE_CLIENT");
            android::sp<android::SurfaceTextureClient> surfaceTextureClient(
                    static_cast<android::SurfaceTextureClient *>(nativeWindow));
            android::sp<android::ISurfaceTexture> nativeSurfaceTexture(
                    surfaceTextureClient->getISurfaceTexture());
            mp->mAVPlayer->setVideoSurfaceTexture(nativeSurfaceTexture);
            result = SL_RESULT_SUCCESS;
            } break;
        case NATIVE_WINDOW_FRAMEBUFFER:              // FramebufferNativeWindow
            // fall through
        default:
            SL_LOGE("ANativeWindow * %p has unknown or unsupported concrete type %d",
                    nativeWindow, value);
            result = SL_RESULT_PARAMETER_INVALID;
            break;
        }
    }
    return result;
}
