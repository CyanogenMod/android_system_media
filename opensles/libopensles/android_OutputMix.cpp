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
        android_eq_init(0/*sessionId*/, &om->mEqualizer);
    }

    return result;
}


SLresult android_outputMix_destroy(COutputMix *om) {
    SLresult result = SL_RESULT_SUCCESS;
    SL_LOGV("om=%p", om);

    return result;
}

