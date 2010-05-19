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

#define DEVICE_ID_HEADSET 1
#define DEVICE_ID_HANDSFREE 2

extern const struct AudioInput_id_descriptor {
    SLuint32 id;
    const SLAudioInputDescriptor *descriptor;
} AudioInput_id_descriptors[];

extern const struct AudioOutput_id_descriptor {
    SLuint32 id;
    const SLAudioOutputDescriptor *descriptor;
} AudioOutput_id_descriptors[];

extern const struct LED_id_descriptor {
    SLuint32 id;
    const SLLEDDescriptor *descriptor;
} LED_id_descriptors[];

extern const struct Vibra_id_descriptor {
    SLuint32 id;
    const SLVibraDescriptor *descriptor;
} Vibra_id_descriptors[];
