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

/* Vibra implementation */

#include "sles_allinclusive.h"

static SLresult IVibra_Vibrate(SLVibraItf self, SLboolean vibrate)
{
    IVibra *this = (IVibra *) self;
    interface_lock_poke(this);
    this->mVibrate = SL_BOOLEAN_FALSE != vibrate; // normalize
    interface_unlock_poke(this);
    return SL_RESULT_SUCCESS;
}

static SLresult IVibra_IsVibrating(SLVibraItf self, SLboolean *pVibrating)
{
    if (NULL == pVibrating)
        return SL_RESULT_PARAMETER_INVALID;
    IVibra *this = (IVibra *) self;
    interface_lock_peek(this);
    SLboolean vibrate = this->mVibrate;
    interface_unlock_peek(this);
    *pVibrating = vibrate;
    return SL_RESULT_SUCCESS;
}

static SLresult IVibra_SetFrequency(SLVibraItf self, SLmilliHertz frequency)
{
    const SLVibraDescriptor *d = Vibra_id_descriptors[0].descriptor;
    if (!d->supportsFrequency)
        return SL_RESULT_PRECONDITIONS_VIOLATED;
    if (!(d->minFrequency <= frequency && frequency <= d->maxFrequency))
        return SL_RESULT_PARAMETER_INVALID;
    IVibra *this = (IVibra *) self;
    interface_lock_poke(this);
    this->mFrequency = frequency;
    interface_unlock_exclusive(this);
    return SL_RESULT_SUCCESS;
}

static SLresult IVibra_GetFrequency(SLVibraItf self, SLmilliHertz *pFrequency)
{
    if (NULL == pFrequency)
        return SL_RESULT_PARAMETER_INVALID;
    IVibra *this = (IVibra *) self;
    interface_lock_peek(this);
    SLmilliHertz frequency = this->mFrequency;
    interface_unlock_peek(this);
    *pFrequency = frequency;
    return SL_RESULT_SUCCESS;
}

static SLresult IVibra_SetIntensity(SLVibraItf self, SLpermille intensity)
{
    const SLVibraDescriptor *d = Vibra_id_descriptors[0].descriptor;
    if (!d->supportsIntensity)
        return SL_RESULT_PRECONDITIONS_VIOLATED;
    if (!(0 <= intensity && intensity <= 1000))
        return SL_RESULT_PARAMETER_INVALID;
    IVibra *this = (IVibra *) self;
    interface_lock_poke(this);
    this->mIntensity = intensity;
    interface_unlock_poke(this);
    return SL_RESULT_SUCCESS;
}

static SLresult IVibra_GetIntensity(SLVibraItf self, SLpermille *pIntensity)
{
    if (NULL == pIntensity)
        return SL_RESULT_PARAMETER_INVALID;
    const SLVibraDescriptor *d = Vibra_id_descriptors[0].descriptor;
    if (!d->supportsIntensity)
        return SL_RESULT_PRECONDITIONS_VIOLATED;
    IVibra *this = (IVibra *) self;
    interface_lock_peek(this);
    SLpermille intensity = this->mIntensity;
    interface_unlock_peek(this);
    *pIntensity = intensity;
    return SL_RESULT_SUCCESS;
}

static const struct SLVibraItf_ IVibra_Itf = {
    IVibra_Vibrate,
    IVibra_IsVibrating,
    IVibra_SetFrequency,
    IVibra_GetFrequency,
    IVibra_SetIntensity,
    IVibra_GetIntensity
};

void IVibra_init(void *self)
{
    IVibra *this = (IVibra *) self;
    this->mItf = &IVibra_Itf;
    this->mVibrate = SL_BOOLEAN_FALSE;
    // next 2 values are undefined per spec
    this->mFrequency = Vibra_id_descriptors[0].descriptor->minFrequency;
    this->mIntensity = 1000;
}
