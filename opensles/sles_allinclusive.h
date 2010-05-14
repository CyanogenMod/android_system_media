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

#include "OpenSLES.h"
#include <stddef.h> // offsetof
#include <stdlib.h> // malloc
#include <string.h> // memcmp
#include <stdio.h>  // debugging
#include <assert.h> // debugging
#include <pthread.h>

#include "MPH.h"
#include "MPH_to.h"

#ifdef USE_SNDFILE
#include <sndfile.h>
#endif // USE_SNDFILE

#ifdef USE_SDL
#include <SDL/SDL_audio.h>
#endif // USE_SDL

#ifdef USE_ANDROID
#include <unistd.h>
#include "media/AudioSystem.h"
#include "media/AudioTrack.h"
#include "media/mediaplayer.h"
#endif

#ifdef USE_OUTPUTMIXEXT
#include "OutputMixExt.h"
#endif

// Hook functions

typedef void (*VoidHook)(void *self);
typedef SLresult (*StatusHook)(void *self);

// Describes how an interface is related to a given class

#define INTERFACE_IMPLICIT           0
#define INTERFACE_EXPLICIT           1
#define INTERFACE_OPTIONAL           2
#define INTERFACE_DYNAMIC            3
#define INTERFACE_DYNAMIC_GAME       INTERFACE_DYNAMIC
#define INTERFACE_DYNAMIC_MUSIC      INTERFACE_DYNAMIC
#define INTERFACE_DYNAMIC_MUSIC_GAME INTERFACE_DYNAMIC
#define INTERFACE_EXPLICIT_GAME      INTERFACE_EXPLICIT
#define INTERFACE_GAME               INTERFACE_OPTIONAL
#define INTERFACE_GAME_MUSIC         INTERFACE_OPTIONAL
#define INTERFACE_MUSIC_GAME         INTERFACE_OPTIONAL
#define INTERFACE_OPTIONAL_DYNAMIC   INTERFACE_DYNAMIC
#define INTERFACE_PHONE_GAME         INTERFACE_OPTIONAL
#define INTERFACE_TBD                INTERFACE_IMPLICIT

// Maps an interface ID to its offset within the class that exposes it

struct iid_vtable {
    unsigned char mMPH;
    unsigned char mInterface;   // relationship
    /*size_t*/ unsigned short mOffset;
};

// Per-class const data shared by all instances of the same class

struct class_ {
    // needed by all classes (class class, the superclass of all classes)
    const struct iid_vtable *mInterfaces;
    SLuint32 mInterfaceCount;
    const signed char *mMPH_to_index;
    // FIXME not yet used
    //const char * const mName;
    size_t mSize;
    SLuint32 mObjectID;
    StatusHook mRealize;
    StatusHook mResume;
    VoidHook mDestroy;
    // append per-class data here
};

#ifdef USE_OUTPUTMIXEXT

// Track describes each input to OutputMixer
// FIXME not for Android

struct Track {
    const SLDataFormat_PCM *mDfPcm;
    struct BufferQueue_interface *mBufferQueue;
    struct Play_interface *mPlay; // mixer examines this track if non-NULL
    const void *mReader;    // pointer to next frame in BufferHeader.mBuffer
    SLuint32 mAvail;        // number of available bytes
};

#endif

// BufferHeader describes each element of a BufferQueue, other than the data

struct BufferHeader {
    const void *mBuffer;
    SLuint32 mSize;
};

#ifdef USE_OUTPUTMIXEXT

// stereo is a frame consisting of a pair of 16-bit PCM samples

typedef struct {
    short left;
    short right;
} stereo;

#endif

#ifdef USE_SNDFILE

struct SndFile {
    // save URI also?
    SLchar *mPathname;
    SNDFILE *mSNDFILE;
    // These are used when Enqueue returns SL_RESULT_BUFFER_INSUFFICIENT
    const void *mRetryBuffer;
    SLuint32 mRetrySize;
    SLboolean mIs0; // which buffer to use next
    // FIXME magic numbers
    short mBuffer0[512];
    short mBuffer1[512];
};

#endif // USE_SNDFILE


/* Interface structures */

struct _3DCommit_interface {
    const struct SL3DCommitItf_ *mItf;
    struct Object_interface *mThis;
    SLboolean mDeferred;
};

