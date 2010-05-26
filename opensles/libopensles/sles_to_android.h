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

#define ANDROID_DEFAULT_OUTPUT_STREAM_TYPE android::AudioSystem::MUSIC
#define ANDROID_DEFAULT_AUDIOTRACK_BUFFER_SIZE 4096

// FIXME should all those functions below be inline? there are separated in different files for
//       code clarity.

/*
 * Checks that the combination of source and sink parameters is supported in this implementation.
 * Return
 *     SL_RESULT_SUCCESS
 *     SL_PARAMETER_INVALID
 */
SLresult sles_to_android_checkAudioPlayerSourceSink(const SLDataSource *pAudioSrc,
        const SLDataSink *pAudioSnk);

/*
 * Determines the Android media framework object that maps to the given audio source and sink.
 * Return
 *     SL_RESULT_SUCCESS if the Android resources were successfully created
 *     SL_PARAMETER_INVALID if the Android resources couldn't be created due to an invalid or
 *         unsupported parameter or value
 *     SL_RESULT_CONTENT_UNSUPPORTED if a format is not supported (e.g. sample rate too high)
 */
SLresult sles_to_android_audioPlayerCreate(const SLDataSource *pAudioSrc, const SLDataSink *pAudioSnk,
        CAudioPlayer *pAudioPlayer);

/*
 * Allocates and initializes the Android media framework objects intended to be used with the
 * given CAudioPlayer data
 * Return
 *     SL_RESULT_SUCCESS
 *     SL_RESULT_CONTENT_UNSUPPORTED if an error occurred during the allocation and initialization
 *         of the Android resources
 */
SLresult sles_to_android_audioPlayerRealize(CAudioPlayer *pAudioPlayer);

SLresult sles_to_android_audioPlayerDestroy(CAudioPlayer *pAudioPlayer);

SLresult sles_to_android_audioPlayerSetPlayState(IPlay *pPlayItf, SLuint32 state);

SLresult sles_to_android_audioPlayerUseEventMask(IPlay *pPlayItf, SLuint32 eventFlags);

SLresult sles_to_android_audioPlayerGetPosition(IPlay *pPlayItf, SLmillisecond *pPosMsec);

SLresult sles_to_android_audioPlayerVolumeUpdate(IVolume *pVolItf);

SLresult sles_to_android_audioPlayerSetMute(IVolume *pVolItf, SLboolean mute);

