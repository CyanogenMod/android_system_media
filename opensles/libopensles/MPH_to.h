/* Copyright 2010 The Android Open Source Project */

#ifndef __MPH_to_H
#define __MPH_to_H

// Map minimal perfect hash of an interface ID to its class index.

extern const signed char
    MPH_to_3DGroup[MPH_MAX],
    MPH_to_AudioPlayer[MPH_MAX],
    MPH_to_AudioRecorder[MPH_MAX],
    MPH_to_Engine[MPH_MAX],
    MPH_to_LEDDevice[MPH_MAX],
    MPH_to_Listener[MPH_MAX],
    MPH_to_MetadataExtractor[MPH_MAX],
    MPH_to_MidiPlayer[MPH_MAX],
    MPH_to_OutputMix[MPH_MAX],
    MPH_to_Vibra[MPH_MAX];

#endif // !defined(__MPH_to_H)
