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

#if !defined(ANDROID) || defined(USE_BACKPORT)
// FIXME needs to be defined in a platform-dependent manner
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
#endif


static SLresult IEqualizer_SetEnabled(SLEqualizerItf self, SLboolean enabled)
{
    SL_ENTER_INTERFACE

    IEqualizer *this = (IEqualizer *) self;
    interface_lock_exclusive(this);
    this->mEnabled = SL_BOOLEAN_FALSE != enabled; // normalize
#if !defined(ANDROID) || defined(USE_BACKPORT)
    result = SL_RESULT_SUCCESS;
#else
    bool fxEnabled = this->mEqEffect->getEnabled();
    // the audio effect framework will return an error if you set the same "enabled" state as
    // the one the effect is currently in, so we only act on state changes
    if (fxEnabled != (enabled == SL_BOOLEAN_TRUE)) {
        // state change
        android::status_t status = this->mEqEffect->setEnabled(this->mEnabled == SL_BOOLEAN_TRUE);
        result = android_fx_statusToResult(status);
    } else {
        result = SL_RESULT_SUCCESS;
    }
#endif
    interface_unlock_exclusive(this);

    SL_LEAVE_INTERFACE
}


static SLresult IEqualizer_IsEnabled(SLEqualizerItf self, SLboolean *pEnabled)
{
    SL_ENTER_INTERFACE

    if (NULL == pEnabled) {
        result = SL_RESULT_PARAMETER_INVALID;
    } else {
        IEqualizer *this = (IEqualizer *) self;
        interface_lock_shared(this);
        SLboolean enabled = this->mEnabled;
        interface_unlock_shared(this);
#if !defined(ANDROID) || defined(USE_BACKPORT)
        *pEnabled = enabled;
        result = SL_RESULT_SUCCESS;
#else
        bool fxEnabled = this->mEqEffect->getEnabled();
        *pEnabled = fxEnabled ? SL_BOOLEAN_TRUE : SL_BOOLEAN_FALSE;
        result = SL_RESULT_SUCCESS;
#endif
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
#if !defined(ANDROID) || defined(USE_BACKPORT)
        this->mLevels[band] = level;
        this->mPreset = SL_EQUALIZER_UNDEFINED;
        result = SL_RESULT_SUCCESS;
#else
        android::status_t status =
                android_eq_setParam(this->mEqEffect, EQ_PARAM_BAND_LEVEL, band, &level);
        result = android_fx_statusToResult(status);
#endif
        interface_unlock_exclusive(this);
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
            SLmillibel level;
            interface_lock_shared(this);
#if !defined(ANDROID) || defined(USE_BACKPORT)
            level = this->mLevels[band];
            result = SL_RESULT_SUCCESS;
#else
            android::status_t status =
                    android_eq_getParam(this->mEqEffect, EQ_PARAM_BAND_LEVEL, band, &level);
            result = android_fx_statusToResult(status);
#endif
            interface_unlock_shared(this);
            *pLevel = level;
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
#if !defined(ANDROID) || defined(USE_BACKPORT)
            // Note: no lock, but OK because it is const
            *pCenter = this->mBands[band].mCenter;
            result = SL_RESULT_SUCCESS;
#else
            SLmilliHertz center;
            interface_lock_shared(this);
            android::status_t status =
                    android_eq_getParam(this->mEqEffect, EQ_PARAM_CENTER_FREQ, band, &center);
            interface_unlock_shared(this);
            result = android_fx_statusToResult(status);
            *pCenter = center;
#endif
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
#if !defined(ANDROID) || defined(USE_BACKPORT)
            // Note: no lock, but OK because it is const
            if (NULL != pMin)
                *pMin = this->mBands[band].mMin;
            if (NULL != pMax)
                *pMax = this->mBands[band].mMax;
            result = SL_RESULT_SUCCESS;
#else
            SLmilliHertz range[2]; // SLmilliHertz is SLuint32
            interface_lock_shared(this);
            android::status_t status =
                    android_eq_getParam(this->mEqEffect, EQ_PARAM_BAND_FREQ_RANGE, band, range);
            interface_unlock_shared(this);
            result = android_fx_statusToResult(status);
            if (NULL != pMin) {
                *pMin = range[0];
            }
            if (NULL != pMax) {
                *pMax = range[1];
            }
#endif
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
#if !defined(ANDROID) || defined(USE_BACKPORT)
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
#else
        uint32_t band;
        interface_lock_shared(this);
        android::status_t status =
            android_eq_getParam(this->mEqEffect, EQ_PARAM_GET_BAND, frequency, &band);
        interface_unlock_shared(this);
        result = android_fx_statusToResult(status);
        *pBand = (SLuint16)band;
#endif
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
#if !defined(ANDROID) || defined(USE_BACKPORT)
        SLuint16 preset = this->mPreset;
        interface_unlock_shared(this);
        *pPreset = preset;
        result = SL_RESULT_SUCCESS;
#else
        int32_t preset;
        android::status_t status =
                android_eq_getParam(this->mEqEffect, EQ_PARAM_CUR_PRESET, 0, &preset);
        interface_unlock_shared(this);
        result = android_fx_statusToResult(status);
        if (preset < 0) {
            *pPreset = SL_EQUALIZER_UNDEFINED;
        } else {
            *pPreset = (SLuint16) preset;
        }
#endif

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
#if !defined(ANDROID) || defined(USE_BACKPORT)
        SLuint16 band;
        for (band = 0; band < this->mNumBands; ++band)
            this->mLevels[band] = EqualizerPresets[index].mLevels[band];
        this->mPreset = index;
        interface_unlock_exclusive(this);
        result = SL_RESULT_SUCCESS;
#else
        int32_t preset = index;
        android::status_t status =
                android_eq_setParam(this->mEqEffect, EQ_PARAM_CUR_PRESET, 0, &preset);
        interface_unlock_shared(this);
        result = android_fx_statusToResult(status);
#endif
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
#if !defined(ANDROID) || defined(USE_BACKPORT)
        *pNumPresets = this->mNumPresets;
#else
        *pNumPresets = this->mThis->mEngine->mEqNumPresets;
#endif
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
#if !defined(ANDROID) || defined(USE_BACKPORT)
        if (index >= this->mNumPresets) {
            result = SL_RESULT_PARAMETER_INVALID;
        } else {
            *ppName = (SLchar *) this->mPresets[index].mName;
            result = SL_RESULT_SUCCESS;
        }
#else
        if (index >= this->mThis->mEngine->mEqNumPresets) {
            result = SL_RESULT_PARAMETER_INVALID;
        } else {
            // FIXME This gives the application access to the library memory space and is
            //  identified as a security issue. This is being addressed by the OpenSL ES WG
            *ppName = (SLchar *) this->mThis->mEngine->mEqPresetNames[index];
            result = SL_RESULT_SUCCESS;
        }
#endif
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
#if (0 < MAX_EQ_BANDS)
    unsigned band;
    for (band = 0; band < MAX_EQ_BANDS; ++band)
        this->mLevels[band] = 0;
#endif
    // const fields
    this->mNumPresets = 0;
    this->mNumBands = 0;
#if !defined(ANDROID) || defined(USE_BACKPORT)
    this->mBands = EqualizerBands;
    this->mPresets = EqualizerPresets;
#endif
    this->mBandLevelRangeMin = 0;
    this->mBandLevelRangeMax = 0;

#if defined(ANDROID) && !defined(USE_BACKPORT)
    // Initialize the EQ settings based on the platform effect, if available
    uint32_t numEffects = 0;
    effect_descriptor_t descriptor;
    bool foundEq = false;
    //   any effects?
    android::status_t res = android::AudioEffect::queryNumberEffects(&numEffects);
    if (android::NO_ERROR != res) {
        SL_LOGE("IEqualizer_init: unable to find any effects.");
        goto effectError;
    }
    //   EQ in the effects?
    for (uint32_t i=0 ; i < numEffects ; i++) {
        res = android::AudioEffect::queryEffect(i, &descriptor);
        if ((android::NO_ERROR == res) &&
                (0 == memcmp(SL_IID_EQUALIZER, &descriptor.type, sizeof(effect_uuid_t)))) {
            SL_LOGV("found effect %d %s", i, descriptor.name);
            foundEq = true;
            break;
        }
    }
    if (foundEq) {
        memcpy(&this->mEqDescriptor, &descriptor, sizeof(effect_descriptor_t));
    } else {
        SL_LOGE("IEqualizer_init: unable to find an EQ implementation.");
        goto effectError;
    }

    return;

effectError:
    this->mNumPresets = 0;
    this->mNumBands = 0;
    this->mBandLevelRangeMin = 0;
    this->mBandLevelRangeMax = 0;
    memset(&this->mEqDescriptor, 0, sizeof(effect_descriptor_t));
#endif
}
