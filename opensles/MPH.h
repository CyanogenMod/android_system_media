/* Copyright 2010 The Android Open Source Project */

#ifndef __MPH_H
#define __MPH_H

// Minimal perfect hash for each interface ID

#define MPH_3DCOMMIT                    0
#define MPH_3DDOPPLER                   1
#define MPH_3DGROUPING                  2
#define MPH_3DLOCATION                  3
#define MPH_3DMACROSCOPIC               4
#define MPH_3DSOURCE                    5
#define MPH_AUDIODECODERCAPABILITIES    6
#define MPH_AUDIOENCODER                7
#define MPH_AUDIOENCODERCAPABILITIES    8
#define MPH_AUDIOIODEVICECAPABILITIES   9
#define MPH_BASSBOOST                  10
#define MPH_BUFFERQUEUE                11
#define MPH_DEVICEVOLUME               12
#define MPH_DYNAMICINTERFACEMANAGEMENT 13
#define MPH_DYNAMICSOURCE              14
#define MPH_EFFECTSEND                 15
#define MPH_ENGINE                     16
#define MPH_ENGINECAPABILITIES         17
#define MPH_ENVIRONMENTALREVERB        18
#define MPH_EQUALIZER                  19
#define MPH_LED                        20
#define MPH_METADATAEXTRACTION         21
#define MPH_METADATATRAVERSAL          22
#define MPH_MIDIMESSAGE                23
#define MPH_MIDIMUTESOLO               24
#define MPH_MIDITEMPO                  25
#define MPH_MIDITIME                   26
#define MPH_MUTESOLO                   27
#if 1 // FIXME why needed?
#define MPH_NULL                       28
#endif
#define MPH_OBJECT                     29
#define MPH_OUTPUTMIX                  30
#define MPH_PITCH                      31
#define MPH_PLAY                       32
#define MPH_PLAYBACKRATE               33
#define MPH_PREFETCHSTATUS             34
#define MPH_PRESETREVERB               35
#define MPH_RATEPITCH                  36
#define MPH_RECORD                     37
#define MPH_SEEK                       38
#define MPH_THREADSYNC                 39
#define MPH_VIBRA                      40
#define MPH_VIRTUALIZER                41
#define MPH_VISUALIZATION              42
#define MPH_VOLUME                     43
// The lack of an ifdef is intentional
#define MPH_OUTPUTMIXEXT               44

#define MPH_MAX                        45

#endif // !defined(__MPH_H)
