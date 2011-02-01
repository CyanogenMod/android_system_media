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
    A_PLR_URIFD      = 0, // audio player, compressed data, URI or FD data source
    A_PLR_PCM_BQ     = 1, // audio player, PCM, buffer queue data source
    A_PLR_TS_ABQ     = 2, // audio player, transport stream, Android simple buffer queue data source
    A_PLR_URIFD_ASQ  = 3, // audio player, URI or FD data source (as in android::MediaPlayer),
                          //    decoded to a PCM Android simple buffer queue data sink
    AV_PLR_TS_ABQ    = 4, // audio video player, transport stream, Android buffer queue data source
    AV_PLR_URIFD     = 5, // audio video player, URI or FD data source (as in android::MediaPlayer)
    A_RCR_MIC_ASQ    = 6, // audio recorder, device data source,
                          //    streamed into a PCM Android simple buffer queue data sink
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

#define PLAYER_SUCCESS 1

#define PLAYER_FD_FIND_FILE_SIZE ((int64_t)0xFFFFFFFFFFFFFFFFll)


/**
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


/**
 * Event notification callback from Android to SL ES framework
 */
typedef void (*notif_cbf_t)(int event, int data1, void* notifUser);

/**
 * Audio data push callback from Android objects to SL ES framework
 */
typedef size_t (*data_push_cbf_t)(const uint8_t *data, size_t size, void* user);


/**
 * Events sent to mNotifyClient during prepare, prefetch, and playback
 * used in APlayer::notify() and AMessage::findxxx()
 */
#define PLAYEREVENT_PREPARED                "prep"
#define PLAYEREVENT_PREFETCHSTATUSCHANGE    "prsc"
#define PLAYEREVENT_PREFETCHFILLLEVELUPDATE "pflu"
#define PLAYEREVENT_ENDOFSTREAM             "eos"
#define PLAYEREVENT_NEW_AUDIOTRACK          "nwat"


/**
 * Command parameters for AHandler commands
 */
#define WHATPARAM_SEEK_SEEKTIME_MS "seekTimeMs"
#define WHATPARAM_LOOP_LOOPING     "looping"

namespace android {

enum {
    kDataLocatorNone = 'none',
    kDataLocatorUri  = 'uri',
    kDataLocatorFd   = 'fd',
    };

struct FdInfo {
    int fd;
    int64_t offset;
    int64_t length;
};

// TODO currently used by SfPlayer, to replace by DataLocator2
union DataLocator {
    char* uri;
    FdInfo fdi;
};

union DataLocator2 {
    const char* uriRef;
    FdInfo fdi;
};

} // namespace android
