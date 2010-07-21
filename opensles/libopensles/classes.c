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

/* Classes vs. interfaces */

#include "sles_allinclusive.h"

// 3DGroup class

static const struct iid_vtable _3DGroup_interfaces[INTERFACES_3DGroup] = {
    {MPH_OBJECT, INTERFACE_IMPLICIT, offsetof(C3DGroup, mObject)},
    {MPH_DYNAMICINTERFACEMANAGEMENT, INTERFACE_IMPLICIT,
        offsetof(C3DGroup, mDynamicInterfaceManagement)},
    {MPH_3DLOCATION, INTERFACE_IMPLICIT, offsetof(C3DGroup, m3DLocation)},
    {MPH_3DDOPPLER, INTERFACE_DYNAMIC_GAME, offsetof(C3DGroup, m3DDoppler)},
    {MPH_3DSOURCE, INTERFACE_GAME, offsetof(C3DGroup, m3DSource)},
    {MPH_3DMACROSCOPIC, INTERFACE_OPTIONAL, offsetof(C3DGroup, m3DMacroscopic)},
};

static const ClassTable C3DGroup_class = {
    _3DGroup_interfaces,
    INTERFACES_3DGroup,
    MPH_to_3DGroup,
    "3DGroup",
    sizeof(C3DGroup),
    SL_OBJECTID_3DGROUP,
    NULL,
    NULL,
    NULL
};

// AudioPlayer class

static const struct iid_vtable AudioPlayer_interfaces[INTERFACES_AudioPlayer] = {
    {MPH_OBJECT, INTERFACE_IMPLICIT, offsetof(CAudioPlayer, mObject)},
    {MPH_DYNAMICINTERFACEMANAGEMENT, INTERFACE_IMPLICIT,
        offsetof(CAudioPlayer, mDynamicInterfaceManagement)},
    {MPH_PLAY, INTERFACE_IMPLICIT, offsetof(CAudioPlayer, mPlay)},
    {MPH_3DDOPPLER, INTERFACE_DYNAMIC_GAME, offsetof(CAudioPlayer, m3DDoppler)},
    {MPH_3DGROUPING, INTERFACE_GAME, offsetof(CAudioPlayer, m3DGrouping)},
    {MPH_3DLOCATION, INTERFACE_GAME, offsetof(CAudioPlayer, m3DLocation)},
    {MPH_3DSOURCE, INTERFACE_GAME, offsetof(CAudioPlayer, m3DSource)},
#ifdef USE_SNDFILE
    // Currently we create an internal buffer queue for playing encoded files
    {MPH_BUFFERQUEUE, INTERFACE_IMPLICIT, offsetof(CAudioPlayer, mBufferQueue)},
#else
    {MPH_BUFFERQUEUE, INTERFACE_GAME, offsetof(CAudioPlayer, mBufferQueue)},
#endif
    {MPH_EFFECTSEND, INTERFACE_MUSIC_GAME, offsetof(CAudioPlayer, mEffectSend)},
    {MPH_MUTESOLO, INTERFACE_GAME, offsetof(CAudioPlayer, mMuteSolo)},
    {MPH_METADATAEXTRACTION, INTERFACE_MUSIC_GAME, offsetof(CAudioPlayer, mMetadataExtraction)},
    {MPH_METADATATRAVERSAL, INTERFACE_MUSIC_GAME, offsetof(CAudioPlayer, mMetadataTraversal)},
    {MPH_PREFETCHSTATUS, INTERFACE_EXPLICIT, offsetof(CAudioPlayer, mPrefetchStatus)},
    {MPH_RATEPITCH, INTERFACE_DYNAMIC_GAME, offsetof(CAudioPlayer, mRatePitch)},
    {MPH_SEEK, INTERFACE_EXPLICIT, offsetof(CAudioPlayer, mSeek)},
    {MPH_VOLUME, INTERFACE_TBD, offsetof(CAudioPlayer, mVolume)},
    {MPH_3DMACROSCOPIC, INTERFACE_OPTIONAL, offsetof(CAudioPlayer, m3DMacroscopic)},
    {MPH_BASSBOOST, INTERFACE_OPTIONAL, offsetof(CAudioPlayer, mBassBoost)},
    {MPH_DYNAMICSOURCE, INTERFACE_OPTIONAL, offsetof(CAudioPlayer, mDynamicSource)},
    {MPH_ENVIRONMENTALREVERB, INTERFACE_OPTIONAL, offsetof(CAudioPlayer, mEnvironmentalReverb)},
    {MPH_EQUALIZER, INTERFACE_OPTIONAL, offsetof(CAudioPlayer, mEqualizer)},
    {MPH_PITCH, INTERFACE_OPTIONAL, offsetof(CAudioPlayer, mPitch)},
    {MPH_PRESETREVERB, INTERFACE_OPTIONAL, offsetof(CAudioPlayer, mPresetReverb)},
    {MPH_PLAYBACKRATE, INTERFACE_OPTIONAL, offsetof(CAudioPlayer, mPlaybackRate)},
    {MPH_VIRTUALIZER, INTERFACE_OPTIONAL, offsetof(CAudioPlayer, mVirtualizer)},
    {MPH_VISUALIZATION, INTERFACE_OPTIONAL, offsetof(CAudioPlayer, mVisualization)},
#ifdef ANDROID
    {MPH_ANDROIDSTREAMTYPE, INTERFACE_IMPLICIT, offsetof(CAudioPlayer, mAndroidStreamType)},
#endif
};

