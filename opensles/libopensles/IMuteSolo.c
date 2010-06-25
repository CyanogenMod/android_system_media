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

/* MuteSolo implementation */

#include "sles_allinclusive.h"

static SLresult IMuteSolo_SetChannelMute(SLMuteSoloItf self, SLuint8 chan, SLboolean mute)
{
    IMuteSolo *this = (IMuteSolo *) self;
    // SLMuteSolo available only on AudioPlayer
    // FIXME check if this is true (see spec appendix D) or false (see spec 7.8 MIDI player object)
    CAudioPlayer* ap = InterfaceToCAudioPlayer(this);
    if (ap->mNumChannels <= chan)
        return SL_RESULT_PARAMETER_INVALID;
    SLuint32 mask = 1 << chan;
    interface_lock_exclusive(this);
    if (mute)
        ap->mMuteMask |= mask;
    else
        ap->mMuteMask &= ~mask;
#ifdef ANDROID
        if (SL_OBJECTID_AUDIOPLAYER == InterfaceToObjectID(this)) {
            sles_to_android_audioPlayerVolumeUpdate(InterfaceToCAudioPlayer(this));
        }
#endif
    interface_unlock_exclusive(this);
    return SL_RESULT_SUCCESS;
}

static SLresult IMuteSolo_GetChannelMute(SLMuteSoloItf self, SLuint8 chan, SLboolean *pMute)
{
    if (NULL == pMute)
        return SL_RESULT_PARAMETER_INVALID;
    IMuteSolo *this = (IMuteSolo *) self;
    // SLMuteSolo available only on AudioPlayer
    // FIXME check if this is true (see spec appendix D) or false (see spec 7.8 MIDI player object)
    CAudioPlayer* ap = InterfaceToCAudioPlayer(this);
    if (ap->mNumChannels <= chan)
        return SL_RESULT_PARAMETER_INVALID;
    interface_lock_peek(this);
    SLuint32 mask = ap->mMuteMask;
    interface_unlock_peek(this);
    *pMute = (mask >> chan) & 1;
    return SL_RESULT_SUCCESS;
}

static SLresult IMuteSolo_SetChannelSolo(SLMuteSoloItf self, SLuint8 chan, SLboolean solo)
{
    IMuteSolo *this = (IMuteSolo *) self;
    // SLMuteSolo available only on AudioPlayer
    // FIXME check if this is true (see spec appendix D) or false (see spec 7.8 MIDI player object)
    CAudioPlayer* ap = InterfaceToCAudioPlayer(this);
    if (ap->mNumChannels <= chan)
        return SL_RESULT_PARAMETER_INVALID;
    SLuint32 mask = 1 << chan;
    interface_lock_exclusive(this);
    if (solo)
        ap->mSoloMask |= mask;
    else
        ap->mSoloMask &= ~mask;
#ifdef ANDROID
        if (SL_OBJECTID_AUDIOPLAYER == InterfaceToObjectID(this)) {
            sles_to_android_audioPlayerVolumeUpdate(InterfaceToCAudioPlayer(this));
        }
#endif
    interface_unlock_exclusive(this);
    return SL_RESULT_SUCCESS;
}

static SLresult IMuteSolo_GetChannelSolo(SLMuteSoloItf self, SLuint8 chan, SLboolean *pSolo)
{
    if (NULL == pSolo)
        return SL_RESULT_PARAMETER_INVALID;
    IMuteSolo *this = (IMuteSolo *) self;
    // SLMuteSolo available only on AudioPlayer
    // FIXME check if this is true (see spec appendix D) or false (see spec 7.8 MIDI player object)
    CAudioPlayer* ap = InterfaceToCAudioPlayer(this);
    if (ap->mNumChannels <= chan)
        return SL_RESULT_PARAMETER_INVALID;
    interface_lock_peek(this);
    SLuint32 mask = ap->mSoloMask;
    interface_unlock_peek(this);
    *pSolo = (mask >> chan) & 1;
    return SL_RESULT_SUCCESS;
}

static SLresult IMuteSolo_GetNumChannels(SLMuteSoloItf self, SLuint8 *pNumChannels)
{
    if (NULL == pNumChannels)
        return SL_RESULT_PARAMETER_INVALID;
    IMuteSolo *this = (IMuteSolo *) self;
    // SLMuteSolo available only on AudioPlayer
    // FIXME check if this is true (see spec appendix D) or false (see spec 7.8 MIDI player object)
    *pNumChannels = InterfaceToCAudioPlayer(this)->mNumChannels;
    return SL_RESULT_SUCCESS;
}

static const struct SLMuteSoloItf_ IMuteSolo_Itf = {
    IMuteSolo_SetChannelMute,
    IMuteSolo_GetChannelMute,
    IMuteSolo_SetChannelSolo,
    IMuteSolo_GetChannelSolo,
    IMuteSolo_GetNumChannels
};

void IMuteSolo_init(void *self)
{
    IMuteSolo *this = (IMuteSolo *) self;
    this->mItf = &IMuteSolo_Itf;
}
