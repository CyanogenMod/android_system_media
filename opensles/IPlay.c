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

/* Play implementation */

#include "sles_allinclusive.h"

static SLresult IPlay_SetPlayState(SLPlayItf self, SLuint32 state)
{
    switch (state) {
    case SL_PLAYSTATE_STOPPED:
    case SL_PLAYSTATE_PAUSED:
    case SL_PLAYSTATE_PLAYING:
        break;
    default:
        return SL_RESULT_PARAMETER_INVALID;
    }
    IPlay *this = (IPlay *) self;
    interface_lock_exclusive(this);
    this->mState = state;
    if (SL_PLAYSTATE_STOPPED == state) {
        this->mPosition = (SLmillisecond) 0;
        // this->mPositionSamples = 0;
    }
    interface_unlock_exclusive(this);
#ifdef USE_ANDROID
    return sles_to_android_audioPlayerSetPlayState(this, state);
#else
    return SL_RESULT_SUCCESS;
#endif
}

static SLresult IPlay_GetPlayState(SLPlayItf self, SLuint32 *pState)
{
    if (NULL == pState)
        return SL_RESULT_PARAMETER_INVALID;
    IPlay *this = (IPlay *) self;
    interface_lock_peek(this);
    SLuint32 state = this->mState;
    interface_unlock_peek(this);
    *pState = state;
    return SL_RESULT_SUCCESS;
}

static SLresult IPlay_GetDuration(SLPlayItf self, SLmillisecond *pMsec)
{
    // FIXME: for SNDFILE only, check to see if already know duration
    // if so, good, otherwise save position,
    // read quickly to end of file, counting frames,
    // use sample rate to compute duration, then seek back to current position
    if (NULL == pMsec)
        return SL_RESULT_PARAMETER_INVALID;
    IPlay *this = (IPlay *) self;
    interface_lock_peek(this);
    SLmillisecond duration = this->mDuration;
    interface_unlock_peek(this);
    *pMsec = duration;
    return SL_RESULT_SUCCESS;
}

static SLresult IPlay_GetPosition(SLPlayItf self, SLmillisecond *pMsec)
{
    if (NULL == pMsec)
        return SL_RESULT_PARAMETER_INVALID;
    IPlay *this = (IPlay *) self;
    interface_lock_peek(this);
    SLmillisecond position = this->mPosition;
    interface_unlock_peek(this);
    *pMsec = position;
    // FIXME convert sample units to time units
    // FIXME handle SL_TIME_UNKNOWN
    return SL_RESULT_SUCCESS;
}

static SLresult IPlay_RegisterCallback(SLPlayItf self, slPlayCallback callback,
    void *pContext)
{
    IPlay *this = (IPlay *) self;
    // FIXME This could be a poke lock, if we had atomic double-word load/store
    interface_lock_exclusive(this);
    this->mCallback = callback;
    this->mContext = pContext;
    interface_unlock_exclusive(this);
    return SL_RESULT_SUCCESS;
}

static SLresult IPlay_SetCallbackEventsMask(SLPlayItf self, SLuint32 eventFlags)
{
    IPlay *this = (IPlay *) self;
    interface_lock_poke(this);
    this->mEventFlags = eventFlags;
    interface_unlock_poke(this);
    return SL_RESULT_SUCCESS;
}

static SLresult IPlay_GetCallbackEventsMask(SLPlayItf self,
    SLuint32 *pEventFlags)
{
    if (NULL == pEventFlags)
        return SL_RESULT_PARAMETER_INVALID;
    IPlay *this = (IPlay *) self;
    interface_lock_peek(this);
    SLuint32 eventFlags = this->mEventFlags;
    interface_unlock_peek(this);
    *pEventFlags = eventFlags;
    return SL_RESULT_SUCCESS;
}

static SLresult IPlay_SetMarkerPosition(SLPlayItf self, SLmillisecond mSec)
{
    IPlay *this = (IPlay *) self;
    interface_lock_poke(this);
    this->mMarkerPosition = mSec;
    interface_unlock_poke(this);
    return SL_RESULT_SUCCESS;
}

static SLresult IPlay_ClearMarkerPosition(SLPlayItf self)
{
    IPlay *this = (IPlay *) self;
    interface_lock_poke(this);
    this->mMarkerPosition = 0;
    interface_unlock_poke(this);
    return SL_RESULT_SUCCESS;
}

static SLresult IPlay_GetMarkerPosition(SLPlayItf self, SLmillisecond *pMsec)
{
    if (NULL == pMsec)
        return SL_RESULT_PARAMETER_INVALID;
    IPlay *this = (IPlay *) self;
    interface_lock_peek(this);
    SLmillisecond markerPosition = this->mMarkerPosition;
    interface_unlock_peek(this);
    *pMsec = markerPosition;
    return SL_RESULT_SUCCESS;
}

static SLresult IPlay_SetPositionUpdatePeriod(SLPlayItf self, SLmillisecond mSec)
{
    IPlay *this = (IPlay *) self;
    interface_lock_poke(this);
    this->mPositionUpdatePeriod = mSec;
    interface_unlock_poke(this);
    return SL_RESULT_SUCCESS;
}

static SLresult IPlay_GetPositionUpdatePeriod(SLPlayItf self,
    SLmillisecond *pMsec)
{
    if (NULL == pMsec)
        return SL_RESULT_PARAMETER_INVALID;
    IPlay *this = (IPlay *) self;
    interface_lock_peek(this);
    SLmillisecond positionUpdatePeriod = this->mPositionUpdatePeriod;
    interface_unlock_peek(this);
    *pMsec = positionUpdatePeriod;
    return SL_RESULT_SUCCESS;
}

static const struct SLPlayItf_ IPlay_Itf = {
    IPlay_SetPlayState,
    IPlay_GetPlayState,
    IPlay_GetDuration,
    IPlay_GetPosition,
    IPlay_RegisterCallback,
    IPlay_SetCallbackEventsMask,
    IPlay_GetCallbackEventsMask,
    IPlay_SetMarkerPosition,
    IPlay_ClearMarkerPosition,
    IPlay_GetMarkerPosition,
    IPlay_SetPositionUpdatePeriod,
    IPlay_GetPositionUpdatePeriod
};

void IPlay_init(void *self)
{
    IPlay *this = (IPlay *) self;
    this->mItf = &IPlay_Itf;
    this->mState = SL_PLAYSTATE_STOPPED;
    this->mDuration = SL_TIME_UNKNOWN;
#ifndef NDEBUG
    this->mPosition = (SLmillisecond) 0;
    // this->mPlay.mPositionSamples = 0;
    this->mCallback = NULL;
    this->mContext = NULL;
    this->mEventFlags = 0;
    this->mMarkerPosition = 0;
    this->mPositionUpdatePeriod = 0;
#endif
}
