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

/* BassBoost implementation */

#include "sles_allinclusive.h"


static SLresult IBassBoost_SetEnabled(SLBassBoostItf self, SLboolean enabled)
{
    SL_ENTER_INTERFACE

    IBassBoost *this = (IBassBoost *) self;
    interface_lock_poke(this);
    this->mEnabled = SL_BOOLEAN_FALSE != enabled; // normalize
    interface_unlock_poke(this);
    result = SL_RESULT_SUCCESS;

    SL_LEAVE_INTERFACE
}


static SLresult IBassBoost_IsEnabled(SLBassBoostItf self, SLboolean *pEnabled)
{
    SL_ENTER_INTERFACE

    if (NULL == pEnabled) {
        result = SL_RESULT_PARAMETER_INVALID;
    } else {
        IBassBoost *this = (IBassBoost *) self;
        interface_lock_peek(this);
        SLboolean enabled = this->mEnabled;
        interface_unlock_peek(this);
        *pEnabled = enabled;
        result = SL_RESULT_SUCCESS;
    }

    SL_LEAVE_INTERFACE
}


static SLresult IBassBoost_SetStrength(SLBassBoostItf self, SLpermille strength)
{
    SL_ENTER_INTERFACE

    if (!(0 <= strength) && (strength <= 1000)) {
        result = SL_RESULT_PARAMETER_INVALID;
    } else {
        IBassBoost *this = (IBassBoost *) self;
        interface_lock_poke(this);
        this->mStrength = strength;
        interface_unlock_poke(this);
        result = SL_RESULT_SUCCESS;
    }

    SL_LEAVE_INTERFACE
}


static SLresult IBassBoost_GetRoundedStrength(SLBassBoostItf self, SLpermille *pStrength)
{
    SL_ENTER_INTERFACE

    if (NULL == pStrength) {
        result = SL_RESULT_PARAMETER_INVALID;
    } else {
        IBassBoost *this = (IBassBoost *) self;
        interface_lock_peek(this);
        SLpermille strength = this->mStrength;
        interface_unlock_peek(this);
        *pStrength = strength;
        result = SL_RESULT_SUCCESS;
    }

    SL_LEAVE_INTERFACE
}


static SLresult IBassBoost_IsStrengthSupported(SLBassBoostItf self, SLboolean *pSupported)
{
    SL_ENTER_INTERFACE

    if (NULL == pSupported) {
        result = SL_RESULT_PARAMETER_INVALID;
    } else {
        *pSupported = SL_BOOLEAN_TRUE;
        result = SL_RESULT_SUCCESS;
    }

    SL_LEAVE_INTERFACE
}


static const struct SLBassBoostItf_ IBassBoost_Itf = {
    IBassBoost_SetEnabled,
    IBassBoost_IsEnabled,
    IBassBoost_SetStrength,
    IBassBoost_GetRoundedStrength,
    IBassBoost_IsStrengthSupported
};

void IBassBoost_init(void *self)
{
    IBassBoost *this = (IBassBoost *) self;
    this->mItf = &IBassBoost_Itf;
    this->mEnabled = SL_BOOLEAN_FALSE;
    this->mStrength = 1000;
}
