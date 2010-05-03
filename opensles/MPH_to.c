/* Copyright 2010 The Android Open Source Project */

// Map minimal perfect hash of an interface ID to its class index.
// This _must_ be compiled with GNU C, not GNU C++ or non-GNU C.

#include "MPH.h"

const signed char MPH_to_3DGroup[MPH_MAX] = {
    [0 ... MPH_MAX-1] = -1,
    [MPH_OBJECT] = 0,
    [MPH_DYNAMICINTERFACEMANAGEMENT] = 1,
    [MPH_3DLOCATION] = 2,
    [MPH_3DDOPPLER] = 3,
    [MPH_3DSOURCE] = 4,
    [MPH_3DMACROSCOPIC] = 5
};

const signed char MPH_to_AudioPlayer[MPH_MAX] = {
    [0 ... MPH_MAX-1] = -1,
    [MPH_OBJECT] = 0,
    [MPH_DYNAMICINTERFACEMANAGEMENT] = 1,
    [MPH_PLAY] = 2,
    [MPH_3DDOPPLER] = 3,
    [MPH_3DGROUPING] = 4,
    [MPH_3DLOCATION] = 5,
    [MPH_3DSOURCE] = 6,
    [MPH_BUFFERQUEUE] = 7,
    [MPH_EFFECTSEND] = 8,
    [MPH_MUTESOLO] = 9,
    [MPH_METADATAEXTRACTION] = 10,
    [MPH_METADATATRAVERSAL] = 11,
    [MPH_PREFETCHSTATUS] = 12,
    [MPH_RATEPITCH] = 13,
    [MPH_SEEK] = 14,
    [MPH_VOLUME] = 15,
    [MPH_3DMACROSCOPIC] = 16,
    [MPH_BASSBOOST] = 17,
    [MPH_DYNAMICSOURCE] = 18,
    [MPH_ENVIRONMENTALREVERB] = 19,
    [MPH_EQUALIZER] = 20,
    [MPH_PITCH] = 21,
    [MPH_PRESETREVERB] = 22,
    [MPH_PLAYBACKRATE] = 23,
    [MPH_VIRTUALIZER] = 24,
    [MPH_VISUALIZATION] = 25
};

const signed char MPH_to_AudioRecorder[MPH_MAX] = {
    [0 ... MPH_MAX-1] = -1,
    [MPH_OBJECT] = 0,
    [MPH_DYNAMICINTERFACEMANAGEMENT] = 1,
    [MPH_RECORD] = 2,
    [MPH_AUDIOENCODER] = 3,
    [MPH_BASSBOOST] = 4,
    [MPH_DYNAMICSOURCE] = 5,
    [MPH_EQUALIZER] = 6,
    [MPH_VISUALIZATION] = 7,
    [MPH_VOLUME] = 8
};

const signed char MPH_to_Engine[MPH_MAX] = {
    [0 ... MPH_MAX-1] = -1,
    [MPH_OBJECT] = 0,
    [MPH_DYNAMICINTERFACEMANAGEMENT] = 1,
    [MPH_ENGINE] = 2,
    [MPH_ENGINECAPABILITIES] = 3,
    [MPH_THREADSYNC] = 4,
    [MPH_AUDIOIODEVICECAPABILITIES] = 5,
    [MPH_AUDIODECODERCAPABILITIES] = 6,
    [MPH_AUDIOENCODERCAPABILITIES] = 7,
    [MPH_3DCOMMIT] = 8,
    [MPH_DEVICEVOLUME] = 9
};

const signed char MPH_to_LEDDevice[MPH_MAX] = {
    [0 ... MPH_MAX-1] = -1,
    [MPH_OBJECT] = 0,
    [MPH_DYNAMICINTERFACEMANAGEMENT] = 1,
    [MPH_LED] = 2
};

const signed char MPH_to_Listener[MPH_MAX] = {
    [0 ... MPH_MAX-1] = -1,
    [MPH_OBJECT] = 0,
    [MPH_DYNAMICINTERFACEMANAGEMENT] = 1,
    [MPH_3DDOPPLER] = 2,
    [MPH_3DLOCATION] = 3
};

const signed char MPH_to_MetadataExtractor[MPH_MAX] = {
    [0 ... MPH_MAX-1] = -1,
    [MPH_OBJECT] = 0,
    [MPH_DYNAMICINTERFACEMANAGEMENT] = 1,
    [MPH_DYNAMICSOURCE] = 2,
    [MPH_METADATAEXTRACTION] = 3,
    [MPH_METADATATRAVERSAL] = 4
};

const signed char MPH_to_MidiPlayer[MPH_MAX] = {
    [0 ... MPH_MAX-1] = -1,
    [MPH_OBJECT] = 0,
    [MPH_DYNAMICINTERFACEMANAGEMENT] = 1,
    [MPH_PLAY] = 2,
    [MPH_3DDOPPLER] = 3,
    [MPH_3DGROUPING] = 4,
    [MPH_3DLOCATION] = 5,
    [MPH_3DSOURCE] = 6,
    [MPH_BUFFERQUEUE] = 7,
    [MPH_EFFECTSEND] = 8,
    [MPH_MUTESOLO] = 9,
    [MPH_METADATAEXTRACTION] = 10,
    [MPH_METADATATRAVERSAL] = 11,
    [MPH_MIDIMESSAGE] = 12,
    [MPH_MIDITIME] = 13,
    [MPH_MIDITEMPO] = 14,
    [MPH_MIDIMUTESOLO] = 15,
    [MPH_PREFETCHSTATUS] = 16,
    [MPH_SEEK] = 17,
    [MPH_VOLUME] = 18,
    [MPH_3DMACROSCOPIC] = 19,
    [MPH_BASSBOOST] = 20,
    [MPH_DYNAMICSOURCE] = 21,
    [MPH_ENVIRONMENTALREVERB] = 22,
    [MPH_EQUALIZER] = 23,
    [MPH_PITCH] = 24,
    [MPH_PRESETREVERB] = 25,
    [MPH_PLAYBACKRATE] = 26,
    [MPH_VIRTUALIZER] = 27,
    [MPH_VISUALIZATION] = 28
};

const signed char MPH_to_OutputMix[MPH_MAX] = {
    [0 ... MPH_MAX-1] = -1,
    [MPH_OBJECT] = 0,
    [MPH_DYNAMICINTERFACEMANAGEMENT] = 1,
    [MPH_OUTPUTMIX] = 2,
#ifdef USE_OUTPUTMIXEXT
    [MPH_OUTPUTMIXEXT] = 3,
#endif
    [MPH_ENVIRONMENTALREVERB] = 4,
    [MPH_EQUALIZER] = 5,
    [MPH_PRESETREVERB] = 6,
    [MPH_VIRTUALIZER] = 7,
    [MPH_VOLUME] = 8,
    [MPH_BASSBOOST] = 9,
    [MPH_VISUALIZATION] = 10
};

const signed char MPH_to_Vibra[MPH_MAX] = {
    [0 ... MPH_MAX-1] = -1,
    [MPH_OBJECT] = 0,
    [MPH_DYNAMICINTERFACEMANAGEMENT] = 1,
    [MPH_VIBRA] = 2
};
