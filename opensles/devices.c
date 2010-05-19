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

/* Device table (change this when you port!) */

static const SLAudioInputDescriptor AudioInputDescriptor_mic = {
    (SLchar *) "mic",            // deviceName
    SL_DEVCONNECTION_INTEGRATED, // deviceConnection
    SL_DEVSCOPE_ENVIRONMENT,     // deviceScope
    SL_DEVLOCATION_HANDSET,      // deviceLocation
    SL_BOOLEAN_TRUE,             // isForTelephony
    SL_SAMPLINGRATE_44_1,        // minSampleRate
    SL_SAMPLINGRATE_44_1,        // maxSampleRate
    SL_BOOLEAN_TRUE,             // isFreqRangeContinuous
    NULL,                        // samplingRatesSupported
    0,                           // numOfSamplingRatesSupported
    1                            // maxChannels
};

const struct AudioInput_id_descriptor AudioInput_id_descriptors[] = {
    {SL_DEFAULTDEVICEID_AUDIOINPUT, &AudioInputDescriptor_mic},
    {0, NULL}
};

static const SLAudioOutputDescriptor AudioOutputDescriptor_speaker = {
    (SLchar *) "speaker",        // deviceName
    SL_DEVCONNECTION_INTEGRATED, // deviceConnection
    SL_DEVSCOPE_USER,            // deviceScope
    SL_DEVLOCATION_HEADSET,      // deviceLocation
    SL_BOOLEAN_TRUE,             // isForTelephony
    SL_SAMPLINGRATE_44_1,        // minSamplingRate
    SL_SAMPLINGRATE_44_1,        // maxSamplingRate
    SL_BOOLEAN_TRUE,             // isFreqRangeContinuous
    NULL,                        // samplingRatesSupported
    0,                           // numOfSamplingRatesSupported
    2                            // maxChannels
};

static const SLAudioOutputDescriptor AudioOutputDescriptor_headset = {
    (SLchar *) "headset",
    SL_DEVCONNECTION_ATTACHED_WIRED,
    SL_DEVSCOPE_USER,
    SL_DEVLOCATION_HEADSET,
    SL_BOOLEAN_FALSE,
    SL_SAMPLINGRATE_44_1,
    SL_SAMPLINGRATE_44_1,
    SL_BOOLEAN_TRUE,
    NULL,
    0,
    2
};

static const SLAudioOutputDescriptor AudioOutputDescriptor_handsfree = {
    (SLchar *) "handsfree",
    SL_DEVCONNECTION_INTEGRATED,
    SL_DEVSCOPE_ENVIRONMENT,
    SL_DEVLOCATION_HANDSET,
    SL_BOOLEAN_FALSE,
    SL_SAMPLINGRATE_44_1,
    SL_SAMPLINGRATE_44_1,
    SL_BOOLEAN_TRUE,
    NULL,
    0,
    2
};

const struct AudioOutput_id_descriptor AudioOutput_id_descriptors[] = {
    {SL_DEFAULTDEVICEID_AUDIOOUTPUT, &AudioOutputDescriptor_speaker},
    {DEVICE_ID_HEADSET, &AudioOutputDescriptor_headset},
    {DEVICE_ID_HANDSFREE, &AudioOutputDescriptor_handsfree},
    {0, NULL}
};

static const SLLEDDescriptor SLLEDDescriptor_default = {
    32, // ledCount
    0,  // primaryLED
    ~0  // colorMask
};

const struct LED_id_descriptor LED_id_descriptors[] = {
    {SL_DEFAULTDEVICEID_LED, &SLLEDDescriptor_default},
    {0, NULL}
};

static const SLVibraDescriptor SLVibraDescriptor_default = {
    SL_BOOLEAN_TRUE, // supportsFrequency
    SL_BOOLEAN_TRUE, // supportsIntensity
    20000,           // minFrequency
    100000           // maxFrequency
};

const struct Vibra_id_descriptor Vibra_id_descriptors[] = {
    {SL_DEFAULTDEVICEID_VIBRA, &SLVibraDescriptor_default},
    {0, NULL}
};