struct _3DDoppler_interface {
    const struct SL3DDopplerItf_ *mItf;
    struct Object_interface *mThis;
    union Cartesian_Spherical1 {
        SLVec3D mCartesian;
        struct {
            SLmillidegree mAzimuth;
            SLmillidegree mElevation;
            SLmillidegree mSpeed;
        } mSpherical;
    } mVelocity;
    SLpermille mDopplerFactor;
};

struct _3DGrouping_interface {
    const struct SL3DGroupingItf_ *mItf;
    struct Object_interface *mThis;
    SLObjectItf mGroup;
};

struct _3DLocation_interface {
    const struct SL3DLocationItf_ *mItf;
    struct Object_interface *mThis;
    union Cartesian_Spherical2 {
        SLVec3D mCartesian;
        struct {
            SLmillidegree mAzimuth;
            SLmillidegree mElevation;
            SLmillidegree mDistance;
        } mSpherical;
    } mLocation;
    SLmillidegree mHeading;
    SLmillidegree mPitch;
    SLmillidegree mRoll;
    SLVec3D mFront;
    SLVec3D mAbove;
    SLVec3D mUp;
};

struct _3DMacroscopic_interface {
    const struct SL3DMacroscopicItf_ *mItf;
    struct Object_interface *mThis;
    struct {
        SLmillimeter mWidth;
        SLmillimeter mHeight;
        SLmillimeter mDepth;
    } mSize;
    struct {
        SLmillimeter mHeading;
        SLmillimeter mPitch;
        SLmillimeter mRoll;
    } mOrientationAngles;
    struct {
        SLVec3D mFront;
        SLVec3D mUp;
    } mOrientationVectors;
    // for optimization
    SLuint32 mGeneration;
};

struct _3DSource_interface {
    const struct SL3DSourceItf_ *mItf;
    struct Object_interface *mThis;
    SLboolean mHeadRelative;
    SLboolean mRolloffMaxDistanceMute;
    SLmillimeter mMaxDistance;
    SLmillimeter mMinDistance;
    struct {
        SLmillidegree mInner;
        SLmillidegree mOuter;
    } mConeAngles;
    SLmillibel mConeOuterLevel;
    SLpermille mRolloffFactor;
    SLpermille mRoomRolloffFactor;
    SLuint8 mDistanceModel;
};

struct AudioDecoderCapabilities_interface {
    const struct SLAudioDecoderCapabilitiesItf_ *mItf;
    struct Object_interface *mThis;
};

struct AudioEncoder_interface {
    const struct SLAudioEncoderItf_ *mItf;
    struct Object_interface *mThis;
    SLAudioEncoderSettings mSettings;
};

struct AudioEncoderCapabilities_interface {
    const struct SLAudioEncoderCapabilitiesItf_ *mItf;
    struct Object_interface *mThis;
};

struct AudioIODeviceCapabilities_interface {
    const struct SLAudioIODeviceCapabilitiesItf_ *mItf;
    struct Object_interface *mThis;
};

struct BassBoost_interface {
    const struct SLBassBoostItf_ *mItf;
    struct Object_interface *mThis;
    SLboolean mEnabled;
    SLpermille mStrength;
};

struct BufferQueue_interface {
    const struct SLBufferQueueItf_ *mItf;
    struct Object_interface *mThis;
    volatile SLBufferQueueState mState;
    slBufferQueueCallback mCallback;
    void *mContext;
    SLuint32 mNumBuffers;
    struct BufferHeader *mArray;
    struct BufferHeader *mFront, *mRear;
    // saves a malloc in the typical case
#define BUFFER_HEADER_TYPICAL 4
    struct BufferHeader mTypical[BUFFER_HEADER_TYPICAL+1];
};

struct DeviceVolume_interface {
    const struct SLDeviceVolumeItf_ *mItf;
    struct Object_interface *mThis;
    SLint32 mVolume[2]; // FIXME Hard-coded for default in/out
};

struct DynamicInterfaceManagement_interface {
    const struct SLDynamicInterfaceManagementItf_ *mItf;
    struct Object_interface *mThis;
    unsigned mAddedMask;    // added interfaces, a subset of exposed interfaces
    slDynamicInterfaceManagementCallback mCallback;
    void *mContext;
};

