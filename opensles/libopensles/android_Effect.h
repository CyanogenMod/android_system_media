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


extern void android_eq_init(int sessionId, CAudioPlayer* ap);

extern android::status_t android_eq_setParam(android::sp<android::AudioEffect> pFx,
        int32_t param, int32_t param2, void *pValue);

extern android::status_t android_eq_getParam(android::sp<android::AudioEffect> pFx,
        int32_t param, int32_t param2, void *pValue);

extern void android_bb_init(int sessionId, CAudioPlayer* ap);

extern android::status_t android_bb_setParam(android::sp<android::AudioEffect> pFx,
        int32_t param, void *pValue);

extern android::status_t android_bb_getParam(android::sp<android::AudioEffect> pFx,
        int32_t param, void *pValue);

extern SLresult android_fx_statusToResult(android::status_t status);

extern bool android_fx_initEffectDescriptor(const SLInterfaceID effectId,
        effect_descriptor_t* fxDescrLoc);


extern SLresult android_genericFx_queryNumEffects(SLuint32 *pNumSupportedAudioEffects);

extern SLresult android_genericFx_queryEffect(SLuint32 index, SLInterfaceID *pAudioEffectId);

extern SLresult android_genericFx_createEffect(int sessionId, SLInterfaceID pUuid,
        void **ppAudioEffect);

extern SLresult android_genericFx_releaseEffect(void *pAudioEffect);

extern SLresult android_genericFx_setEnabled(void *pAudioEffect, SLboolean enabled);

extern SLresult android_genericFx_isEnabled(void *pAudioEffect, SLboolean *pEnabled);

extern SLresult android_genericFx_sendCommand(void *pAudioEffect, SLuint32 command,
        SLuint32 commandSize, void* pCommand, SLuint32 *replySize, void *pReply);

