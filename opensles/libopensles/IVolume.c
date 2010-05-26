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

//#include "sles_allinclusive.h"

static SLresult IVolume_SetVolumeLevel(SLVolumeItf self, SLmillibel level)
{
#if 0
    // some compilers allow a wider int type to be passed as a parameter,
    // but that will be truncated during the field assignment
    if (!((SL_MILLIBEL_MIN <= level) && (SL_MILLIBEL_MAX >= level)))
        return SL_RESULT_PARAMETER_INVALID;
#endif
#ifdef USE_ANDROID
    if (!((SL_MILLIBEL_MIN <= level) && (ANDROID_SL_MILLIBEL_MAX >= level)))
        return SL_RESULT_PARAMETER_INVALID;
#endif

    IVolume *this = (IVolume *) self;
    interface_lock_poke(this);
    this->mLevel = level;
    if(this->mMute == SL_BOOLEAN_FALSE) {
#ifdef USE_ANDROID
        // FIXME poke lock correct?
        switch(this->mThis->mClass->mObjectID) {
        case SL_OBJECTID_AUDIOPLAYER:
            sles_to_android_audioPlayerVolumeUpdate(this);
            break;
        case SL_OBJECTID_OUTPUTMIX:
            // FIXME mute/unmute all players attached to this outputmix
            fprintf(stderr, "FIXME: IVolume_SetVolumeLevel on an SL_OBJECTID_OUTPUTMIX to be implemented\n");
            break;
        default:
            break;
        }
#endif
    }
    interface_unlock_poke(this);
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

static SLresult IVolume_GetMaxVolumeLevel(SLVolumeItf self,
    SLmillibel *pMaxLevel)
{
    if (NULL == pMaxLevel)
        return SL_RESULT_PARAMETER_INVALID;
    *pMaxLevel = SL_MILLIBEL_MAX;
#ifdef USE_ANDROID
    *pMaxLevel = ANDROID_SL_MILLIBEL_MAX;
#endif
    return SL_RESULT_SUCCESS;
}

static SLresult IVolume_SetMute(SLVolumeItf self, SLboolean mute)
{
    IVolume *this = (IVolume *) self;
    interface_lock_poke(this);
    this->mMute = mute;
    if(this->mMute == SL_BOOLEAN_FALSE) {
        // when unmuting, reapply volume
        IVolume_SetVolumeLevel(self, this->mLevel);
    }
#ifdef USE_ANDROID
    // FIXME poke lock correct?
    switch(this->mThis->mClass->mObjectID) {
        case SL_OBJECTID_AUDIOPLAYER:
            sles_to_android_audioPlayerSetMute(this, mute);
            break;
        case SL_OBJECTID_OUTPUTMIX:
            // FIXME mute/unmute all players attached to this outputmix
            fprintf(stderr, "FIXME: IVolume_SetMute on an SL_OBJECTID_OUTPUTMIX to be implemented\n");
            break;
        default:
            break;
    }
#endif
    interface_unlock_poke(this);
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
    interface_lock_poke(this);
    this->mEnableStereoPosition = enable;
#ifdef USE_ANDROID
    // FIXME poke lock correct?
    if (this->mThis->mClass->mObjectID == SL_OBJECTID_AUDIOPLAYER) {
        sles_to_android_audioPlayerVolumeUpdate(this);
    }
#endif
    interface_unlock_poke(this);
    return SL_RESULT_SUCCESS;
}

static SLresult IVolume_IsEnabledStereoPosition(SLVolumeItf self,
    SLboolean *pEnable)
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

static SLresult IVolume_SetStereoPosition(SLVolumeItf self,
    SLpermille stereoPosition)
{
    if (!((-1000 <= stereoPosition) && (1000 >= stereoPosition)))
        return SL_RESULT_PARAMETER_INVALID;
    IVolume *this = (IVolume *) self;
    interface_lock_poke(this);
    this->mStereoPosition = stereoPosition;
#ifdef USE_ANDROID
    // FIXME poke lock correct?
    if (this->mThis->mClass->mObjectID == SL_OBJECTID_AUDIOPLAYER) {
        sles_to_android_audioPlayerVolumeUpdate(this);
    }
#endif
    interface_unlock_poke(this);
    return SL_RESULT_SUCCESS;
}

static SLresult IVolume_GetStereoPosition(SLVolumeItf self,
    SLpermille *pStereoPosition)
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
#ifndef NDEBUG
    this->mLevel = 0;
    this->mMute = SL_BOOLEAN_FALSE;
    this->mEnableStereoPosition = SL_BOOLEAN_FALSE;
    this->mStereoPosition = 0;
#endif
#ifdef USE_ANDROID
    this->mAmplFromVolLevel = 1.0f;
    this->mAmplFromStereoPos[0] = 1.0f;
    this->mAmplFromStereoPos[1] = 1.0f;
    this->mChannelMutes = 0;
    this->mChannelSolos = 0;
#endif
}
