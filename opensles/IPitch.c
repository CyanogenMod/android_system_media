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

/* Pitch implementation */

#include "sles_allinclusive.h"

static SLresult IPitch_SetPitch(SLPitchItf self, SLpermille pitch)
{
    IPitch *this = (IPitch *) self;
    interface_lock_poke(this);
    this->mPitch = pitch;
    interface_unlock_poke(this);
    return SL_RESULT_SUCCESS;
}

static SLresult IPitch_GetPitch(SLPitchItf self, SLpermille *pPitch)
{
    if (NULL == pPitch)
        return SL_RESULT_PARAMETER_INVALID;
    IPitch *this = (IPitch *) self;
    interface_lock_peek(this);
    SLpermille pitch = this->mPitch;
    interface_unlock_peek(this);
    *pPitch = pitch;
    return SL_RESULT_SUCCESS;
}

static SLresult IPitch_GetPitchCapabilities(SLPitchItf self,
    SLpermille *pMinPitch, SLpermille *pMaxPitch)
{
    if (NULL == pMinPitch || NULL == pMaxPitch)
        return SL_RESULT_PARAMETER_INVALID;
    IPitch *this = (IPitch *) self;
    SLpermille minPitch = this->mMinPitch;
    SLpermille maxPitch = this->mMaxPitch;
    *pMinPitch = minPitch;
    *pMaxPitch = maxPitch;
    return SL_RESULT_SUCCESS;
}

static const struct SLPitchItf_ IPitch_Itf = {
    IPitch_SetPitch,
    IPitch_GetPitch,
    IPitch_GetPitchCapabilities
};

void IPitch_init(void *self)
{
    IPitch *this = (IPitch *) self;
    this->mItf = &IPitch_Itf;
}
