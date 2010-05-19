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

/* EnvironmentalReverb implementation */

#include "sles_allinclusive.h"

// Note: all Set operations use exclusive not poke,
// because SetEnvironmentalReverbProperties is exclusive.
// It is safe for the Get operations to use peek,
// on the assumption that the block copy will atomically
// replace each word of the block.

static SLresult IEnvironmentalReverb_SetRoomLevel(SLEnvironmentalReverbItf self,
    SLmillibel room)
{
    IEnvironmentalReverb *this = (IEnvironmentalReverb *) self;
    interface_lock_exclusive(this);
    this->mProperties.roomLevel = room;
    interface_unlock_exclusive(this);
    return SL_RESULT_SUCCESS;
}

static SLresult IEnvironmentalReverb_GetRoomLevel(SLEnvironmentalReverbItf self,
    SLmillibel *pRoom)
{
    if (NULL == pRoom)
        return SL_RESULT_PARAMETER_INVALID;
    IEnvironmentalReverb *this = (IEnvironmentalReverb *) self;
    interface_lock_peek(this);
    SLmillibel roomLevel = this->mProperties.roomLevel;
    interface_unlock_peek(this);
    *pRoom = roomLevel;
    return SL_RESULT_SUCCESS;
}

static SLresult IEnvironmentalReverb_SetRoomHFLevel(
    SLEnvironmentalReverbItf self, SLmillibel roomHF)
{
    IEnvironmentalReverb *this = (IEnvironmentalReverb *) self;
    interface_lock_exclusive(this);
    this->mProperties.roomHFLevel = roomHF;
    interface_unlock_exclusive(this);
    return SL_RESULT_SUCCESS;
}

static SLresult IEnvironmentalReverb_GetRoomHFLevel(
    SLEnvironmentalReverbItf self, SLmillibel *pRoomHF)
{
    if (NULL == pRoomHF)
        return SL_RESULT_PARAMETER_INVALID;
    IEnvironmentalReverb *this = (IEnvironmentalReverb *) self;
    interface_lock_peek(this);
    SLmillibel roomHFLevel = this->mProperties.roomHFLevel;
    interface_unlock_peek(this);
    *pRoomHF = roomHFLevel;
    return SL_RESULT_SUCCESS;
}

static SLresult IEnvironmentalReverb_SetDecayTime(
    SLEnvironmentalReverbItf self, SLmillisecond decayTime)
{
    IEnvironmentalReverb *this = (IEnvironmentalReverb *) self;
    interface_lock_exclusive(this);
    this->mProperties.decayTime = decayTime;
    interface_unlock_exclusive(this);
    return SL_RESULT_SUCCESS;
}

static SLresult IEnvironmentalReverb_GetDecayTime(
    SLEnvironmentalReverbItf self, SLmillisecond *pDecayTime)
{
    if (NULL == pDecayTime)
        return SL_RESULT_PARAMETER_INVALID;
    IEnvironmentalReverb *this = (IEnvironmentalReverb *) self;
    interface_lock_peek(this);
    SLmillisecond decayTime = this->mProperties.decayTime;
    interface_unlock_peek(this);
    *pDecayTime = decayTime;
    return SL_RESULT_SUCCESS;
}

static SLresult IEnvironmentalReverb_SetDecayHFRatio(
    SLEnvironmentalReverbItf self, SLpermille decayHFRatio)
{
    IEnvironmentalReverb *this = (IEnvironmentalReverb *) self;
    interface_lock_exclusive(this);
    this->mProperties.decayHFRatio = decayHFRatio;
    interface_unlock_exclusive(this);
    return SL_RESULT_SUCCESS;
}

static SLresult IEnvironmentalReverb_GetDecayHFRatio(
    SLEnvironmentalReverbItf self, SLpermille *pDecayHFRatio)
{
    if (NULL == pDecayHFRatio)
        return SL_RESULT_PARAMETER_INVALID;
    IEnvironmentalReverb *this = (IEnvironmentalReverb *) self;
    interface_lock_peek(this);
    SLpermille decayHFRatio = this->mProperties.decayHFRatio;
    interface_unlock_peek(this);
    *pDecayHFRatio = decayHFRatio;
    return SL_RESULT_SUCCESS;
}

static SLresult IEnvironmentalReverb_SetReflectionsLevel(
    SLEnvironmentalReverbItf self, SLmillibel reflectionsLevel)
{
    IEnvironmentalReverb *this = (IEnvironmentalReverb *) self;
    interface_lock_exclusive(this);
    this->mProperties.reflectionsLevel = reflectionsLevel;
    interface_unlock_exclusive(this);
    return SL_RESULT_SUCCESS;
}

