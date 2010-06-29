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

#ifndef OPENSL_ES_ANDROID_H_
#define OPENSL_ES_ANDROID_H_

#ifdef __cplusplus
extern "C" {

/*---------------------------------------------------------------------------*/
/* Android Stream Type interface                                             */
/*---------------------------------------------------------------------------*/

/** Stream types */
// FIXME verify those are all the ones we need
// FIXME verify we want to use the same values as in android::AudioSystem
/*      same as android.media.AudioManager.STREAM_VOICE_CALL */
#define SL_ANDROID_STREAM_VOICE        ((SLuint32) 0x00000000)
/*      same as android.media.AudioManager.STREAM_SYSTEM */
#define SL_ANDROID_STREAM_SYSTEM       ((SLuint32) 0x00000001)
/*      same as android.media.AudioManager.STREAM_RING */
#define SL_ANDROID_STREAM_RING         ((SLuint32) 0x00000002)
/*      same as android.media.AudioManager.STREAM_MUSIC */
#define SL_ANDROID_STREAM_MEDIA        ((SLuint32) 0x00000003)
/*      same as android.media.AudioManager.STREAM_ALARM */
#define SL_ANDROID_STREAM_ALARM        ((SLuint32) 0x00000004)
/*      same as android.media.AudioManager.STREAM_NOTIFICATION */
#define SL_ANDROID_STREAM_NOTIFICATION ((SLuint32) 0x00000005)


extern SLAPIENTRY const SLInterfaceID SL_IID_ANDROIDSTREAMTYPE;

/** Android Stream Type interface methods */

struct SLAndroidStreamTypeItf_;
typedef const struct SLAndroidStreamTypeItf_ * const * SLAndroidStreamTypeItf;


struct SLAndroidStreamTypeItf_ {
    SLresult (*SetStreamType) (
        SLAndroidStreamTypeItf self,
        SLuint32 type
    );
    SLresult (*GetStreamType) (
        SLAndroidStreamTypeItf self,
        SLuint32 *pType
    );
};

}
#endif /* __cplusplus */

#endif /* OPENSL_ES_ANDROID_H_ */
