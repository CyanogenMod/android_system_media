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

extern SLresult sles_to_android_checkAudioPlayerSourceSink(const SLDataSource *pAudioSrc,
        const SLDataSink *pAudioSnk);

extern SLresult sles_to_android_audioPlayerCreate(const SLDataSource *pAudioSrc, const SLDataSink *pAudioSnk,
        CAudioPlayer *pAudioPlayer);

extern SLresult sles_to_android_audioPlayerRealize(CAudioPlayer *pAudioPlayer, SLboolean async);

extern SLresult sles_to_android_audioPlayerDestroy(CAudioPlayer *pAudioPlayer);

extern SLresult sles_to_android_audioPlayerSetPlayState(IPlay *pPlayItf, SLuint32 state);

extern SLresult sles_to_android_audioPlayerUseEventMask(IPlay *pPlayItf, SLuint32 eventFlags);

extern SLresult sles_to_android_audioPlayerGetPosition(IPlay *pPlayItf, SLmillisecond *pPosMsec);

extern SLresult sles_to_android_audioPlayerVolumeUpdate(IVolume *pVolItf);

extern SLresult sles_to_android_audioPlayerSetMute(IVolume *pVolItf, SLboolean mute);