static SLresult IEnvironmentalReverb_GetReflectionsLevel(
    SLEnvironmentalReverbItf self, SLmillibel *pReflectionsLevel)
{
    if (NULL == pReflectionsLevel)
        return SL_RESULT_PARAMETER_INVALID;
    IEnvironmentalReverb *this = (IEnvironmentalReverb *) self;
    interface_lock_peek(this);
    SLmillibel reflectionsLevel = this->mProperties.reflectionsLevel;
    interface_unlock_peek(this);
    *pReflectionsLevel = reflectionsLevel;
    return SL_RESULT_SUCCESS;
}

static SLresult IEnvironmentalReverb_SetReflectionsDelay(
    SLEnvironmentalReverbItf self, SLmillisecond reflectionsDelay)
{
    IEnvironmentalReverb *this = (IEnvironmentalReverb *) self;
    interface_lock_exclusive(this);
    this->mProperties.reflectionsDelay = reflectionsDelay;
    interface_unlock_exclusive(this);
    return SL_RESULT_SUCCESS;
}

static SLresult IEnvironmentalReverb_GetReflectionsDelay(
    SLEnvironmentalReverbItf self, SLmillisecond *pReflectionsDelay)
{
    if (NULL == pReflectionsDelay)
        return SL_RESULT_PARAMETER_INVALID;
    IEnvironmentalReverb *this = (IEnvironmentalReverb *) self;
    interface_lock_peek(this);
    SLmillisecond reflectionsDelay = this->mProperties.reflectionsDelay;
    interface_unlock_peek(this);
    *pReflectionsDelay = reflectionsDelay;
    return SL_RESULT_SUCCESS;
}

static SLresult IEnvironmentalReverb_SetReverbLevel(
    SLEnvironmentalReverbItf self, SLmillibel reverbLevel)
{
    IEnvironmentalReverb *this = (IEnvironmentalReverb *) self;
    interface_lock_exclusive(this);
    this->mProperties.reverbLevel = reverbLevel;
    interface_unlock_exclusive(this);
    return SL_RESULT_SUCCESS;
}

static SLresult IEnvironmentalReverb_GetReverbLevel(
    SLEnvironmentalReverbItf self, SLmillibel *pReverbLevel)
{
    if (NULL == pReverbLevel)
        return SL_RESULT_PARAMETER_INVALID;
    IEnvironmentalReverb *this = (IEnvironmentalReverb *) self;
    interface_lock_peek(this);
    SLmillibel reverbLevel = this->mProperties.reverbLevel;
    interface_unlock_peek(this);
    *pReverbLevel = reverbLevel;
    return SL_RESULT_SUCCESS;
}

static SLresult IEnvironmentalReverb_SetReverbDelay(
    SLEnvironmentalReverbItf self, SLmillisecond reverbDelay)
{
    IEnvironmentalReverb *this = (IEnvironmentalReverb *) self;
    interface_lock_exclusive(this);
    this->mProperties.reverbDelay = reverbDelay;
    interface_unlock_exclusive(this);
    return SL_RESULT_SUCCESS;
}

static SLresult IEnvironmentalReverb_GetReverbDelay(
    SLEnvironmentalReverbItf self, SLmillisecond *pReverbDelay)
{
    if (NULL == pReverbDelay)
        return SL_RESULT_PARAMETER_INVALID;
    IEnvironmentalReverb *this = (IEnvironmentalReverb *) self;
    interface_lock_peek(this);
    SLmillisecond reverbDelay = this->mProperties.reverbDelay;
    interface_unlock_peek(this);
    *pReverbDelay = reverbDelay;
    return SL_RESULT_SUCCESS;
}

static SLresult IEnvironmentalReverb_SetDiffusion(
    SLEnvironmentalReverbItf self, SLpermille diffusion)
{
    IEnvironmentalReverb *this = (IEnvironmentalReverb *) self;
    interface_lock_exclusive(this);
    this->mProperties.diffusion = diffusion;
    interface_unlock_exclusive(this);
    return SL_RESULT_SUCCESS;
}

