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

/* Volume implementation */

#include "sles_allinclusive.h"

static SLresult IVolume_SetVolumeLevel(SLVolumeItf self, SLmillibel level)
{
    if (!((SL_MILLIBEL_MIN <= level) && (level <= PLATFORM_MILLIBEL_MAX_VOLUME)))
        return SL_RESULT_PARAMETER_INVALID;
    IVolume *this = (IVolume *) self;
    interface_lock_exclusive(this);
    SLmillibel oldLevel = this->mLevel;
    if (oldLevel != level) {
        this->mLevel = level;
        interface_unlock_exclusive_attributes(this, ATTR_GAIN);
    } else
        interface_unlock_exclusive(this);
    return SL_RESULT_SUCCESS;
}

static SLresult IVolume_GetVolumeLevel(SLVolumeItf self, SLmillibel *pLevel)
{
    if (NULL == pLevel)
        return SL_RESULT_PARAMETER_INVALID;
    IVolume *this = (IVolume *) self;
    interface_lock_peek(this);
    SLmillibel level = this->mLevel;
    interface_unlock_peek(this);
    *pLevel = level;
    return SL_RESULT_SUCCESS;
}

static SLresult IVolume_GetMaxVolumeLevel(SLVolumeItf self, SLmillibel *pMaxLevel)
{
    if (NULL == pMaxLevel)
        return SL_RESULT_PARAMETER_INVALID;
    *pMaxLevel = PLATFORM_MILLIBEL_MAX_VOLUME;
    return SL_RESULT_SUCCESS;
}

static SLresult IVolume_SetMute(SLVolumeItf self, SLboolean mute)
{
    IVolume *this = (IVolume *) self;
    mute = SL_BOOLEAN_FALSE != mute; // normalize
    interface_lock_exclusive(this);
    SLboolean oldMute = this->mMute;
    if (oldMute != mute) {
        this->mMute = mute;
        interface_unlock_exclusive_attributes(this, ATTR_GAIN);
    } else
        interface_unlock_exclusive(this);
    return SL_RESULT_SUCCESS;
}

static SLresult IVolume_GetMute(SLVolumeItf self, SLboolean *pMute)
{
    if (NULL == pMute)
        return SL_RESULT_PARAMETER_INVALID;
    IVolume *this = (IVolume *) self;
    interface_lock_peek(this);
    SLboolean mute = this->mMute;
    interface_unlock_peek(this);
    *pMute = mute;
    return SL_RESULT_SUCCESS;
}

static SLresult IVolume_EnableStereoPosition(SLVolumeItf self, SLboolean enable)
{
    IVolume *this = (IVolume *) self;
    enable = SL_BOOLEAN_FALSE != enable; // normalize
    interface_lock_exclusive(this);
    SLboolean oldEnable = this->mEnableStereoPosition;
    if (oldEnable != enable) {
        this->mEnableStereoPosition = enable;
        interface_unlock_exclusive_attributes(this, ATTR_GAIN);
    } else
        interface_unlock_exclusive(this);
    return SL_RESULT_SUCCESS;
}

static SLresult IVolume_IsEnabledStereoPosition(SLVolumeItf self, SLboolean *pEnable)
{
    if (NULL == pEnable)
        return SL_RESULT_PARAMETER_INVALID;
    IVolume *this = (IVolume *) self;
    interface_lock_peek(this);
    SLboolean enable = this->mEnableStereoPosition;
    interface_unlock_peek(this);
    *pEnable = enable;
    return SL_RESULT_SUCCESS;
}

static SLresult IVolume_SetStereoPosition(SLVolumeItf self, SLpermille stereoPosition)
{
    if (!((-1000 <= stereoPosition) && (1000 >= stereoPosition)))
        return SL_RESULT_PARAMETER_INVALID;
    IVolume *this = (IVolume *) self;
    interface_lock_exclusive(this);
    SLpermille oldStereoPosition = this->mStereoPosition;
    if (oldStereoPosition != stereoPosition) {
        this->mStereoPosition = stereoPosition;
        interface_unlock_exclusive_attributes(this, ATTR_GAIN);
    } else
        interface_unlock_exclusive(this);
    return SL_RESULT_SUCCESS;
}

static SLresult IVolume_GetStereoPosition(SLVolumeItf self, SLpermille *pStereoPosition)
{
    if (NULL == pStereoPosition)
        return SL_RESULT_PARAMETER_INVALID;
    IVolume *this = (IVolume *) self;
    interface_lock_peek(this);
    SLpermille stereoPosition = this->mStereoPosition;
    interface_unlock_peek(this);
    *pStereoPosition = stereoPosition;
    return SL_RESULT_SUCCESS;
}

static const struct SLVolumeItf_ IVolume_Itf = {
    IVolume_SetVolumeLevel,
    IVolume_GetVolumeLevel,
    IVolume_GetMaxVolumeLevel,
    IVolume_SetMute,
    IVolume_GetMute,
    IVolume_EnableStereoPosition,
    IVolume_IsEnabledStereoPosition,
    IVolume_SetStereoPosition,
    IVolume_GetStereoPosition
};

void IVolume_init(void *self)
{
    IVolume *this = (IVolume *) self;
    this->mItf = &IVolume_Itf;
    this->mLevel = 0;
    this->mMute = SL_BOOLEAN_FALSE;
    this->mEnableStereoPosition = SL_BOOLEAN_FALSE;
    this->mStereoPosition = 0;
}