static const ClassTable CAudioPlayer_class = {
    AudioPlayer_interfaces,
    INTERFACES_AudioPlayer,
    MPH_to_AudioPlayer,
    "AudioPlayer",
    sizeof(CAudioPlayer),
    SL_OBJECTID_AUDIOPLAYER,
    CAudioPlayer_Realize,
    NULL /*CAudioPlayer_Resume*/,
    CAudioPlayer_Destroy
};

// AudioRecorder class

static const struct iid_vtable AudioRecorder_interfaces[INTERFACES_AudioRecorder] = {
    {MPH_OBJECT, INTERFACE_IMPLICIT, offsetof(CAudioRecorder, mObject)},
    {MPH_DYNAMICINTERFACEMANAGEMENT, INTERFACE_IMPLICIT,
        offsetof(CAudioRecorder, mDynamicInterfaceManagement)},
    {MPH_RECORD, INTERFACE_IMPLICIT, offsetof(CAudioRecorder, mRecord)},
    {MPH_AUDIOENCODER, INTERFACE_TBD, offsetof(CAudioRecorder, mAudioEncoder)},
    {MPH_BASSBOOST, INTERFACE_OPTIONAL, offsetof(CAudioRecorder, mBassBoost)},
    {MPH_DYNAMICSOURCE, INTERFACE_OPTIONAL, offsetof(CAudioRecorder, mDynamicSource)},
    {MPH_EQUALIZER, INTERFACE_OPTIONAL, offsetof(CAudioRecorder, mEqualizer)},
    {MPH_VISUALIZATION, INTERFACE_OPTIONAL, offsetof(CAudioRecorder, mVisualization)},
    {MPH_VOLUME, INTERFACE_OPTIONAL, offsetof(CAudioRecorder, mVolume)}
#ifdef ANDROID
    ,{MPH_BUFFERQUEUE, INTERFACE_EXPLICIT, offsetof(CAudioRecorder, mBufferQueue)},
#endif
};

static const ClassTable CAudioRecorder_class = {
    AudioRecorder_interfaces,
    INTERFACES_AudioRecorder,
    MPH_to_AudioRecorder,
    "AudioRecorder",
    sizeof(CAudioRecorder),
    SL_OBJECTID_AUDIORECORDER,
    CAudioRecorder_Realize,
    CAudioRecorder_Resume,
    CAudioRecorder_Destroy
};

// Engine class

