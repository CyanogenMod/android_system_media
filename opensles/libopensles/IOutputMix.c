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

/* OutputMix implementation */

#include "sles_allinclusive.h"

static SLresult IOutputMix_GetDestinationOutputDeviceIDs(SLOutputMixItf self,
   SLint32 *pNumDevices, SLuint32 *pDeviceIDs)
{
    if (NULL == pNumDevices)
        return SL_RESULT_PARAMETER_INVALID;
    if (NULL != pDeviceIDs) {
        if (1 > *pNumDevices)
            return SL_RESULT_BUFFER_INSUFFICIENT;
        pDeviceIDs[0] = SL_DEFAULTDEVICEID_AUDIOOUTPUT;
    }
    *pNumDevices = 1;
    return SL_RESULT_SUCCESS;
}

static SLresult IOutputMix_RegisterDeviceChangeCallback(SLOutputMixItf self,
    slMixDeviceChangeCallback callback, void *pContext)
{
    IOutputMix *this = (IOutputMix *) self;
    interface_lock_exclusive(this);
    this->mCallback = callback;
    this->mContext = pContext;
    interface_unlock_exclusive(this);
    return SL_RESULT_SUCCESS;
}

static SLresult IOutputMix_ReRoute(SLOutputMixItf self, SLint32 numOutputDevices,
    SLuint32 *pOutputDeviceIDs)
{
    if (1 > numOutputDevices)
        return SL_RESULT_PARAMETER_INVALID;
    if (NULL == pOutputDeviceIDs)
        return SL_RESULT_PARAMETER_INVALID;
    switch (pOutputDeviceIDs[0]) {
    case SL_DEFAULTDEVICEID_AUDIOOUTPUT:
    case DEVICE_ID_HEADSET:
    case DEVICE_ID_HANDSFREE:
        break;
    default:
        return SL_RESULT_PARAMETER_INVALID;
    }
    return SL_RESULT_SUCCESS;
}

static const struct SLOutputMixItf_ IOutputMix_Itf = {
    IOutputMix_GetDestinationOutputDeviceIDs,
    IOutputMix_RegisterDeviceChangeCallback,
    IOutputMix_ReRoute
};

void IOutputMix_init(void *self)
{
    IOutputMix *this = (IOutputMix *) self;
    this->mItf = &IOutputMix_Itf;
    this->mCallback = NULL;
    this->mContext = NULL;
#ifdef USE_OUTPUTMIXEXT
    this->mActiveMask = 0;
    struct Track *track = &this->mTracks[0];
    // FIXME O(n)
    // FIXME magic number
    unsigned i;
    for (i = 0; i < 32; ++i, ++track)
        track->mPlay = NULL;
#endif
}