struct DynamicSource_interface {
    const struct SLDynamicSourceItf_ *mItf;
    struct Object_interface *mThis;
    SLDataSource *mDataSource;
};

// FIXME Move this elsewhere

#define AUX_ENVIRONMENTALREVERB 0
#define AUX_PRESETREVERB        1
#define AUX_MAX                 2

#if 0
static const unsigned char AUX_to_MPH[AUX_MAX] = {
    MPH_ENVIRONMENTALREVERB,
    MPH_PRESETREVERB
};
#endif

// private

struct EnableLevel {
    SLboolean mEnable;
    SLmillibel mSendLevel;
};

struct EffectSend_interface {
    const struct SLEffectSendItf_ *mItf;
    struct Object_interface *mThis;
    struct OutputMix_class *mOutputMix;
    SLmillibel mDirectLevel;
    struct EnableLevel mEnableLevels[AUX_MAX];
};

struct Engine_interface {
    const struct SLEngineItf_ *mItf;
    struct Object_interface *mThis;
    SLboolean mLossOfControlGlobal;
    // FIXME Per-class non-const data such as vector of created objects.
    // Each engine is its own universe.
    // SLuint32 mInstanceCount;
    // Vector<Type> instances;
};

struct EngineCapabilities_interface {
    const struct SLEngineCapabilitiesItf_ *mItf;
    struct Object_interface *mThis;
    SLboolean mThreadSafe;
};

struct EnvironmentalReverb_interface {
    const struct SLEnvironmentalReverbItf_ *mItf;
    struct Object_interface *mThis;
    SLEnvironmentalReverbSettings mProperties;
};

// FIXME move
struct EqualizerBand {
    SLmilliHertz mMin;
    SLmilliHertz mMax;
    SLmilliHertz mCenter;
    /*TBD*/ int mLevel;
};

struct Equalizer_interface {
    const struct SLEqualizerItf_ *mItf;
    struct Object_interface *mThis;
    SLboolean mEnabled;
    SLmillibel *mLevels;
    SLuint16 mPreset;
    // const
    SLuint16 mNumPresets;
    SLuint16 mNumBands;
    SLmillibel mMin;
    SLmillibel mMax;
    struct EqualizerBand *mBands;
    const SLchar * const *mPresetNames;
    /*TBD*/ int mBandLevelRangeMin;
    /*TBD*/ int mBandLevelRangeMax;
};

struct LEDArray_interface {
    const struct SLLEDArrayItf_ *mItf;
    struct Object_interface *mThis;
    SLuint32 mLightMask;
    SLHSL *mColor;
    // const
    SLuint8 mCount;
};

struct MetadataExtraction_interface {
    const struct SLMetadataExtractionItf_ *mItf;
    struct Object_interface *mThis;
    SLuint32 mKeySize;
    const void *mKey;
    SLuint32 mKeyEncoding;
    const SLchar *mValueLangCountry;
    SLuint32 mValueEncoding;
    SLuint8 mFilterMask;
    /*FIXME*/ int mKeyFilter;
};

struct MetadataTraversal_interface {
    const struct SLMetadataTraversalItf_ *mItf;
    struct Object_interface *mThis;
    SLuint32 mIndex;
    SLuint32 mMode;
    SLuint32 mCount;
    SLuint32 mSize;
};

struct MIDIMessage_interface {
    const struct SLMIDIMessageItf_ *mItf;
    struct Object_interface *mThis;
    slMetaEventCallback mMetaEventCallback;
    void *mMetaEventContext;
    slMIDIMessageCallback mMessageCallback;
    void *mMessageContext;
    int /*TBD*/ mMessageTypes;
};

struct MIDIMuteSolo_interface {
    const struct SLMIDIMuteSoloItf_ *mItf;
    struct Object_interface *mThis;
    SLuint16 mChannelMuteMask;
    SLuint16 mChannelSoloMask;
    SLuint32 mTrackMuteMask;
    SLuint32 mTrackSoloMask;
    // const ?
    SLuint16 mTrackCount;
};

struct MIDITempo_interface {
    const struct SLMIDITempoItf_ *mItf;
    struct Object_interface *mThis;
    SLuint32 mTicksPerQuarterNote;
    SLuint32 mMicrosecondsPerQuarterNote;
};

