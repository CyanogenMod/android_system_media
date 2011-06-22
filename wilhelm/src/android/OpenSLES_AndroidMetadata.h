/*
 * Copyright (C) 2011 The Android Open Source Project
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


/**
 * Additional metadata keys
 *   the ANDROID_KEY_PCMFORMAT_* keys follow the fields of the SLDataFormat_PCM struct, and as such
 *     all corresponding values match SLuint32 size, and field definitions.
 * FIXME these values are candidates to be included in OpenSLES_Android.h as official metadata
 *     keys supported on the platform.
 */
#define ANDROID_KEY_PCMFORMAT_NUMCHANNELS   "AndroidPcmFormatNumChannels"
#define ANDROID_KEY_PCMFORMAT_SAMPLESPERSEC "AndroidPcmFormatSamplesPerSec"
#define ANDROID_KEY_PCMFORMAT_BITSPERSAMPLE "AndroidPcmFormatBitsPerSample"
#define ANDROID_KEY_PCMFORMAT_CONTAINERSIZE "AndroidPcmFormatContainerSize"
#define ANDROID_KEY_PCMFORMAT_CHANNELMASK   "AndroidPcmFormatChannelMask"
#define ANDROID_KEY_PCMFORMAT_ENDIANNESS    "AndroidPcmFormatEndianness"
