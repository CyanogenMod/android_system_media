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

/* MIDITime implementation */

#include "sles_allinclusive.h"

static SLresult IMIDITime_GetDuration(SLMIDITimeItf self, SLuint32 *pDuration)
{
    if (NULL == pDuration)
        return SL_RESULT_PARAMETER_INVALID;
    IMIDITime *this = (IMIDITime *) self;
    SLuint32 duration = this->mDuration;
    *pDuration = duration;
    return SL_RESULT_SUCCESS;
}

static SLresult IMIDITime_SetPosition(SLMIDITimeItf self, SLuint32 position)
{
    IMIDITime *this = (IMIDITime *) self;
    interface_lock_poke(this);
    this->mPosition = position;
    interface_unlock_poke(this);
    return SL_RESULT_SUCCESS;
}

static SLresult IMIDITime_GetPosition(SLMIDITimeItf self, SLuint32 *pPosition)
{
    if (NULL == pPosition)
        return SL_RESULT_PARAMETER_INVALID;
    IMIDITime *this = (IMIDITime *) self;
    interface_lock_peek(this);
    SLuint32 position = this->mPosition;
    interface_unlock_peek(this);
    *pPosition = position;
    return SL_RESULT_SUCCESS;
}

static SLresult IMIDITime_SetLoopPoints(SLMIDITimeItf self, SLuint32 startTick, SLuint32 numTicks)
{
    IMIDITime *this = (IMIDITime *) self;
    interface_lock_exclusive(this);
    this->mStartTick = startTick;
    this->mNumTicks = numTicks;
    interface_unlock_exclusive(this);
    return SL_RESULT_SUCCESS;
}

static SLresult IMIDITime_GetLoopPoints(SLMIDITimeItf self, SLuint32 *pStartTick,
    SLuint32 *pNumTicks)
{
    if (NULL == pStartTick || NULL == pNumTicks)
        return SL_RESULT_PARAMETER_INVALID;
    IMIDITime *this = (IMIDITime *) self;
    interface_lock_shared(this);
    SLuint32 startTick = this->mStartTick;
    SLuint32 numTicks = this->mNumTicks;
    interface_unlock_shared(this);
    *pStartTick = startTick;
    *pNumTicks = numTicks;
    return SL_RESULT_SUCCESS;
}

static const struct SLMIDITimeItf_ IMIDITime_Itf = {
    IMIDITime_GetDuration,
    IMIDITime_SetPosition,
    IMIDITime_GetPosition,
    IMIDITime_SetLoopPoints,
    IMIDITime_GetLoopPoints
};

void IMIDITime_init(void *self)
{
    IMIDITime *this = (IMIDITime *) self;
    this->mItf = &IMIDITime_Itf;
#ifndef NDEBUG
    this->mDuration = 0;
    this->mPosition = 0;
    this->mStartTick = 0;
    this->mNumTicks = 0;
#endif
}
