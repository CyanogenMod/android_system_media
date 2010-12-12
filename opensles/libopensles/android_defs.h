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


/**
 * Used to define the mapping from an OpenSL ES audio player to an Android
 * media framework object
 */
enum AndroidObject_type {
    INVALID_TYPE     =-1,
    A_PLR_URI_FD     = 0, // audio player, compressed data, URI or FD data source
    A_PLR_PCM_BQ     = 1, // audio player, PCM, buffer queue data source
    A_PLR_TS_ABQ     = 2, // audio player, transport stream, Android buffer queue data source
    AV_PLR_TS_ABQ    = 3, // audio video player, transport stream, Android buffer queue data source
    NUM_AUDIOPLAYER_MAP_TYPES
};


/**
 * Used to define the states of the OpenSL ES / OpenMAX AL object initialization and preparation
 * with regards to the Android-side of the data
 */
enum AndroidObject_state {
    ANDROID_UNINITIALIZED = -1,
    ANDROID_PREPARING,
    ANDROID_READY,
    NUM_ANDROID_STATES
};


#define ANDROID_DEFAULT_OUTPUT_STREAM_TYPE android::AudioSystem::MUSIC


/*
 * Structure to maintain the set of audio levels about a player
 */
typedef struct AndroidAudioLevels_struct {
/** send level to aux effect, there's a single aux bus, so there's a single level */
    SLmillibel mAuxSendLevel;
    /**
     * Amplification (can be attenuation) factor derived for the VolumeLevel
     */
    float mAmplFromVolLevel;
    /**
     * Left/right amplification (can be attenuations) factors derived for the StereoPosition
     */
    float mAmplFromStereoPos[STEREO_CHANNELS];
    /**
     * Attenuation factor derived from direct level
     */
    float mAmplFromDirectLevel;
} AndroidAudioLevels;

