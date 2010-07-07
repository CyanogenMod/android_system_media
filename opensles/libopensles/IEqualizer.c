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
    SL_ENTER_INTERFACE

    IEqualizer *this = (IEqualizer *) self;
    interface_lock_poke(this);
    this->mEnabled = SL_BOOLEAN_FALSE != enabled; // normalize
    interface_unlock_poke(this);
    result = SL_RESULT_SUCCESS;

    SL_LEAVE_INTERFACE
}


static SLresult IEqualizer_IsEnabled(SLEqualizerItf self, SLboolean *pEnabled)
{
    SL_ENTER_INTERFACE

    if (NULL == pEnabled) {
        result = SL_RESULT_PARAMETER_INVALID;
    } else {
        IEqualizer *this = (IEqualizer *) self;
        interface_lock_peek(this);
        SLboolean enabled = this->mEnabled;
        interface_unlock_peek(this);
        *pEnabled = enabled;
        result = SL_RESULT_SUCCESS;
    }

    SL_LEAVE_INTERFACE
}


static SLresult IEqualizer_GetNumberOfBands(SLEqualizerItf self, SLuint16 *pNumBands)
{
    SL_ENTER_INTERFACE

    if (NULL == pNumBands) {
        result = SL_RESULT_PARAMETER_INVALID;
    } else {
        IEqualizer *this = (IEqualizer *) self;
        // Note: no lock, but OK because it is const
        *pNumBands = this->mNumBands;
        result = SL_RESULT_SUCCESS;
    }

    SL_LEAVE_INTERFACE
}


static SLresult IEqualizer_GetBandLevelRange(SLEqualizerItf self, SLmillibel *pMin,
    SLmillibel *pMax)
{
    SL_ENTER_INTERFACE

    if (NULL == pMin && NULL == pMax) {
        result = SL_RESULT_PARAMETER_INVALID;
    } else {
        IEqualizer *this = (IEqualizer *) self;
        // Note: no lock, but OK because it is const
        if (NULL != pMin)
            *pMin = this->mBandLevelRangeMin;
        if (NULL != pMax)
            *pMax = this->mBandLevelRangeMax;
        result = SL_RESULT_SUCCESS;
    }

    SL_LEAVE_INTERFACE
}


static SLresult IEqualizer_SetBandLevel(SLEqualizerItf self, SLuint16 band, SLmillibel level)
{
    SL_ENTER_INTERFACE

    IEqualizer *this = (IEqualizer *) self;
    if (!(this->mBandLevelRangeMin <= level && level <= this->mBandLevelRangeMax) ||
        (band >= this->mNumBands)) {
        result = SL_RESULT_PARAMETER_INVALID;
    } else {
        interface_lock_exclusive(this);
        this->mLevels[band] = level;
        this->mPreset = SL_EQUALIZER_UNDEFINED;
        interface_unlock_exclusive(this);
        result = SL_RESULT_SUCCESS;
    }

    SL_LEAVE_INTERFACE
}


static SLresult IEqualizer_GetBandLevel(SLEqualizerItf self, SLuint16 band, SLmillibel *pLevel)
{
    SL_ENTER_INTERFACE

    if (NULL == pLevel) {
        result = SL_RESULT_PARAMETER_INVALID;
    } else {
        IEqualizer *this = (IEqualizer *) self;
        // const, no lock needed
        if (band >= this->mNumBands) {
            result = SL_RESULT_PARAMETER_INVALID;
        } else {
            interface_lock_peek(this);
            SLmillibel level = this->mLevels[band];
            interface_unlock_peek(this);
            *pLevel = level;
            result = SL_RESULT_SUCCESS;
        }
    }

    SL_LEAVE_INTERFACE
}