static const struct iid_vtable Engine_interfaces[INTERFACES_Engine] = {
    {MPH_OBJECT, INTERFACE_IMPLICIT, offsetof(CEngine, mObject)},
    {MPH_DYNAMICINTERFACEMANAGEMENT, INTERFACE_IMPLICIT_CT,
        offsetof(CEngine, mDynamicInterfaceManagement)},
    {MPH_ENGINE, INTERFACE_IMPLICIT, offsetof(CEngine, mEngine)},
    {MPH_ENGINECAPABILITIES, INTERFACE_IMPLICIT_CT, offsetof(CEngine, mEngineCapabilities)},
    {MPH_THREADSYNC, INTERFACE_IMPLICIT_CT, offsetof(CEngine, mThreadSync)},
    {MPH_AUDIOIODEVICECAPABILITIES, INTERFACE_IMPLICIT_CT,
        offsetof(CEngine, mAudioIODeviceCapabilities)},
    {MPH_AUDIODECODERCAPABILITIES, INTERFACE_EXPLICIT_CT,
        offsetof(CEngine, mAudioDecoderCapabilities)},
    {MPH_AUDIOENCODERCAPABILITIES, INTERFACE_EXPLICIT_CT,
        offsetof(CEngine, mAudioEncoderCapabilities)},
    {MPH_3DCOMMIT, INTERFACE_EXPLICIT_GAME_CT, offsetof(CEngine, m3DCommit)},
    {MPH_DEVICEVOLUME, INTERFACE_OPTIONAL_CT, offsetof(CEngine, mDeviceVolume)}
};

static const ClassTable CEngine_class = {
    Engine_interfaces,
    INTERFACES_Engine,
    MPH_to_Engine,
    "Engine",
    sizeof(CEngine),
    SL_OBJECTID_ENGINE,
    CEngine_Realize,
    NULL,
    CEngine_Destroy
};

// LEDDevice class

static const struct iid_vtable LEDDevice_interfaces[INTERFACES_LEDDevice] = {
    {MPH_OBJECT, INTERFACE_IMPLICIT, offsetof(CLEDDevice, mObject)},
    {MPH_DYNAMICINTERFACEMANAGEMENT, INTERFACE_IMPLICIT,
        offsetof(CLEDDevice, mDynamicInterfaceManagement)},
    {MPH_LED, INTERFACE_IMPLICIT, offsetof(CLEDDevice, mLEDArray)}
};

static const ClassTable CLEDDevice_class = {
    LEDDevice_interfaces,
    INTERFACES_LEDDevice,
    MPH_to_LEDDevice,
    "LEDDevice",
    sizeof(CLEDDevice),
    SL_OBJECTID_LEDDEVICE,
    NULL,
    NULL,
    NULL
};

// Listener class

static const struct iid_vtable Listener_interfaces[INTERFACES_Listener] = {
    {MPH_OBJECT, INTERFACE_IMPLICIT, offsetof(CListener, mObject)},
    {MPH_DYNAMICINTERFACEMANAGEMENT, INTERFACE_IMPLICIT,
        offsetof(CListener, mDynamicInterfaceManagement)},
    {MPH_3DDOPPLER, INTERFACE_DYNAMIC_GAME, offsetof(C3DGroup, m3DDoppler)},
    {MPH_3DLOCATION, INTERFACE_EXPLICIT_GAME, offsetof(C3DGroup, m3DLocation)}
};

static const ClassTable CListener_class = {
    Listener_interfaces,
    INTERFACES_Listener,
    MPH_to_Listener,
    "Listener",
    sizeof(CListener),
    SL_OBJECTID_LISTENER,
    NULL,
    NULL,
    NULL
};

// MetadataExtractor class

static const struct iid_vtable MetadataExtractor_interfaces[INTERFACES_MetadataExtractor] = {
    {MPH_OBJECT, INTERFACE_IMPLICIT, offsetof(CMetadataExtractor, mObject)},
    {MPH_DYNAMICINTERFACEMANAGEMENT, INTERFACE_IMPLICIT,
        offsetof(CMetadataExtractor, mDynamicInterfaceManagement)},
    {MPH_DYNAMICSOURCE, INTERFACE_IMPLICIT, offsetof(CMetadataExtractor, mDynamicSource)},
    {MPH_METADATAEXTRACTION, INTERFACE_IMPLICIT, offsetof(CMetadataExtractor, mMetadataExtraction)},
    {MPH_METADATATRAVERSAL, INTERFACE_IMPLICIT, offsetof(CMetadataExtractor, mMetadataTraversal)}
};

static const ClassTable CMetadataExtractor_class = {
    MetadataExtractor_interfaces,
    INTERFACES_MetadataExtractor,
    MPH_to_MetadataExtractor,
    "MetadataExtractor",
    sizeof(CMetadataExtractor),
    SL_OBJECTID_METADATAEXTRACTOR,
    NULL,
    NULL,
    NULL
};

