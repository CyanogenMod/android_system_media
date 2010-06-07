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

/* Virtualizer implementation */

#include "sles_allinclusive.h"

static SLresult IVirtualizer_SetEnabled(SLVirtualizerItf self, SLboolean enabled)
{
    IVirtualizer *this = (IVirtualizer *) self;
    interface_lock_poke(this);
    this->mEnabled = SL_BOOLEAN_FALSE != enabled; // normalize
    interface_unlock_poke(this);
    return SL_RESULT_SUCCESS;
}

static SLresult IVirtualizer_IsEnabled(SLVirtualizerItf self, SLboolean *pEnabled)
{
    if (NULL == pEnabled)
        return SL_RESULT_PARAMETER_INVALID;
    IVirtualizer *this = (IVirtualizer *) self;
    interface_lock_peek(this);
    SLboolean enabled = this->mEnabled;
    interface_unlock_peek(this);
    *pEnabled = enabled;
    return SL_RESULT_SUCCESS;
}

static SLresult IVirtualizer_SetStrength(SLVirtualizerItf self, SLpermille strength)
{
    if (!(0 <= strength && strength <= 1000))
        return SL_RESULT_PARAMETER_INVALID;
    IVirtualizer *this = (IVirtualizer *) self;
    interface_lock_poke(this);
    this->mStrength = strength;
    interface_unlock_poke(this);
    return SL_RESULT_SUCCESS;
}

static SLresult IVirtualizer_GetRoundedStrength(SLVirtualizerItf self, SLpermille *pStrength)
{
    if (NULL == pStrength)
        return SL_RESULT_PARAMETER_INVALID;
    IVirtualizer *this = (IVirtualizer *) self;
    interface_lock_peek(this);
    SLpermille strength = this->mStrength;
    interface_unlock_peek(this);
    *pStrength = strength;
    return SL_RESULT_SUCCESS;
}

static SLresult IVirtualizer_IsStrengthSupported(SLVirtualizerItf self, SLboolean *pSupported)
{
    if (NULL == pSupported)
        return SL_RESULT_PARAMETER_INVALID;
    *pSupported = SL_BOOLEAN_TRUE;
    return SL_RESULT_SUCCESS;
}

static const struct SLVirtualizerItf_ IVirtualizer_Itf = {
    IVirtualizer_SetEnabled,
    IVirtualizer_IsEnabled,
    IVirtualizer_SetStrength,
    IVirtualizer_GetRoundedStrength,
    IVirtualizer_IsStrengthSupported
};

void IVirtualizer_init(void *self)
{
    IVirtualizer *this = (IVirtualizer *) self;
    this->mItf = &IVirtualizer_Itf;
    this->mEnabled = SL_BOOLEAN_FALSE;
    this->mStrength = 0;
}
