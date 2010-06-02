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

/* Equalizer implementation */

#include "sles_allinclusive.h"

// FIXME move to platform-specific configuration

#define MAX_EQ_PRESETS 3

static const struct EqualizerBand EqualizerBands[MAX_EQ_BANDS] = {
    {1000, 1500, 2000},
    {2000, 3000, 4000},
    {4000, 5500, 7000},
    {7000, 8000, 9000}
};

static const struct EqualizerPreset {
    const char *mName;
    SLmillibel mLevels[MAX_EQ_BANDS];
} EqualizerPresets[MAX_EQ_PRESETS] = {
    {"Default", {0, 0, 0, 0}},
    {"Bass", {500, 200, 100, 0}},
    {"Treble", {0, 100, 200, 500}}
};

static SLresult IEqualizer_SetEnabled(SLEqualizerItf self, SLboolean enabled)
{
    IEqualizer *this = (IEqualizer *) self;
    interface_lock_poke(this);
    this->mEnabled = enabled;
    interface_unlock_poke(this);
    return SL_RESULT_SUCCESS;
}

static SLresult IEqualizer_IsEnabled(SLEqualizerItf self, SLboolean *pEnabled)
{
    if (NULL == pEnabled)
        return SL_RESULT_PARAMETER_INVALID;
    IEqualizer *this = (IEqualizer *) self;
    interface_lock_peek(this);
    SLboolean enabled = this->mEnabled;
    interface_unlock_peek(this);
    *pEnabled = enabled;
    return SL_RESULT_SUCCESS;
}

static SLresult IEqualizer_GetNumberOfBands(SLEqualizerItf self, SLuint16 *pNumBands)
{
    if (NULL == pNumBands)
        return SL_RESULT_PARAMETER_INVALID;
    IEqualizer *this = (IEqualizer *) self;
    // Note: no lock, but OK because it is const
    *pNumBands = this->mNumBands;
    return SL_RESULT_SUCCESS;
}

static SLresult IEqualizer_GetBandLevelRange(SLEqualizerItf self,
    SLmillibel *pMin, SLmillibel *pMax)
{
    if (NULL == pMin && NULL == pMax)
        return SL_RESULT_PARAMETER_INVALID;
    IEqualizer *this = (IEqualizer *) self;
    // Note: no lock, but OK because it is const
    if (NULL != pMin)
        *pMin = this->mBandLevelRangeMin;
    if (NULL != pMax)
        *pMax = this->mBandLevelRangeMax;
    return SL_RESULT_SUCCESS;
}

static SLresult IEqualizer_SetBandLevel(SLEqualizerItf self, SLuint16 band, SLmillibel level)
{
    IEqualizer *this = (IEqualizer *) self;
    if (band >= this->mNumBands)
        return SL_RESULT_PARAMETER_INVALID;
    interface_lock_exclusive(this);
    this->mLevels[band] = level;
    this->mPreset = SL_EQUALIZER_UNDEFINED;
    interface_unlock_exclusive(this);
    return SL_RESULT_SUCCESS;
}

static SLresult IEqualizer_GetBandLevel(SLEqualizerItf self, SLuint16 band,
    SLmillibel *pLevel)
{
    if (NULL == pLevel)
        return SL_RESULT_PARAMETER_INVALID;
    IEqualizer *this = (IEqualizer *) self;
    if (band >= this->mNumBands)
        return SL_RESULT_PARAMETER_INVALID;
    interface_lock_peek(this);
    SLmillibel level = this->mLevels[band];
    interface_unlock_peek(this);
    *pLevel = level;
    return SL_RESULT_SUCCESS;
}

static SLresult IEqualizer_GetCenterFreq(SLEqualizerItf self, SLuint16 band, SLmilliHertz *pCenter)
{
    if (NULL == pCenter)
        return SL_RESULT_PARAMETER_INVALID;
    IEqualizer *this = (IEqualizer *) self;
    if (band >= this->mNumBands)
        return SL_RESULT_PARAMETER_INVALID;
    // Note: no lock, but OK because it is const
    *pCenter = this->mBands[band].mCenter;
    return SL_RESULT_SUCCESS;
}

static SLresult IEqualizer_GetBandFreqRange(SLEqualizerItf self, SLuint16 band,
    SLmilliHertz *pMin, SLmilliHertz *pMax)
{
    if (NULL == pMin && NULL == pMax)
        return SL_RESULT_PARAMETER_INVALID;
    IEqualizer *this = (IEqualizer *) self;
    if (band >= this->mNumBands)
        return SL_RESULT_PARAMETER_INVALID;
    // Note: no lock, but OK because it is const
    if (NULL != pMin)
        *pMin = this->mBands[band].mMin;
    if (NULL != pMax)
        *pMax = this->mBands[band].mMax;
    return SL_RESULT_SUCCESS;
}