// MidiPlayer class

static const struct iid_vtable MidiPlayer_interfaces[INTERFACES_MidiPlayer] = {
    {MPH_OBJECT, INTERFACE_IMPLICIT, offsetof(CMidiPlayer, mObject)},
    {MPH_DYNAMICINTERFACEMANAGEMENT, INTERFACE_IMPLICIT,
        offsetof(CMidiPlayer, mDynamicInterfaceManagement)},
    {MPH_PLAY, INTERFACE_IMPLICIT, offsetof(CMidiPlayer, mPlay)},
    {MPH_3DDOPPLER, INTERFACE_DYNAMIC_GAME, offsetof(C3DGroup, m3DDoppler)},
    {MPH_3DGROUPING, INTERFACE_GAME, offsetof(CMidiPlayer, m3DGrouping)},
    {MPH_3DLOCATION, INTERFACE_GAME, offsetof(CMidiPlayer, m3DLocation)},
    {MPH_3DSOURCE, INTERFACE_GAME, offsetof(CMidiPlayer, m3DSource)},
    {MPH_BUFFERQUEUE, INTERFACE_GAME, offsetof(CMidiPlayer, mBufferQueue)},
    {MPH_EFFECTSEND, INTERFACE_GAME, offsetof(CMidiPlayer, mEffectSend)},
    {MPH_MUTESOLO, INTERFACE_GAME, offsetof(CMidiPlayer, mMuteSolo)},
    {MPH_METADATAEXTRACTION, INTERFACE_GAME, offsetof(CMidiPlayer, mMetadataExtraction)},
    {MPH_METADATATRAVERSAL, INTERFACE_GAME, offsetof(CMidiPlayer, mMetadataTraversal)},
    {MPH_MIDIMESSAGE, INTERFACE_PHONE_GAME, offsetof(CMidiPlayer, mMIDIMessage)},
    {MPH_MIDITIME, INTERFACE_PHONE_GAME, offsetof(CMidiPlayer, mMIDITime)},
    {MPH_MIDITEMPO, INTERFACE_PHONE_GAME, offsetof(CMidiPlayer, mMIDITempo)},
    {MPH_MIDIMUTESOLO, INTERFACE_GAME, offsetof(CMidiPlayer, mMIDIMuteSolo)},
    {MPH_PREFETCHSTATUS, INTERFACE_PHONE_GAME, offsetof(CMidiPlayer, mPrefetchStatus)},
    {MPH_SEEK, INTERFACE_PHONE_GAME, offsetof(CMidiPlayer, mSeek)},
    {MPH_VOLUME, INTERFACE_PHONE_GAME, offsetof(CMidiPlayer, mVolume)},
    {MPH_3DMACROSCOPIC, INTERFACE_OPTIONAL, offsetof(CMidiPlayer, m3DMacroscopic)},
    {MPH_BASSBOOST, INTERFACE_OPTIONAL, offsetof(CMidiPlayer, mBassBoost)},
    {MPH_DYNAMICSOURCE, INTERFACE_OPTIONAL, offsetof(CMidiPlayer, mDynamicSource)},
    {MPH_ENVIRONMENTALREVERB, INTERFACE_OPTIONAL, offsetof(CMidiPlayer, mEnvironmentalReverb)},
    {MPH_EQUALIZER, INTERFACE_OPTIONAL, offsetof(CMidiPlayer, mEqualizer)},
    {MPH_PITCH, INTERFACE_OPTIONAL, offsetof(CMidiPlayer, mPitch)},
    {MPH_PRESETREVERB, INTERFACE_OPTIONAL, offsetof(CMidiPlayer, mPresetReverb)},
    {MPH_PLAYBACKRATE, INTERFACE_OPTIONAL, offsetof(CMidiPlayer, mPlaybackRate)},
    {MPH_VIRTUALIZER, INTERFACE_OPTIONAL, offsetof(CMidiPlayer, mVirtualizer)},
    {MPH_VISUALIZATION, INTERFACE_OPTIONAL, offsetof(CMidiPlayer, mVisualization)},
#ifdef ANDROID
    {MPH_ANDROIDSTREAMTYPE, INTERFACE_IMPLICIT, offsetof(CMidiPlayer, mAndroidStreamType)},
#endif
};

