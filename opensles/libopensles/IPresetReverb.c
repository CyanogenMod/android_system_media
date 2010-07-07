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

/* PresetReverb implementation */

#include "sles_allinclusive.h"

static SLresult IPresetReverb_SetPreset(SLPresetReverbItf self, SLuint16 preset)
{
    SL_ENTER_INTERFACE

    IPresetReverb *this = (IPresetReverb *) self;
    switch (preset) {
    case SL_REVERBPRESET_NONE:
    case SL_REVERBPRESET_SMALLROOM:
    case SL_REVERBPRESET_MEDIUMROOM:
    case SL_REVERBPRESET_LARGEROOM:
    case SL_REVERBPRESET_MEDIUMHALL:
    case SL_REVERBPRESET_LARGEHALL:
    case SL_REVERBPRESET_PLATE:
        interface_lock_poke(this);
        this->mPreset = preset;
        interface_unlock_poke(this);
        result = SL_RESULT_SUCCESS;
        break;
    default:
        result = SL_RESULT_PARAMETER_INVALID;
        break;
    }

    SL_LEAVE_INTERFACE
}

static SLresult IPresetReverb_GetPreset(SLPresetReverbItf self, SLuint16 *pPreset)
{
    SL_ENTER_INTERFACE

    if (NULL == pPreset) {
        result = SL_RESULT_PARAMETER_INVALID;
    } else {
        IPresetReverb *this = (IPresetReverb *) self;
        interface_lock_peek(this);
        SLuint16 preset = this->mPreset;
        interface_unlock_peek(this);
        *pPreset = preset;
        result = SL_RESULT_SUCCESS;
    }

    SL_LEAVE_INTERFACE
}

static const struct SLPresetReverbItf_ IPresetReverb_Itf = {
    IPresetReverb_SetPreset,
    IPresetReverb_GetPreset
};

void IPresetReverb_init(void *self)
{
    IPresetReverb *this = (IPresetReverb *) self;
    this->mItf = &IPresetReverb_Itf;
    this->mPreset = SL_REVERBPRESET_NONE;
}