static SLresult IEqualizer_GetBand(SLEqualizerItf self, SLmilliHertz frequency, SLuint16 *pBand)
{
    if (NULL == pBand)
        return SL_RESULT_PARAMETER_INVALID;
    IEqualizer *this = (IEqualizer *) self;
    // search for band whose center frequency has the closest ratio to 1.0
    // assumes bands are unsorted (a pessimistic assumption)
    // assumes bands can overlap (a pessimistic assumption)
    // assumes a small number of bands, so no need for a fancier algorithm
    const struct EqualizerBand *band;
    float floatFreq = (float) frequency;
    float bestRatio = 0.0;
    SLuint16 bestBand = SL_EQUALIZER_UNDEFINED;
    for (band = this->mBands; band < &this->mBands[this->mNumBands]; ++band) {
        if (!(band->mMin <= frequency && frequency <= band->mMax))
            continue;
        assert(band->mMin <= band->mCenter && band->mCenter <= band->mMax);
        assert(band->mCenter != 0);
        float ratio = frequency <= band->mCenter ?
            floatFreq / band->mCenter : band->mCenter / floatFreq;
        if (ratio > bestRatio) {
            bestRatio = ratio;
            bestBand = band - this->mBands;
        }
    }
    *pBand = band - this->mBands;
    return SL_RESULT_SUCCESS;
}

static SLresult IEqualizer_GetCurrentPreset(SLEqualizerItf self,
    SLuint16 *pPreset)
{
    if (NULL == pPreset)
        return SL_RESULT_PARAMETER_INVALID;
    IEqualizer *this = (IEqualizer *) self;
    interface_lock_shared(this);
    SLuint16 preset = this->mPreset;
    interface_unlock_shared(this);
    *pPreset = preset;
    return SL_RESULT_SUCCESS;
}

static SLresult IEqualizer_UsePreset(SLEqualizerItf self, SLuint16 index)
{
    IEqualizer *this = (IEqualizer *) self;
    if (index >= this->mNumPresets)
        return SL_RESULT_PARAMETER_INVALID;
    interface_lock_exclusive(this);
    SLuint16 band;
    for (band = 0; band < this->mNumBands; ++band)
        this->mLevels[band] = EqualizerPresets[index].mLevels[band];
    this->mPreset = index;
    interface_unlock_exclusive(this);
    return SL_RESULT_SUCCESS;
}

static SLresult IEqualizer_GetNumberOfPresets(SLEqualizerItf self,
    SLuint16 *pNumPresets)
{
    if (NULL == pNumPresets)
        return SL_RESULT_PARAMETER_INVALID;
    IEqualizer *this = (IEqualizer *) self;
    // Note: no lock, but OK because it is const
    *pNumPresets = this->mNumPresets;
    return SL_RESULT_SUCCESS;
}

static SLresult IEqualizer_GetPresetName(SLEqualizerItf self, SLuint16 index,
    const SLchar ** ppName)
{
    if (NULL == ppName)
        return SL_RESULT_PARAMETER_INVALID;
    IEqualizer *this = (IEqualizer *) self;
    if (index >= this->mNumPresets)
        return SL_RESULT_PARAMETER_INVALID;
    *ppName = (SLchar *) this->mPresets[index].mName;
    return SL_RESULT_SUCCESS;
}

static const struct SLEqualizerItf_ IEqualizer_Itf = {
    IEqualizer_SetEnabled,
    IEqualizer_IsEnabled,
    IEqualizer_GetNumberOfBands,
    IEqualizer_GetBandLevelRange,
    IEqualizer_SetBandLevel,
    IEqualizer_GetBandLevel,
    IEqualizer_GetCenterFreq,
    IEqualizer_GetBandFreqRange,
    IEqualizer_GetBand,
    IEqualizer_GetCurrentPreset,
    IEqualizer_UsePreset,
    IEqualizer_GetNumberOfPresets,
    IEqualizer_GetPresetName
};

void IEqualizer_init(void *self)
{
    IEqualizer *this = (IEqualizer *) self;
    this->mItf = &IEqualizer_Itf;
    this->mEnabled = SL_BOOLEAN_FALSE;
    this->mPreset = SL_EQUALIZER_UNDEFINED;
    unsigned band;
    for (band = 0; band < MAX_EQ_BANDS; ++band)
        this->mLevels[band] = 0;
    // const fields
    this->mNumPresets = MAX_EQ_PRESETS;
    this->mNumBands = MAX_EQ_BANDS;
    this->mBands = EqualizerBands;
    this->mPresets = EqualizerPresets;
    this->mBandLevelRangeMin = 0;
    this->mBandLevelRangeMax = 1000;
}
