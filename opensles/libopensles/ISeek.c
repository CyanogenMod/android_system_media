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

/* Seek implementation */

#include "sles_allinclusive.h"

static SLresult ISeek_SetPosition(SLSeekItf self, SLmillisecond pos,
    SLuint32 seekMode)
{
    switch (seekMode) {
    case SL_SEEKMODE_FAST:
    case SL_SEEKMODE_ACCURATE:
        break;
    default:
        return SL_RESULT_PARAMETER_INVALID;
    }
    ISeek *this = (ISeek *) self;
    interface_lock_poke(this);
    this->mPos = pos;
    interface_unlock_poke(this);
    return SL_RESULT_SUCCESS;
}

static SLresult ISeek_SetLoop(SLSeekItf self, SLboolean loopEnable,
    SLmillisecond startPos, SLmillisecond endPos)
{
    ISeek *this = (ISeek *) self;
    interface_lock_exclusive(this);
    this->mLoopEnabled = loopEnable;
    this->mStartPos = startPos;
    this->mEndPos = endPos;
    interface_unlock_exclusive(this);
    return SL_RESULT_SUCCESS;
}

static SLresult ISeek_GetLoop(SLSeekItf self, SLboolean *pLoopEnabled,
    SLmillisecond *pStartPos, SLmillisecond *pEndPos)
{
    if (NULL == pLoopEnabled || NULL == pStartPos || NULL == pEndPos)
        return SL_RESULT_PARAMETER_INVALID;
    ISeek *this = (ISeek *) self;
    interface_lock_shared(this);
    SLboolean loopEnabled = this->mLoopEnabled;
    SLmillisecond startPos = this->mStartPos;
    SLmillisecond endPos = this->mEndPos;
    interface_unlock_shared(this);
    *pLoopEnabled = loopEnabled;
    *pStartPos = startPos;
    *pEndPos = endPos;
    return SL_RESULT_SUCCESS;
}

static const struct SLSeekItf_ ISeek_Itf = {
    ISeek_SetPosition,
    ISeek_SetLoop,
    ISeek_GetLoop
};

void ISeek_init(void *self)
{
    ISeek *this = (ISeek *) self;
    this->mItf = &ISeek_Itf;
    this->mPos = (SLmillisecond) -1;
    this->mStartPos = (SLmillisecond) -1;
    this->mEndPos = (SLmillisecond) -1;
    this->mLoopEnabled = SL_BOOLEAN_FALSE;
}