struct MIDITime_interface {
    const struct SLMIDITimeItf_ *mItf;
    struct Object_interface *mThis;
    SLuint32 mDuration;
    SLuint32 mPosition;
    SLuint32 mStartTick;
    SLuint32 mNumTicks;
};

struct MuteSolo_interface {
    const struct SLMuteSoloItf_ *mItf;
    struct Object_interface *mThis;
    SLuint32 mMuteMask;
    SLuint32 mSoloMask;
    // const
    SLuint8 mNumChannels;
};

struct Object_interface {
    const struct SLObjectItf_ *mItf;
    // FIXME probably not needed for an Object, as it is always first,
    // but look for lingering code that assumes it is here before deleting
    struct Object_interface *mThis;
    const struct class_ *mClass;
    volatile SLuint32 mState;
    slObjectCallback mCallback;
    void *mContext;
    unsigned mExposedMask;  // exposed interfaces
    unsigned mLossOfControlMask;    // interfaces with loss of control enabled
    SLint32 mPriority;
    SLboolean mPreemptable;
    pthread_mutex_t mMutex;
    pthread_cond_t mCond;
    // FIXME also an object ID for RPC
    // FIXME and a human-readable name for debugging
};

struct OutputMix_interface {
    const struct SLOutputMixItf_ *mItf;
    struct Object_interface *mThis;
#ifdef USE_OUTPUTMIXEXT
    unsigned mActiveMask;   // 1 bit per active track
    struct Track mTracks[32];
#endif
};

#ifdef USE_OUTPUTMIXEXT
struct OutputMixExt_interface {
    const struct SLOutputMixExtItf_ *mItf;
    struct Object_interface *mThis;
};
#endif

struct Pitch_interface {
    const struct SLPitchItf_ *mItf;
    struct Object_interface *mThis;
    SLpermille mPitch;
    // const
    SLpermille mMinPitch;
    SLpermille mMaxPitch;
};

struct Play_interface {
    const struct SLPlayItf_ *mItf;
    struct Object_interface *mThis;
    volatile SLuint32 mState;
    SLmillisecond mDuration;
    SLmillisecond mPosition;
    // unsigned mPositionSamples;  // position in sample units
    slPlayCallback mCallback;
    void *mContext;
    SLuint32 mEventFlags;
    SLmillisecond mMarkerPosition;
    SLmillisecond mPositionUpdatePeriod;
};

struct PlaybackRate_interface {
    const struct SLPlaybackRateItf_ *mItf;
    struct Object_interface *mThis;
    SLpermille mRate;
    SLuint32 mPropertyConstraints;
    SLuint32 mProperties;
    SLpermille mMinRate;
    SLpermille mMaxRate;
    SLpermille mStepSize;
    SLuint32 mCapabilities;
};

struct PrefetchStatus_interface {
    const struct SLPrefetchStatusItf_ *mItf;
    struct Object_interface *mThis;
    SLuint32 mStatus;
    SLpermille mLevel;
    slPrefetchCallback mCallback;
    void *mContext;
    SLuint32 mCallbackEventsMask;
    SLpermille mFillUpdatePeriod;
};

struct PresetReverb_interface {
    const struct SLPresetReverbItf_ *mItf;
    struct Object_interface *mThis;
    SLuint16 mPreset;
};

struct RatePitch_interface {
    const struct SLRatePitchItf_ *mItf;
    struct Object_interface *mThis;
    SLpermille mRate;
    SLpermille mMinRate;
    SLpermille mMaxRate;
};

struct Record_interface {
    const struct SLRecordItf_ *mItf;
    struct Object_interface *mThis;
    SLuint32 mState;
    SLmillisecond mDurationLimit;
    SLmillisecond mPosition;
    slRecordCallback mCallback;
    void *mContext;
    SLuint32 mCallbackEventsMask;
    SLmillisecond mMarkerPosition;
    SLmillisecond mPositionUpdatePeriod;
};

struct Seek_interface {
    const struct SLSeekItf_ *mItf;
    struct Object_interface *mThis;
    SLmillisecond mPos;
    SLboolean mLoopEnabled;
    SLmillisecond mStartPos;
    SLmillisecond mEndPos;
};

