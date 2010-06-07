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

/* MIDIMuteSolo implementation */

#include "sles_allinclusive.h"

static SLresult IMIDIMuteSolo_SetChannelMute(SLMIDIMuteSoloItf self, SLuint8 channel, SLboolean mute)
{
    if (channel > 15)
        return SL_RESULT_PARAMETER_INVALID;
    IMIDIMuteSolo *this = (IMIDIMuteSolo *) self;
    SLuint16 mask = 1 << channel;
    interface_lock_exclusive(this);
    if (mute)
        this->mChannelMuteMask |= mask;
    else
        this->mChannelMuteMask &= ~mask;
    interface_unlock_exclusive(this);
    return SL_RESULT_SUCCESS;
}

static SLresult IMIDIMuteSolo_GetChannelMute(SLMIDIMuteSoloItf self, SLuint8 channel,
    SLboolean *pMute)
{
    if (channel > 15 || (NULL == pMute))
        return SL_RESULT_PARAMETER_INVALID;
    IMIDIMuteSolo *this = (IMIDIMuteSolo *) self;
    interface_lock_peek(this);
    SLuint16 mask = this->mChannelMuteMask;
    interface_unlock_peek(this);
    *pMute = (mask >> channel) & 1;
    return SL_RESULT_SUCCESS;
}

static SLresult IMIDIMuteSolo_SetChannelSolo(SLMIDIMuteSoloItf self, SLuint8 channel, SLboolean solo)
{
    if (channel > 15)
        return SL_RESULT_PARAMETER_INVALID;
    IMIDIMuteSolo *this = (IMIDIMuteSolo *) self;
    SLuint16 mask = 1 << channel;
    interface_lock_exclusive(this);
    if (solo)
        this->mChannelSoloMask |= mask;
    else
        this->mChannelSoloMask &= ~mask;
    interface_unlock_exclusive(this);
    return SL_RESULT_SUCCESS;
}

static SLresult IMIDIMuteSolo_GetChannelSolo(SLMIDIMuteSoloItf self, SLuint8 channel,
    SLboolean *pSolo)
{
    if (channel > 15 || (NULL == pSolo))
        return SL_RESULT_PARAMETER_INVALID;
    IMIDIMuteSolo *this = (IMIDIMuteSolo *) self;
    interface_lock_peek(this);
    SLuint16 mask = this->mChannelSoloMask;
    interface_unlock_peek(this);
    *pSolo = (mask >> channel) & 1;
    return SL_RESULT_SUCCESS;
}

static SLresult IMIDIMuteSolo_GetTrackCount(SLMIDIMuteSoloItf self, SLuint16 *pCount)
{
    if (NULL == pCount)
        return SL_RESULT_PARAMETER_INVALID;
    IMIDIMuteSolo *this = (IMIDIMuteSolo *) self;
    // const, so no lock needed
    SLuint16 trackCount = this->mTrackCount;
    *pCount = trackCount;
    return SL_RESULT_SUCCESS;
}

static SLresult IMIDIMuteSolo_SetTrackMute(SLMIDIMuteSoloItf self, SLuint16 track, SLboolean mute)
{
    IMIDIMuteSolo *this = (IMIDIMuteSolo *) self;
    // const
    if (!(track < this->mTrackCount))
        return SL_RESULT_PARAMETER_INVALID;
    SLuint32 mask = 1 << track;
    interface_lock_exclusive(this);
    if (mute)
        this->mTrackMuteMask |= mask;
    else
        this->mTrackMuteMask &= ~mask;
    interface_unlock_exclusive(this);
    return SL_RESULT_SUCCESS;
}

static SLresult IMIDIMuteSolo_GetTrackMute(SLMIDIMuteSoloItf self, SLuint16 track, SLboolean *pMute)
{
    IMIDIMuteSolo *this = (IMIDIMuteSolo *) self;
    // const, no lock needed
    if (!(track < this->mTrackCount) || NULL == pMute)
        return SL_RESULT_PARAMETER_INVALID;
    interface_lock_peek(this);
    SLuint32 mask = this->mTrackMuteMask;
    interface_unlock_peek(this);
    *pMute = (mask >> track) & 1;
    return SL_RESULT_SUCCESS;
}

static SLresult IMIDIMuteSolo_SetTrackSolo(SLMIDIMuteSoloItf self, SLuint16 track, SLboolean solo)
{
    IMIDIMuteSolo *this = (IMIDIMuteSolo *) self;
    // const
    if (!(track < this->mTrackCount))
        return SL_RESULT_PARAMETER_INVALID;
    SLuint32 mask = 1 << track;
    interface_lock_exclusive(this);
    if (solo)
        this->mTrackSoloMask |= mask;
    else
        this->mTrackSoloMask &= ~mask;
    interface_unlock_exclusive(this);
    return SL_RESULT_SUCCESS;
}

static SLresult IMIDIMuteSolo_GetTrackSolo(SLMIDIMuteSoloItf self, SLuint16 track, SLboolean *pSolo)
{
    IMIDIMuteSolo *this = (IMIDIMuteSolo *) self;
    // const, no lock needed
    if (!(track < this->mTrackCount) || NULL == pSolo)
        return SL_RESULT_PARAMETER_INVALID;
    interface_lock_peek(this);
    SLuint32 mask = this->mTrackSoloMask;
    interface_unlock_peek(this);
    *pSolo = (mask >> track) & 1;
    return SL_RESULT_SUCCESS;
}

static const struct SLMIDIMuteSoloItf_ IMIDIMuteSolo_Itf = {
    IMIDIMuteSolo_SetChannelMute,
    IMIDIMuteSolo_GetChannelMute,
    IMIDIMuteSolo_SetChannelSolo,
    IMIDIMuteSolo_GetChannelSolo,
    IMIDIMuteSolo_GetTrackCount,
    IMIDIMuteSolo_SetTrackMute,
    IMIDIMuteSolo_GetTrackMute,
    IMIDIMuteSolo_SetTrackSolo,
    IMIDIMuteSolo_GetTrackSolo
};

void IMIDIMuteSolo_init(void *self)
{
    IMIDIMuteSolo *this = (IMIDIMuteSolo *) self;
    this->mItf = &IMIDIMuteSolo_Itf;
    this->mChannelMuteMask = 0;
    this->mChannelSoloMask = 0;
    this->mTrackMuteMask = 0;
    this->mTrackSoloMask = 0;
    // const
    this->mTrackCount = 32; // FIXME
}