static const ClassTable CMidiPlayer_class = {
    MidiPlayer_interfaces,
    INTERFACES_MidiPlayer,
    MPH_to_MidiPlayer,
    "MidiPlayer",
    sizeof(CMidiPlayer),
    SL_OBJECTID_MIDIPLAYER,
    NULL,
    NULL,
    NULL
};

// OutputMix class

static const struct iid_vtable OutputMix_interfaces[INTERFACES_OutputMix] = {
    {MPH_OBJECT, INTERFACE_IMPLICIT, offsetof(COutputMix, mObject)},
    {MPH_DYNAMICINTERFACEMANAGEMENT, INTERFACE_IMPLICIT,
        offsetof(COutputMix, mDynamicInterfaceManagement)},
    {MPH_OUTPUTMIX, INTERFACE_IMPLICIT, offsetof(COutputMix, mOutputMix)},
#ifdef USE_OUTPUTMIXEXT
    // should be internal implicit (available, but not advertised
    // publicly or included in possible interface count)
    {MPH_OUTPUTMIXEXT, INTERFACE_IMPLICIT, offsetof(COutputMix, mOutputMixExt)},
#else
    {MPH_OUTPUTMIXEXT, INTERFACE_UNAVAILABLE, 0},
#endif
    {MPH_ENVIRONMENTALREVERB, INTERFACE_DYNAMIC_GAME, offsetof(COutputMix, mEnvironmentalReverb)},
    {MPH_EQUALIZER, INTERFACE_DYNAMIC_MUSIC_GAME, offsetof(COutputMix, mEqualizer)},
    {MPH_PRESETREVERB, INTERFACE_DYNAMIC_MUSIC, offsetof(COutputMix, mPresetReverb)},
    {MPH_VIRTUALIZER, INTERFACE_DYNAMIC_MUSIC_GAME, offsetof(COutputMix, mVirtualizer)},
    {MPH_VOLUME, INTERFACE_GAME_MUSIC, offsetof(COutputMix, mVolume)},
    {MPH_BASSBOOST, INTERFACE_OPTIONAL_DYNAMIC, offsetof(COutputMix, mBassBoost)},
    {MPH_VISUALIZATION, INTERFACE_OPTIONAL, offsetof(COutputMix, mVisualization)}
};

static const ClassTable COutputMix_class = {
    OutputMix_interfaces,
    INTERFACES_OutputMix,
    MPH_to_OutputMix,
    "OutputMix",
    sizeof(COutputMix),
    SL_OBJECTID_OUTPUTMIX,
    NULL,
    NULL,
    NULL
};

// Vibra class

static const struct iid_vtable VibraDevice_interfaces[INTERFACES_VibraDevice] = {
    {MPH_OBJECT, INTERFACE_IMPLICIT, offsetof(CVibraDevice, mObject)},
    {MPH_DYNAMICINTERFACEMANAGEMENT, INTERFACE_IMPLICIT,
        offsetof(CVibraDevice, mDynamicInterfaceManagement)},
    {MPH_VIBRA, INTERFACE_IMPLICIT, offsetof(CVibraDevice, mVibra)}
};

static const ClassTable CVibraDevice_class = {
    VibraDevice_interfaces,
    INTERFACES_VibraDevice,
    MPH_to_Vibra,
    "VibraDevice",
    sizeof(CVibraDevice),
    SL_OBJECTID_VIBRADEVICE,
    NULL,
    NULL,
    NULL
};

/* Map SL_OBJECTID to class */

static const ClassTable * const classes[] = {
    // Do not change order of these entries; they are in numerical order
    &CEngine_class,
    &CLEDDevice_class,
    &CVibraDevice_class,
    &CAudioPlayer_class,
    &CAudioRecorder_class,
    &CMidiPlayer_class,
    &CListener_class,
    &C3DGroup_class,
    &COutputMix_class,
    &CMetadataExtractor_class
};

const ClassTable *objectIDtoClass(SLuint32 objectID)
{
    SLuint32 objectID0 = classes[0]->mObjectID;
    if (objectID0 <= objectID &&
        classes[sizeof(classes)/sizeof(classes[0])-1]->mObjectID >= objectID)
        return classes[objectID - objectID0];
    return NULL;
}
