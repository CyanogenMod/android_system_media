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

#ifdef __cplusplus
extern "C" {
#endif

// OpenMAX AL 1.0.1
const XAInterfaceID XA_IID_ENGINE = (XAInterfaceID) &SL_IID_array[MPH_XAENGINE];
const XAInterfaceID XA_IID_PLAY = (XAInterfaceID) &SL_IID_array[MPH_XAPLAY];
const XAInterfaceID XA_IID_STREAMINFORMATION =
        (XAInterfaceID) &SL_IID_array[MPH_XASTREAMINFORMATION];
const XAInterfaceID XA_IID_VOLUME = (XAInterfaceID) &SL_IID_array[MPH_XAVOLUME];

// OpenMAX AL 1.0.1 Android API level 10 extended interfaces
const XAInterfaceID XA_IID_ANDROIDBUFFERQUEUE =
        (XAInterfaceID) &SL_IID_array[MPH_ANDROIDBUFFERQUEUE]; //same as SL

#ifdef __cplusplus
}
#endif