struct ThreadSync_interface {
    const struct SLThreadSyncItf_ *mItf;
    struct Object_interface *mThis;
    SLboolean mInCriticalSection;
    SLboolean mWaiting;
    pthread_t mOwner;
};

struct Vibra_interface {
    const struct SLVibraItf_ *mItf;
    struct Object_interface *mThis;
    SLboolean mVibrate;
    SLmilliHertz mFrequency;
    SLpermille mIntensity;
};

struct Virtualizer_interface {
    const struct SLVirtualizerItf_ *mItf;
    struct Object_interface *mThis;
    SLboolean mEnabled;
    SLpermille mStrength;
};

struct Visualization_interface {
    const struct SLVisualizationItf_ *mItf;
    struct Object_interface *mThis;
    slVisualizationCallback mCallback;
    void *mContext;
    SLmilliHertz mRate;
};

struct Volume_interface {
    const struct SLVolumeItf_ *mItf;
    struct Object_interface *mThis;
    SLmillibel mLevel;
    SLboolean mMute;
    SLboolean mEnableStereoPosition;
    SLpermille mStereoPosition;
};

/* Class structures */

struct _3DGroup_class {
    struct Object_interface mObject;
    struct DynamicInterfaceManagement_interface mDynamicInterfaceManagement;
    struct _3DLocation_interface m3DLocation;
    struct _3DDoppler_interface m3DDoppler;
    struct _3DSource_interface m3DSource;
    struct _3DMacroscopic_interface m3DMacroscopic;
};

#ifdef USE_ANDROID
/*
 * Used to define the mapping from an OpenSL ES audio player to an Android
 * media framework object
 */
enum AndroidObject_type {
    //DEFAULT          =-1,
    MEDIAPLAYER      = 0,
    AUDIOTRACK_PUSH  = 1,
    AUDIOTRACK_PULL  = 2,
    NUM_AUDIOPLAYER_MAP_TYPES
};
#endif

struct AudioPlayer_class {
    struct Object_interface mObject;
    struct DynamicInterfaceManagement_interface mDynamicInterfaceManagement;
    struct Play_interface mPlay;
    struct _3DDoppler_interface m3DDoppler;
    struct _3DGrouping_interface m3DGrouping;
    struct _3DLocation_interface m3DLocation;
    struct _3DSource_interface m3DSource;
    struct BufferQueue_interface mBufferQueue;
    struct EffectSend_interface mEffectSend;
    struct MuteSolo_interface mMuteSolo;
    struct MetadataExtraction_interface mMetadataExtraction;
    struct MetadataTraversal_interface mMetadataTraversal;
    struct PrefetchStatus_interface mPrefetchStatus;
    struct RatePitch_interface mRatePitch;
    struct Seek_interface mSeek;
    struct Volume_interface mVolume;
    // optional interfaces
    struct _3DMacroscopic_interface m3DMacroscopic;
    struct BassBoost_interface mBassBoost;
    struct DynamicSource_interface mDynamicSource;
    struct EnvironmentalReverb_interface mEnvironmentalReverb;
    struct Equalizer_interface mEqualizer;
    struct Pitch_interface mPitch;
    struct PresetReverb_interface mPresetReverb;
    struct PlaybackRate_interface mPlaybackRate;
    struct Virtualizer_interface mVirtualizer;
    struct Visualization_interface mVisualization;
    // rest of fields are not related to the interfaces
#ifdef USE_SNDFILE
    struct SndFile mSndFile;
#endif // USE_SNDFILE
#ifdef USE_ANDROID
    enum AndroidObject_type mAndroidObjType;
    union {
        android::AudioTrack *mAudioTrack;
        android::MediaPlayer *mMediaPlayer;
    };
    pthread_t mThread;
#endif
};

struct AudioRecorder_class {
    // mandated interfaces
    struct Object_interface mObject;
    struct DynamicInterfaceManagement_interface mDynamicInterfaceManagement;
    struct Record_interface mRecord;
    struct AudioEncoder_interface mAudioEncoder;
    // optional interfaces
    struct BassBoost_interface mBassBoost;
    struct DynamicSource_interface mDynamicSource;
    struct Equalizer_interface mEqualizer;
    struct Visualization_interface mVisualization;
    struct Volume_interface mVolume;
};

