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

#include <surfaceflinger/Surface.h>
#include <gui/ISurfaceTexture.h>


/**************************************************************************************************
 * Player lifecycle
 ****************************/
extern XAresult android_Player_checkSourceSink(CMediaPlayer *mp);

extern XAresult android_Player_create(CMediaPlayer *mp);

extern XAresult android_Player_realize(CMediaPlayer *mp, SLboolean async);

extern XAresult android_Player_destroy(CMediaPlayer *mp);


/**************************************************************************************************
 * Configuration
 ****************************/
/**
 * pre-conditions: gp != 0, surface != 0
 */
extern XAresult android_Player_setVideoSurface(
        const android::sp<android::GenericPlayer> &gp,
        const android::sp<android::Surface> &surface);

/**
 * pre-conditions: gp != 0, surfaceTexture != 0
 */
extern XAresult android_Player_setVideoSurfaceTexture(
        const android::sp<android::GenericPlayer> &gp,
        const android::sp<android::ISurfaceTexture> &surfaceTexture);

extern XAresult android_Player_getDuration(IPlay *pPlayItf, SLmillisecond *pDurMsec);

/**
 * pre-condition: avp != NULL, pVolItf != NULL
 */
extern XAresult android_Player_volumeUpdate(android::GenericPlayer *avp, IVolume *pVolItf);

/**************************************************************************************************
 * Playback control and events
 ****************************/
/**
 * pre-condition: avp != NULL
 */
extern XAresult android_Player_setPlayState(android::GenericPlayer *avp, SLuint32 playState,
        AndroidObject_state* pObjState);



/**************************************************************************************************
 * Buffer Queue events
 ****************************/

/**************************************************************************************************
 * Android Buffer Queue
 ****************************/

/* must be called with a lock on mp->mThis */
extern void android_Player_androidBufferQueue_registerCallback_l(CMediaPlayer *mp);
/* must be called with a lock on mp->mThis */
extern void android_Player_androidBufferQueue_clear_l(CMediaPlayer *mp);
/* must be called with a lock on mp->mThis */
extern void android_Player_androidBufferQueue_onRefilled_l(CMediaPlayer *mp);
