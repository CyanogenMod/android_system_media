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

/*
 * Platform independent check that returns
 *    SL_RESULT_PARAMETER_INVALID if an invalid or null parameter is passed
 *    SL_RESULT_SUCCESS if the given source and sinks can be by the audio player
 */

SLresult sles_checkAudioPlayerSourceSink(const SLDataSource *pAudioSrc, const SLDataSink *pAudioSnk);