struct Engine_class {
    // mandated implicit interfaces
    struct Object_interface mObject;
    struct DynamicInterfaceManagement_interface mDynamicInterfaceManagement;
    struct Engine_interface mEngine;
    struct EngineCapabilities_interface mEngineCapabilities;
    struct ThreadSync_interface mThreadSync;
    // mandated explicit interfaces
    struct AudioIODeviceCapabilities_interface mAudioIODeviceCapabilities;
    struct AudioDecoderCapabilities_interface mAudioDecoderCapabilities;
    struct AudioEncoderCapabilities_interface mAudioEncoderCapabilities;
    struct _3DCommit_interface m3DCommit;
    // optional interfaces
    struct DeviceVolume_interface mDeviceVolume;
};

struct LEDDevice_class {
    // mandated interfaces
    struct Object_interface mObject;
    struct DynamicInterfaceManagement_interface mDynamicInterfaceManagement;
    struct LEDArray_interface mLEDArray;
    SLuint32 mDeviceID;
};

struct Listener_class {
    // mandated interfaces
    struct Object_interface mObject;
    struct DynamicInterfaceManagement_interface mDynamicInterfaceManagement;
    struct _3DDoppler_interface m3DDoppler;
    struct _3DLocation_interface m3DLocation;
};

struct MetadataExtractor_class {
    // mandated interfaces
    struct Object_interface mObject;
    struct DynamicInterfaceManagement_interface mDynamicInterfaceManagement;
    struct DynamicSource_interface mDynamicSource;
    struct MetadataExtraction_interface mMetadataExtraction;
    struct MetadataTraversal_interface mMetadataTraversal;
};

struct MidiPlayer_class {
    // mandated interfaces
    struct Object_interface mObject;
    struct DynamicInterfaceManagement_interface mDynamicInterfaceManagement;
    struct Play_interface mPlay;
    struct _3DDoppler_interface m3DDoppler;
    struct _3DGrouping_interface m3DGrouping;
    struct _3DLocation_interface m3DLocation;
    struct _3DSource_interface m3DSource;
    struct BufferQueue_interface mBufferQueue;
    struct EffectSend_interface mEffectSend;
    struct MuteSolo_interface mMuteSolo;
    struct MetadataExtraction_interface mMetadataExtraction;
    struct MetadataTraversal_interface mMetadataTraversal;
    struct MIDIMessage_interface mMIDIMessage;
    struct MIDITime_interface mMIDITime;
    struct MIDITempo_interface mMIDITempo;
    struct MIDIMuteSolo_interface mMIDIMuteSolo;
    struct PrefetchStatus_interface mPrefetchStatus;
    struct Seek_interface mSeek;
    struct Volume_interface mVolume;
    // optional interfaces
    struct _3DMacroscopic_interface m3DMacroscopic;
    struct BassBoost_interface mBassBoost;
    struct DynamicSource_interface mDynamicSource;
    struct EnvironmentalReverb_interface mEnvironmentalReverb;
    struct Equalizer_interface mEqualizer;
    struct Pitch_interface mPitch;
    struct PresetReverb_interface mPresetReverb;
    struct PlaybackRate_interface mPlaybackRate;
    struct Virtualizer_interface mVirtualizer;
    struct Visualization_interface mVisualization;
};

struct OutputMix_class {
    // mandated interfaces
    struct Object_interface mObject;
    struct DynamicInterfaceManagement_interface mDynamicInterfaceManagement;
    struct OutputMix_interface mOutputMix;
#ifdef USE_OUTPUTMIXEXT
    struct OutputMixExt_interface mOutputMixExt;
#endif
    struct EnvironmentalReverb_interface mEnvironmentalReverb;
    struct Equalizer_interface mEqualizer;
    struct PresetReverb_interface mPresetReverb;
    struct Virtualizer_interface mVirtualizer;
    struct Volume_interface mVolume;
    // optional interfaces
    struct BassBoost_interface mBassBoost;
    struct Visualization_interface mVisualization;
};

struct VibraDevice_class {
    // mandated interfaces
    struct Object_interface mObject;
    struct DynamicInterfaceManagement_interface mDynamicInterfaceManagement;
    struct Vibra_interface mVibra;
    //
    SLuint32 mDeviceID;
};
