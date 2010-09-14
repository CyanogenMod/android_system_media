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

extern SLresult android_audioRecorder_checkSourceSinkSupport(CAudioRecorder* ar);

extern SLresult android_audioRecorder_create(CAudioRecorder* ar);

extern SLresult android_audioRecorder_setConfig(CAudioRecorder* ar, const SLchar *configKey,
        const void *pConfigValue, SLuint32 valueSize);

extern SLresult android_audioRecorder_getConfig(CAudioRecorder* ar, const SLchar *configKey,
        SLuint32* pValueSize, void *pConfigValue);

extern SLresult android_audioRecorder_realize(CAudioRecorder* ar, SLboolean async);

extern void android_audioRecorder_destroy(CAudioRecorder* ar);

extern void android_audioRecorder_setRecordState(CAudioRecorder* ar, SLuint32 state);