static SLresult IEqualizer_GetCenterFreq(SLEqualizerItf self, SLuint16 band, SLmilliHertz *pCenter)
{
    SL_ENTER_INTERFACE

    if (NULL == pCenter) {
        result = SL_RESULT_PARAMETER_INVALID;
    } else {
        IEqualizer *this = (IEqualizer *) self;
        if (band >= this->mNumBands) {
            result = SL_RESULT_PARAMETER_INVALID;
        } else {
            // Note: no lock, but OK because it is const
            *pCenter = this->mBands[band].mCenter;
            result = SL_RESULT_SUCCESS;
        }
    }

    SL_LEAVE_INTERFACE
}


static SLresult IEqualizer_GetBandFreqRange(SLEqualizerItf self, SLuint16 band,
    SLmilliHertz *pMin, SLmilliHertz *pMax)
{
    SL_ENTER_INTERFACE

    if (NULL == pMin && NULL == pMax) {
        result = SL_RESULT_PARAMETER_INVALID;
    } else {
        IEqualizer *this = (IEqualizer *) self;
        if (band >= this->mNumBands) {
            result = SL_RESULT_PARAMETER_INVALID;
        } else {
            // Note: no lock, but OK because it is const
            if (NULL != pMin)
                *pMin = this->mBands[band].mMin;
            if (NULL != pMax)
                *pMax = this->mBands[band].mMax;
            result = SL_RESULT_SUCCESS;
        }
    }

    SL_LEAVE_INTERFACE
}


static SLresult IEqualizer_GetBand(SLEqualizerItf self, SLmilliHertz frequency, SLuint16 *pBand)
{
    SL_ENTER_INTERFACE

    if (NULL == pBand) {
        result = SL_RESULT_PARAMETER_INVALID;
    } else {
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
        result = SL_RESULT_SUCCESS;
    }

    SL_LEAVE_INTERFACE
}


static SLresult IEqualizer_GetCurrentPreset(SLEqualizerItf self, SLuint16 *pPreset)
{
    SL_ENTER_INTERFACE

    if (NULL == pPreset) {
        result = SL_RESULT_PARAMETER_INVALID;
    } else {
        IEqualizer *this = (IEqualizer *) self;
        interface_lock_shared(this);
        SLuint16 preset = this->mPreset;
        interface_unlock_shared(this);
        *pPreset = preset;
        result = SL_RESULT_SUCCESS;
    }

    SL_LEAVE_INTERFACE
}


static SLresult IEqualizer_UsePreset(SLEqualizerItf self, SLuint16 index)
{
    SL_ENTER_INTERFACE

    IEqualizer *this = (IEqualizer *) self;
    if (index >= this->mNumPresets) {
        result = SL_RESULT_PARAMETER_INVALID;
    } else {
        interface_lock_exclusive(this);
        SLuint16 band;
        for (band = 0; band < this->mNumBands; ++band)
            this->mLevels[band] = EqualizerPresets[index].mLevels[band];
        this->mPreset = index;
        interface_unlock_exclusive(this);
        result = SL_RESULT_SUCCESS;
    }

    SL_LEAVE_INTERFACE
}


static SLresult IEqualizer_GetNumberOfPresets(SLEqualizerItf self, SLuint16 *pNumPresets)
{
    SL_ENTER_INTERFACE

    if (NULL == pNumPresets) {
        result = SL_RESULT_PARAMETER_INVALID;
    } else {
        IEqualizer *this = (IEqualizer *) self;
        // Note: no lock, but OK because it is const
        *pNumPresets = this->mNumPresets;
        result = SL_RESULT_SUCCESS;
    }

    SL_LEAVE_INTERFACE
}


static SLresult IEqualizer_GetPresetName(SLEqualizerItf self, SLuint16 index, const SLchar **ppName)
{
    SL_ENTER_INTERFACE

    if (NULL == ppName) {
        result = SL_RESULT_PARAMETER_INVALID;
    } else {
        IEqualizer *this = (IEqualizer *) self;
        if (index >= this->mNumPresets) {
            result = SL_RESULT_PARAMETER_INVALID;
        } else {
            *ppName = (SLchar *) this->mPresets[index].mName;
            result = SL_RESULT_SUCCESS;
        }
    }

    SL_LEAVE_INTERFACE
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
