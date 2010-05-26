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

/* RatePitch implementation */

#include "sles_allinclusive.h"

static SLresult IRatePitch_SetRate(SLRatePitchItf self, SLpermille rate)
{
    IRatePitch *this = (IRatePitch *) self;
    interface_lock_poke(this);
    this->mRate = rate;
    interface_unlock_poke(this);
    return SL_RESULT_SUCCESS;
}

static SLresult IRatePitch_GetRate(SLRatePitchItf self, SLpermille *pRate)
{
    if (NULL == pRate)
        return SL_RESULT_PARAMETER_INVALID;
    IRatePitch *this = (IRatePitch *) self;
    interface_lock_peek(this);
    SLpermille rate = this->mRate;
    interface_unlock_peek(this);
    *pRate = rate;
    return SL_RESULT_SUCCESS;
}

static SLresult IRatePitch_GetRatePitchCapabilities(SLRatePitchItf self,
    SLpermille *pMinRate, SLpermille *pMaxRate)
{
    if (NULL == pMinRate || NULL == pMaxRate)
        return SL_RESULT_PARAMETER_INVALID;
    IRatePitch *this = (IRatePitch *) self;
    // FIXME const, direct access?
    SLpermille minRate = this->mMinRate;
    SLpermille maxRate = this->mMaxRate;
    *pMinRate = minRate;
    *pMaxRate = maxRate;
    return SL_RESULT_SUCCESS;
}

static const struct SLRatePitchItf_ IRatePitch_Itf = {
    IRatePitch_SetRate,
    IRatePitch_GetRate,
    IRatePitch_GetRatePitchCapabilities
};

void IRatePitch_init(void *self)
{
    IRatePitch *this = (IRatePitch *) self;
    this->mItf = &IRatePitch_Itf;
#ifndef NDEBUG
    this->mRate = 0;
#endif
    this->mMinRate = 500;
    this->mMaxRate = 2000;
}
