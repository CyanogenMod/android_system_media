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

#include "sles_allinclusive.h"
//#include "math.h"
//#include "utils/RefBase.h"


SLresult android_outputMix_create(COutputMix *om) {
    SLresult result = SL_RESULT_SUCCESS;
    SL_LOGV("om=%p", om);

    return result;
}


SLresult android_outputMix_realize(COutputMix *om, SLboolean async) {
    SLresult result = SL_RESULT_SUCCESS;
    SL_LOGV("om=%p", om);

    // initialize effects
    // initialize EQ
    if (memcmp(SL_IID_EQUALIZER, &om->mEqualizer.mEqDescriptor.type,
            sizeof(effect_uuid_t)) == 0) {
        android_eq_init(android::AudioSystem::SESSION_OUTPUT_MIX /*sessionId*/, &om->mEqualizer);
    }
    // initialize BassBoost
    if (memcmp(SL_IID_BASSBOOST, &om->mBassBoost.mBassBoostDescriptor.type,
            sizeof(effect_uuid_t)) == 0) {
        android_bb_init(android::AudioSystem::SESSION_OUTPUT_MIX /*sessionId*/, &om->mBassBoost);
    }
    // initialize PresetReverb
    if (memcmp(SL_IID_PRESETREVERB, &om->mPresetReverb.mPresetReverbDescriptor.type,
            sizeof(effect_uuid_t)) == 0) {
        android_prev_init(&om->mPresetReverb);
    }
    // initialize EnvironmentalReverb
    if (memcmp(SL_IID_ENVIRONMENTALREVERB,
            &om->mEnvironmentalReverb.mEnvironmentalReverbDescriptor.type,
            sizeof(effect_uuid_t)) == 0) {
        android_erev_init(&om->mEnvironmentalReverb);
    }
    // initialize Virtualizer
    if (memcmp(SL_IID_VIRTUALIZER, &om->mVirtualizer.mVirtualizerDescriptor.type,
            sizeof(effect_uuid_t)) == 0) {
        android_virt_init(android::AudioSystem::SESSION_OUTPUT_MIX /*sessionId*/,
                &om->mVirtualizer);
    }

    return result;
}


SLresult android_outputMix_destroy(COutputMix *om) {
    SLresult result = SL_RESULT_SUCCESS;
    SL_LOGV("om=%p", om);


#ifndef USE_BACKPORT
    // FIXME this shouldn't have to be done here, there should be an "interface destroy" hook,
    //       just like there is an interface init hook, to avoid memory leaks.
    om->mEqualizer.mEqEffect.clear();
    om->mBassBoost.mBassBoostEffect.clear();
    om->mVirtualizer.mVirtualizerEffect.clear();
    om->mEnvironmentalReverb.mEnvironmentalReverbEffect.clear();
    om->mPresetReverb.mPresetReverbEffect.clear();
    if (!om->mAndroidEffect.mEffects.isEmpty()) {
        for (size_t i = 0 ; i < om->mAndroidEffect.mEffects.size() ; i++) {
            delete om->mAndroidEffect.mEffects.valueAt(i);
        }
        om->mAndroidEffect.mEffects.clear();
    }
#endif

    return result;
}