static SLresult IEnvironmentalReverb_GetDiffusion(SLEnvironmentalReverbItf self,
     SLpermille *pDiffusion)
{
    if (NULL == pDiffusion)
        return SL_RESULT_PARAMETER_INVALID;
    IEnvironmentalReverb *this = (IEnvironmentalReverb *) self;
    interface_lock_peek(this);
    SLpermille diffusion = this->mProperties.diffusion;
    interface_unlock_peek(this);
    *pDiffusion = diffusion;
    return SL_RESULT_SUCCESS;
}

static SLresult IEnvironmentalReverb_SetDensity(SLEnvironmentalReverbItf self,
    SLpermille density)
{
    IEnvironmentalReverb *this = (IEnvironmentalReverb *) self;
    interface_lock_exclusive(this);
    this->mProperties.density = density;
    interface_unlock_exclusive(this);
    return SL_RESULT_SUCCESS;
}

static SLresult IEnvironmentalReverb_GetDensity(SLEnvironmentalReverbItf self,
    SLpermille *pDensity)
{
    if (NULL == pDensity)
        return SL_RESULT_PARAMETER_INVALID;
    IEnvironmentalReverb *this = (IEnvironmentalReverb *) self;
    interface_lock_peek(this);
    SLpermille density = this->mProperties.density;
    interface_unlock_peek(this);
    *pDensity = density;
    return SL_RESULT_SUCCESS;
}

static SLresult IEnvironmentalReverb_SetEnvironmentalReverbProperties(
    SLEnvironmentalReverbItf self,
    const SLEnvironmentalReverbSettings *pProperties)
{
    if (NULL == pProperties)
        return SL_RESULT_PARAMETER_INVALID;
    IEnvironmentalReverb *this = (IEnvironmentalReverb *) self;
    SLEnvironmentalReverbSettings properties = *pProperties;
    interface_lock_exclusive(this);
    this->mProperties = properties;
    interface_unlock_exclusive(this);
    return SL_RESULT_SUCCESS;
}

static SLresult IEnvironmentalReverb_GetEnvironmentalReverbProperties(
    SLEnvironmentalReverbItf self, SLEnvironmentalReverbSettings *pProperties)
{
    if (NULL == pProperties)
        return SL_RESULT_PARAMETER_INVALID;
    IEnvironmentalReverb *this = (IEnvironmentalReverb *) self;
    interface_lock_shared(this);
    SLEnvironmentalReverbSettings properties = this->mProperties;
    interface_unlock_shared(this);
    *pProperties = properties;
    return SL_RESULT_SUCCESS;
}

static const struct SLEnvironmentalReverbItf_ IEnvironmentalReverb_Itf = {
    IEnvironmentalReverb_SetRoomLevel,
    IEnvironmentalReverb_GetRoomLevel,
    IEnvironmentalReverb_SetRoomHFLevel,
    IEnvironmentalReverb_GetRoomHFLevel,
    IEnvironmentalReverb_SetDecayTime,
    IEnvironmentalReverb_GetDecayTime,
    IEnvironmentalReverb_SetDecayHFRatio,
    IEnvironmentalReverb_GetDecayHFRatio,
    IEnvironmentalReverb_SetReflectionsLevel,
    IEnvironmentalReverb_GetReflectionsLevel,
    IEnvironmentalReverb_SetReflectionsDelay,
    IEnvironmentalReverb_GetReflectionsDelay,
    IEnvironmentalReverb_SetReverbLevel,
    IEnvironmentalReverb_GetReverbLevel,
    IEnvironmentalReverb_SetReverbDelay,
    IEnvironmentalReverb_GetReverbDelay,
    IEnvironmentalReverb_SetDiffusion,
    IEnvironmentalReverb_GetDiffusion,
    IEnvironmentalReverb_SetDensity,
    IEnvironmentalReverb_GetDensity,
    IEnvironmentalReverb_SetEnvironmentalReverbProperties,
    IEnvironmentalReverb_GetEnvironmentalReverbProperties
};

static const SLEnvironmentalReverbSettings IEnvironmentalReverb_default = {
    SL_MILLIBEL_MIN, // roomLevel
    0,               // roomHFLevel
    1000,            // decayTime
    500,             // decayHFRatio
    SL_MILLIBEL_MIN, // reflectionsLevel
    20,              // reflectionsDelay
    SL_MILLIBEL_MIN, // reverbLevel
    40,              // reverbDelay
    1000,            // diffusion
    1000             // density
};

void IEnvironmentalReverb_init(void *self)
{
    IEnvironmentalReverb *this = (IEnvironmentalReverb *) self;
    this->mItf = &IEnvironmentalReverb_Itf;
    this->mProperties = IEnvironmentalReverb_default;
}
