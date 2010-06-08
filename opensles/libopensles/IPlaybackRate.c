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

/* PlaybackRate implementation */

#include "sles_allinclusive.h"

static SLresult IPlaybackRate_SetRate(SLPlaybackRateItf self, SLpermille rate)
{
    IPlaybackRate *this = (IPlaybackRate *) self;
    interface_lock_poke(this);
    this->mRate = rate;
    interface_unlock_poke(this);
#ifdef USE_ANDROID
    return sles_to_android_audioPlayerSetPlayRate(this, rate);
#endif
    return SL_RESULT_SUCCESS;
}

static SLresult IPlaybackRate_GetRate(SLPlaybackRateItf self, SLpermille *pRate)
{
    if (NULL == pRate) {
        return SL_RESULT_PARAMETER_INVALID;
    }
    IPlaybackRate *this = (IPlaybackRate *) self;
    interface_lock_peek(this);
    SLpermille rate = this->mRate;
    interface_unlock_peek(this);
    *pRate = rate;
    return SL_RESULT_SUCCESS;
}

static SLresult IPlaybackRate_SetPropertyConstraints(SLPlaybackRateItf self,
    SLuint32 constraints)
{
    IPlaybackRate *this = (IPlaybackRate *) self;
    SLresult result = SL_RESULT_SUCCESS;
    this->mProperties = constraints;
#ifdef USE_ANDROID
    // verify property support before storing
    result = sles_to_android_audioPlayerSetPlaybackRateBehavior(this, constraints);
#endif
    interface_lock_poke(this);
    if (result == SL_RESULT_SUCCESS) {
        this->mProperties = constraints;
    }
    interface_unlock_poke(this);
    return result;
}

static SLresult IPlaybackRate_GetProperties(SLPlaybackRateItf self,
    SLuint32 *pProperties)
{
    if (NULL == pProperties) {
        return SL_RESULT_PARAMETER_INVALID;
    }
    IPlaybackRate *this = (IPlaybackRate *) self;
    interface_lock_peek(this);
    SLuint32 properties = this->mProperties;
    interface_unlock_peek(this);
    *pProperties = properties;
    return SL_RESULT_SUCCESS;
}

static SLresult IPlaybackRate_GetCapabilitiesOfRate(SLPlaybackRateItf self,
    SLpermille rate, SLuint32 *pCapabilities)
{
    if (NULL == pCapabilities) {
        return SL_RESULT_PARAMETER_INVALID;
    }
    IPlaybackRate *this = (IPlaybackRate *) self;
    SLuint32 capabilities = 0;
#ifdef USE_ANDROID
    sles_to_android_audioPlayerGetCapabilitiesOfRate(this, &capabilities);
#endif
    *pCapabilities = capabilities;
    return SL_RESULT_SUCCESS;
}

static SLresult IPlaybackRate_GetRateRange(SLPlaybackRateItf self, SLuint8 index,
    SLpermille *pMinRate, SLpermille *pMaxRate, SLpermille *pStepSize,
    SLuint32 *pCapabilities)
{
    if (NULL == pMinRate || NULL == pMaxRate || NULL == pStepSize || NULL == pCapabilities)
        return SL_RESULT_PARAMETER_INVALID;
    IPlaybackRate *this = (IPlaybackRate *) self;
    interface_lock_shared(this);
    SLpermille minRate = this->mMinRate;
    SLpermille maxRate = this->mMaxRate;
    SLpermille stepSize = this->mStepSize;
    SLuint32 capabilities = this->mCapabilities;
    interface_unlock_shared(this);
    *pMinRate = minRate;
    *pMaxRate = maxRate;
    *pStepSize = stepSize;
    *pCapabilities = capabilities;
    return SL_RESULT_SUCCESS;
}

static const struct SLPlaybackRateItf_ IPlaybackRate_Itf = {
    IPlaybackRate_SetRate,
    IPlaybackRate_GetRate,
    IPlaybackRate_SetPropertyConstraints,
    IPlaybackRate_GetProperties,
    IPlaybackRate_GetCapabilitiesOfRate,
    IPlaybackRate_GetRateRange
};

void IPlaybackRate_init(void *self)
{
    IPlaybackRate *this = (IPlaybackRate *) self;
    this->mItf = &IPlaybackRate_Itf;
    this->mProperties = 0;
    this->mRate = 1000;
    this->mMinRate = 500;
    this->mMaxRate = 2000;
    this->mStepSize = 100;
#ifdef USE_ANDROID
    this->mStepSize = 0;
    // for an AudioPlayer, mCapabilities will be initialized in sles_to_android_audioPlayerCreate
#endif
    this->mCapabilities = 0;
}
