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

/* EffectSend implementation */

#include "sles_allinclusive.h"

static struct EnableLevel *getEnableLevel(IEffectSend *this,
    const void *pAuxEffect)
{
    COutputMix *outputMix = this->mOutputMix;
    // Make sure the sink for this player is an output mix
#if 0 // not necessary
    if (NULL == outputMix)
        return NULL;
#endif
    if (pAuxEffect == &outputMix->mEnvironmentalReverb)
        return &this->mEnableLevels[AUX_ENVIRONMENTALREVERB];
    if (pAuxEffect == &outputMix->mPresetReverb)
        return &this->mEnableLevels[AUX_PRESETREVERB];
    return NULL;
#if 0 // App couldn't have an interface for effect without exposure
    unsigned interfaceMask = 1 << MPH_to_OutputMix[AUX_to_MPH[aux]];
    if (outputMix->mExposedMask & interfaceMask)
        result = SL_RESULT_PARAMETER_INVALID;
#endif
}

static SLresult IEffectSend_EnableEffectSend(SLEffectSendItf self,
    const void *pAuxEffect, SLboolean enable, SLmillibel initialLevel)
{
    IEffectSend *this = (IEffectSend *) self;
    struct EnableLevel *enableLevel = getEnableLevel(this, pAuxEffect);
    if (NULL == enableLevel)
        return SL_RESULT_PARAMETER_INVALID;
    interface_lock_exclusive(this);
    enableLevel->mEnable = enable;
    enableLevel->mSendLevel = initialLevel;
    interface_unlock_exclusive(this);
    return SL_RESULT_SUCCESS;
}

static SLresult IEffectSend_IsEnabled(SLEffectSendItf self,
    const void *pAuxEffect, SLboolean *pEnable)
{
    if (NULL == pEnable)
        return SL_RESULT_PARAMETER_INVALID;
    IEffectSend *this = (IEffectSend *) self;
    struct EnableLevel *enableLevel = getEnableLevel(this, pAuxEffect);
    if (NULL == enableLevel)
        return SL_RESULT_PARAMETER_INVALID;
    interface_lock_peek(this);
    SLboolean enable = enableLevel->mEnable;
    interface_unlock_peek(this);
    *pEnable = enable;
    return SL_RESULT_SUCCESS;
}

static SLresult IEffectSend_SetDirectLevel(SLEffectSendItf self,
    SLmillibel directLevel)
{
    IEffectSend *this = (IEffectSend *) self;
    interface_lock_poke(this);
    this->mDirectLevel = directLevel;
    interface_unlock_poke(this);
    return SL_RESULT_SUCCESS;
}

static SLresult IEffectSend_GetDirectLevel(SLEffectSendItf self,
    SLmillibel *pDirectLevel)
{
    if (NULL == pDirectLevel)
        return SL_RESULT_PARAMETER_INVALID;
    IEffectSend *this = (IEffectSend *) self;
    interface_lock_peek(this);
    SLmillibel directLevel = this->mDirectLevel;
    interface_unlock_peek(this);
    *pDirectLevel = directLevel;
    return SL_RESULT_SUCCESS;
}

static SLresult IEffectSend_SetSendLevel(SLEffectSendItf self,
    const void *pAuxEffect, SLmillibel sendLevel)
{
    IEffectSend *this = (IEffectSend *) self;
    struct EnableLevel *enableLevel = getEnableLevel(this, pAuxEffect);
    if (NULL == enableLevel)
        return SL_RESULT_PARAMETER_INVALID;
    // EnableEffectSend is exclusive, so this has to be also
    interface_lock_exclusive(this);
    enableLevel->mSendLevel = sendLevel;
    interface_unlock_exclusive(this);
    return SL_RESULT_SUCCESS;
}

static SLresult IEffectSend_GetSendLevel(SLEffectSendItf self,
    const void *pAuxEffect, SLmillibel *pSendLevel)
{
    if (NULL == pSendLevel)
        return SL_RESULT_PARAMETER_INVALID;
    IEffectSend *this = (IEffectSend *) self;
    struct EnableLevel *enableLevel = getEnableLevel(this, pAuxEffect);
    if (NULL == enableLevel)
        return SL_RESULT_PARAMETER_INVALID;
    interface_lock_peek(this);
    SLmillibel sendLevel = enableLevel->mSendLevel;
    interface_unlock_peek(this);
    *pSendLevel = sendLevel;
    return SL_RESULT_SUCCESS;
}

static const struct SLEffectSendItf_ IEffectSend_Itf = {
    IEffectSend_EnableEffectSend,
    IEffectSend_IsEnabled,
    IEffectSend_SetDirectLevel,
    IEffectSend_GetDirectLevel,
    IEffectSend_SetSendLevel,
    IEffectSend_GetSendLevel
};

void IEffectSend_init(void *self)
{
    IEffectSend *this = (IEffectSend *) self;
    this->mItf = &IEffectSend_Itf;
#ifndef NDEBUG
    this->mOutputMix = NULL; // FIXME wrong
    this->mDirectLevel = 0;
    // FIXME hard-coded array initialization should be loop on AUX_MAX
    this->mEnableLevels[AUX_ENVIRONMENTALREVERB].mEnable = SL_BOOLEAN_FALSE;
    this->mEnableLevels[AUX_ENVIRONMENTALREVERB].mSendLevel = 0;
    this->mEnableLevels[AUX_PRESETREVERB].mEnable = SL_BOOLEAN_FALSE;
    this->mEnableLevels[AUX_PRESETREVERB].mSendLevel = 0;
#endif
}
