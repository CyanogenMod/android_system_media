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

/* EngineCapabilities implementation */

#include "sles_allinclusive.h"

static SLresult IEngineCapabilities_QuerySupportedProfiles(
    SLEngineCapabilitiesItf self, SLuint16 *pProfilesSupported)
{
    if (NULL == pProfilesSupported)
        return SL_RESULT_PARAMETER_INVALID;
    // This omits the unofficial driver profile
    *pProfilesSupported =
        SL_PROFILES_PHONE | SL_PROFILES_MUSIC | SL_PROFILES_GAME;
    return SL_RESULT_SUCCESS;
}

static SLresult IEngineCapabilities_QueryAvailableVoices(
    SLEngineCapabilitiesItf self, SLuint16 voiceType, SLint16 *pNumMaxVoices,
    SLboolean *pIsAbsoluteMax, SLint16 *pNumFreeVoices)
{
    switch (voiceType) {
    case SL_VOICETYPE_2D_AUDIO:
    case SL_VOICETYPE_MIDI:
    case SL_VOICETYPE_3D_AUDIO:
    case SL_VOICETYPE_3D_MIDIOUTPUT:
        break;
    default:
        return SL_RESULT_PARAMETER_INVALID;
    }
    if (NULL != pNumMaxVoices)
        *pNumMaxVoices = 32;
    if (NULL != pIsAbsoluteMax)
        *pIsAbsoluteMax = SL_BOOLEAN_TRUE;
    if (NULL != pNumFreeVoices)
        *pNumFreeVoices = 32;
    return SL_RESULT_SUCCESS;
}

static SLresult IEngineCapabilities_QueryNumberOfMIDISynthesizers(
    SLEngineCapabilitiesItf self, SLint16 *pNum)
{
    if (NULL == pNum)
        return SL_RESULT_PARAMETER_INVALID;
    *pNum = 1;
    return SL_RESULT_SUCCESS;
}

static SLresult IEngineCapabilities_QueryAPIVersion(SLEngineCapabilitiesItf self,
    SLint16 *pMajor, SLint16 *pMinor, SLint16 *pStep)
{
    if (!(NULL != pMajor && NULL != pMinor && NULL != pStep))
        return SL_RESULT_PARAMETER_INVALID;
    *pMajor = 1;
    *pMinor = 0;
    *pStep = 1;
    return SL_RESULT_SUCCESS;
}

static SLresult IEngineCapabilities_QueryLEDCapabilities(
    SLEngineCapabilitiesItf self, SLuint32 *pIndex, SLuint32 *pLEDDeviceID,
    SLLEDDescriptor *pDescriptor)
{
    const struct LED_id_descriptor *id_descriptor = LED_id_descriptors;
    while (NULL != id_descriptor->descriptor)
        ++id_descriptor;
    // FIXME should cache this value
    SLuint32 maxIndex = id_descriptor - LED_id_descriptors;
    SLuint32 index;
    if (NULL != pIndex) {
        if (NULL != pLEDDeviceID || NULL != pDescriptor) {
            index = *pIndex;
            if (index >= maxIndex)
                return SL_RESULT_PARAMETER_INVALID;
            id_descriptor = &LED_id_descriptors[index];
            if (NULL != pLEDDeviceID)
                *pLEDDeviceID = id_descriptor->id;
            if (NULL != pDescriptor)
                *pDescriptor = *id_descriptor->descriptor;
        }
        /* FIXME else? */
        *pIndex = maxIndex;
        return SL_RESULT_SUCCESS;
    } else {
        if (NULL != pLEDDeviceID && NULL != pDescriptor) {
            SLuint32 id = *pLEDDeviceID;
            for (index = 0; index < maxIndex; ++index) {
                id_descriptor = &LED_id_descriptors[index];
                if (id == id_descriptor->id) {
                    *pDescriptor = *id_descriptor->descriptor;
                    return SL_RESULT_SUCCESS;
                }
            }
        }
        return SL_RESULT_PARAMETER_INVALID;
    }
}

static SLresult IEngineCapabilities_QueryVibraCapabilities(
    SLEngineCapabilitiesItf self, SLuint32 *pIndex, SLuint32 *pVibraDeviceID,
    SLVibraDescriptor *pDescriptor)
{
    const struct Vibra_id_descriptor *id_descriptor = Vibra_id_descriptors;
    while (NULL != id_descriptor->descriptor)
        ++id_descriptor;
    // FIXME should cache this value
    SLuint32 maxIndex = id_descriptor - Vibra_id_descriptors;
    SLuint32 index;
    if (NULL != pIndex) {
        if (NULL != pVibraDeviceID || NULL != pDescriptor) {
            index = *pIndex;
            if (index >= maxIndex)
                return SL_RESULT_PARAMETER_INVALID;
            id_descriptor = &Vibra_id_descriptors[index];
            if (NULL != pVibraDeviceID)
                *pVibraDeviceID = id_descriptor->id;
            if (NULL != pDescriptor)
                *pDescriptor = *id_descriptor->descriptor;
        }
        /* FIXME else? */
        *pIndex = maxIndex;
        return SL_RESULT_SUCCESS;
    } else {
        if (NULL != pVibraDeviceID && NULL != pDescriptor) {
            SLuint32 id = *pVibraDeviceID;
            for (index = 0; index < maxIndex; ++index) {
                id_descriptor = &Vibra_id_descriptors[index];
                if (id == id_descriptor->id) {
                    *pDescriptor = *id_descriptor->descriptor;
                    return SL_RESULT_SUCCESS;
                }
            }
        }
        return SL_RESULT_PARAMETER_INVALID;
    }
    return SL_RESULT_SUCCESS;
}

static SLresult IEngineCapabilities_IsThreadSafe(SLEngineCapabilitiesItf self,
    SLboolean *pIsThreadSafe)
{
    if (NULL == pIsThreadSafe)
        return SL_RESULT_PARAMETER_INVALID;
    IEngineCapabilities *this =
        (IEngineCapabilities *) self;
    *pIsThreadSafe = this->mThreadSafe;
    return SL_RESULT_SUCCESS;
}

static const struct SLEngineCapabilitiesItf_ IEngineCapabilities_Itf = {
    IEngineCapabilities_QuerySupportedProfiles,
    IEngineCapabilities_QueryAvailableVoices,
    IEngineCapabilities_QueryNumberOfMIDISynthesizers,
    IEngineCapabilities_QueryAPIVersion,
    IEngineCapabilities_QueryLEDCapabilities,
    IEngineCapabilities_QueryVibraCapabilities,
    IEngineCapabilities_IsThreadSafe
};

void IEngineCapabilities_init(void *self)
{
    IEngineCapabilities *this = (IEngineCapabilities *) self;
    this->mItf = &IEngineCapabilities_Itf;
    // mThreadSafe is initialized in slCreateEngine
}
