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

/* Android stream type implementation */

#ifdef ANDROID

#include "sles_allinclusive.h"


static SLresult IAndroidAudioEffect_QueryNumEffects(SLAndroidAudioEffectItf self,
        SLuint32 * pNumSupportedAudioEffects) {

    SL_ENTER_INTERFACE

    result = android_genericFx_queryNumEffects(pNumSupportedAudioEffects);

    SL_LEAVE_INTERFACE
}


static SLresult IAndroidAudioEffect_QueryEffect(SLAndroidAudioEffectItf self,
        SLuint32 index, SLInterfaceID *pAudioEffectId) {

    SL_ENTER_INTERFACE

    result = android_genericFx_queryEffect(index, pAudioEffectId);

    SL_LEAVE_INTERFACE
}


static SLresult IAndroidAudioEffect_CreateEffect(SLAndroidAudioEffectItf self,
        SLInterfaceID audioEffectId, void **ppAudioEffect) {

    SL_ENTER_INTERFACE

    int sessionId = 0;

    IAndroidAudioEffect *this = (IAndroidAudioEffect *) self;
    if (SL_OBJECTID_AUDIOPLAYER == IObjectToObjectID(this->mThis)) {
        CAudioPlayer *ap = (CAudioPlayer *)this->mThis;
        sessionId = ap->mAudioTrack->getSessionId();
    }

    result = android_genericFx_createEffect(sessionId, audioEffectId, ppAudioEffect);

    SL_LEAVE_INTERFACE
}


static SLresult IAndroidAudioEffect_ReleaseEffect(SLAndroidAudioEffectItf self,
        void *pAudioEffect) {

    SL_ENTER_INTERFACE

    result = android_genericFx_releaseEffect(pAudioEffect);

    SL_LEAVE_INTERFACE
}


static SLresult IAndroidAudioEffect_SetEnabled(SLAndroidAudioEffectItf self,
        void *pAudioEffect, SLboolean enabled) {

    SL_ENTER_INTERFACE

    result = android_genericFx_setEnabled(pAudioEffect, enabled);

    SL_LEAVE_INTERFACE
}


static SLresult IAndroidAudioEffect_IsEnabled(SLAndroidAudioEffectItf self,
        void *pAudioEffect, SLboolean * pEnabled) {

    SL_ENTER_INTERFACE

    result = android_genericFx_isEnabled(pAudioEffect, pEnabled);

    SL_LEAVE_INTERFACE
}


static SLresult IAndroidAudioEffect_SendCommand(SLAndroidAudioEffectItf self,
        void *pAudioEffect, SLuint32 command, SLuint32 commandSize, void* pCommand,
        SLuint32 *replySize, void *pReply) {

    SL_ENTER_INTERFACE

    result = android_genericFx_sendCommand(pAudioEffect, command, commandSize, pCommand, replySize,
            pReply);

    SL_LEAVE_INTERFACE
}


static const struct SLAndroidAudioEffectItf_ IAndroidAudioEffect_Itf = {
        IAndroidAudioEffect_QueryNumEffects,
        IAndroidAudioEffect_QueryEffect,
        IAndroidAudioEffect_CreateEffect,
        IAndroidAudioEffect_ReleaseEffect,
        IAndroidAudioEffect_SetEnabled,
        IAndroidAudioEffect_IsEnabled,
        IAndroidAudioEffect_SendCommand
};

void IAndroidAudioEffect_init(void *self)
{
    IAndroidAudioEffect *this = (IAndroidAudioEffect *) self;
    this->mItf = &IAndroidAudioEffect_Itf;

}

#endif // #ifdef ANDROID
