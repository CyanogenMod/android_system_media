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
#include <unistd.h> // usleep

#include "MPH.h"
#include "MPH_to.h"
#include "devices.h"

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

typedef struct {
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
} ClassTable;

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

#ifdef __cplusplus
#define this this_
#endif

/* Interface structures */

typedef struct Object_interface {
    const struct SLObjectItf_ *mItf;
    // FIXME probably not needed for an Object, as it is always first,
    // but look for lingering code that assumes it is here before deleting
    struct Object_interface *mThis;
    const ClassTable *mClass;
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
} IObject;

#include "locks.h"

typedef struct {
    const struct SL3DCommitItf_ *mItf;
    IObject *mThis;
    SLboolean mDeferred;
    SLuint32 mGeneration;   // incremented each master clock cycle
} I3DCommit;

// FIXME move
enum CartesianSphericalActive {
    CARTESIAN_COMPUTED_SPHERICAL_SET,
    CARTESIAN_REQUESTED_SPHERICAL_SET,
    CARTESIAN_UNKNOWN_SPHERICAL_SET,
    CARTESIAN_SET_SPHERICAL_COMPUTED,   // not in 1.0.1
    CARTESIAN_SET_SPHERICAL_REQUESTED,  // not in 1.0.1
    CARTESIAN_SET_SPHERICAL_UNKNOWN
};

typedef struct {
    const struct SL3DDopplerItf_ *mItf;
    IObject *mThis;
    // The API allows client to specify either Cartesian and spherical velocities.
    // But an implementation will likely prefer one or the other. So for
    // maximum portablity, we maintain both units and an indication of which
    // unit was set most recently. In addition, we keep a flag saying whether
    // the other unit has been derived yet. It can take significant time
    // to compute the other unit, so this may be deferred to another thread.
    // For this reason we also keep an indication of whether the secondary
    // has been computed yet, and its accuracy.
    // Though only one unit is primary at a time, a union is inappropriate:
    // the application might read in both units (not in 1.0.1),
    // and due to multi-threading concerns.
    SLVec3D mVelocityCartesian;
    struct {
        SLmillidegree mAzimuth;
        SLmillidegree mElevation;
        SLmillidegree mSpeed;
    } mVelocitySpherical;
    enum CartesianSphericalActive mVelocityActive;
    SLpermille mDopplerFactor;
} I3DDoppler;

typedef struct {
    const struct SL3DGroupingItf_ *mItf;
    IObject *mThis;
    SLObjectItf mGroup;
} I3DGrouping;

// FIXME move
enum AnglesVectorsActive {
    ANGLES_COMPUTED_VECTORS_SET,    // not in 1.0.1
    ANGLES_REQUESTED_VECTORS_SET,   // not in 1.0.1
    ANGLES_UNKNOWN_VECTORS_SET,
    ANGLES_SET_VECTORS_COMPUTED,
    ANGLES_SET_VECTORS_REQUESTED,
    ANGLES_SET_VECTORS_UNKNOWN
};

typedef struct {
    const struct SL3DLocationItf_ *mItf;
    IObject *mThis;
    SLVec3D mLocationCartesian;
    struct {
        SLmillidegree mAzimuth;
        SLmillidegree mElevation;
        SLmillimeter mDistance;
    } mLocationSpherical;
    enum CartesianSphericalActive mLocationActive;
    struct {
        SLmillidegree mHeading;
        SLmillidegree mPitch;
        SLmillidegree mRoll;
    } mOrientationAngles;
    struct {
        SLVec3D mFront;
        SLVec3D mAbove;
        SLVec3D mUp;
    } mOrientationVectors;
    enum AnglesVectorsActive mOrientationActive;
    // Rotations can be slow, so are deferred.
    SLmillidegree mTheta;
    SLVec3D mAxis;
    SLboolean mRotatePending;
} I3DLocation;

typedef struct {
    const struct SL3DMacroscopicItf_ *mItf;
    IObject *mThis;
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
    enum AnglesVectorsActive mOrientationActive;
    // FIXME no longer needed? was for optimization
    // SLuint32 mGeneration;
    // Rotations can be slow, so are deferred.
    SLmillidegree mTheta;
    SLVec3D mAxis;
    SLboolean mRotatePending;
} I3DMacroscopic;

typedef struct {
    const struct SL3DSourceItf_ *mItf;
    IObject *mThis;
    SLboolean mHeadRelative;
    SLboolean mRolloffMaxDistanceMute;
    SLmillimeter mMaxDistance;
    SLmillimeter mMinDistance;
    SLmillidegree mConeInnerAngle;
    SLmillidegree mConeOuterAngle;
    SLmillibel mConeOuterLevel;
    SLpermille mRolloffFactor;
    SLpermille mRoomRolloffFactor;
    SLuint8 mDistanceModel;
} I3DSource;

typedef struct {
    const struct SLAudioDecoderCapabilitiesItf_ *mItf;
    IObject *mThis;
} IAudioDecoderCapabilities;

typedef struct {
    const struct SLAudioEncoderItf_ *mItf;
    IObject *mThis;
    SLAudioEncoderSettings mSettings;
} IAudioEncoder;

typedef struct {
    const struct SLAudioEncoderCapabilitiesItf_ *mItf;
    IObject *mThis;
} IAudioEncoderCapabilities;

typedef struct {
    const struct SLAudioIODeviceCapabilitiesItf_ *mItf;
    IObject *mThis;
    slAvailableAudioInputsChangedCallback mAvailableAudioInputsChangedCallback;
    void *mAvailableAudioInputsChangedContext;
    slAvailableAudioOutputsChangedCallback mAvailableAudioOutputsChangedCallback;
    void *mAvailableAudioOutputsChangedContext;
    slDefaultDeviceIDMapChangedCallback mDefaultDeviceIDMapChangedCallback;
    void *mDefaultDeviceIDMapChangedContext;
} IAudioIODeviceCapabilities;

typedef struct {
    const struct SLBassBoostItf_ *mItf;
    IObject *mThis;
    SLboolean mEnabled;
    SLpermille mStrength;
} IBassBoost;

typedef struct BufferQueue_interface {
    const struct SLBufferQueueItf_ *mItf;
    IObject *mThis;
    volatile SLBufferQueueState mState;
    slBufferQueueCallback mCallback;
    void *mContext;
    SLuint32 mNumBuffers;
    struct BufferHeader *mArray;
    struct BufferHeader *mFront, *mRear;
    SLuint32 mSizeConsumed;
    // saves a malloc in the typical case
#define BUFFER_HEADER_TYPICAL 4
    struct BufferHeader mTypical[BUFFER_HEADER_TYPICAL+1];
} IBufferQueue;

typedef struct {
    const struct SLDeviceVolumeItf_ *mItf;
    IObject *mThis;
    SLint32 mVolume[2]; // FIXME Hard-coded for default in/out
} IDeviceVolume;

typedef struct {
    const struct SLDynamicInterfaceManagementItf_ *mItf;
    IObject *mThis;
    unsigned mAddedMask;    // added interfaces, a subset of exposed interfaces
    slDynamicInterfaceManagementCallback mCallback;
    void *mContext;
} IDynamicInterfaceManagement;

typedef struct {
    const struct SLDynamicSourceItf_ *mItf;
    IObject *mThis;
    SLDataSource *mDataSource;
} IDynamicSource;

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

typedef struct {
    const struct SLEffectSendItf_ *mItf;
    IObject *mThis;
    struct OutputMix_class *mOutputMix;
    SLmillibel mDirectLevel;
    struct EnableLevel mEnableLevels[AUX_MAX];
} IEffectSend;

// private

typedef struct {
    const struct SLEngineItf_ *mItf;
    IObject *mThis;
    SLboolean mLossOfControlGlobal;
    // FIXME Per-class non-const data such as vector of created objects.
    // Each engine is its own universe.
    SLuint32 mInstanceCount;
    // Vector<Type> instances;
    // FIXME set of objects
#define INSTANCE_MAX 32 // FIXME no magic numbers
    IObject *mInstances[INSTANCE_MAX];
} IEngine;

typedef struct {
    const struct SLEngineCapabilitiesItf_ *mItf;
    IObject *mThis;
    SLboolean mThreadSafe;
} IEngineCapabilities;

typedef struct {
    const struct SLEnvironmentalReverbItf_ *mItf;
    IObject *mThis;
    SLEnvironmentalReverbSettings mProperties;
} IEnvironmentalReverb;

// FIXME move
struct EqualizerBand {
    SLmilliHertz mMin;
    SLmilliHertz mMax;
    SLmilliHertz mCenter;
    /*TBD*/ int mLevel;
};

typedef struct {
    const struct SLEqualizerItf_ *mItf;
    IObject *mThis;
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
} IEqualizer;

typedef struct {
    const struct SLLEDArrayItf_ *mItf;
    IObject *mThis;
    SLuint32 mLightMask;
    SLHSL *mColor;
    // const
    SLuint8 mCount;
} ILEDArray;

typedef struct {
    const struct SLMetadataExtractionItf_ *mItf;
    IObject *mThis;
    SLuint32 mKeySize;
    const void *mKey;
    SLuint32 mKeyEncoding;
    const SLchar *mValueLangCountry;
    SLuint32 mValueEncoding;
    SLuint8 mFilterMask;
    /*FIXME*/ int mKeyFilter;
} IMetadataExtraction;

typedef struct {
    const struct SLMetadataTraversalItf_ *mItf;
    IObject *mThis;
    SLuint32 mIndex;
    SLuint32 mMode;
    SLuint32 mCount;
    SLuint32 mSize;
} IMetadataTraversal;

typedef struct {
    const struct SLMIDIMessageItf_ *mItf;
    IObject *mThis;
    slMetaEventCallback mMetaEventCallback;
    void *mMetaEventContext;
    slMIDIMessageCallback mMessageCallback;
    void *mMessageContext;
    int /*TBD*/ mMessageTypes;
} IMIDIMessage;

typedef struct {
    const struct SLMIDIMuteSoloItf_ *mItf;
    IObject *mThis;
    SLuint16 mChannelMuteMask;
    SLuint16 mChannelSoloMask;
    SLuint32 mTrackMuteMask;
    SLuint32 mTrackSoloMask;
    // const ?
    SLuint16 mTrackCount;
} IMIDIMuteSolo;

typedef struct {
    const struct SLMIDITempoItf_ *mItf;
    IObject *mThis;
    SLuint32 mTicksPerQuarterNote;
    SLuint32 mMicrosecondsPerQuarterNote;
} IMIDITempo;

typedef struct {
    const struct SLMIDITimeItf_ *mItf;
    IObject *mThis;
    SLuint32 mDuration;
    SLuint32 mPosition;
    SLuint32 mStartTick;
    SLuint32 mNumTicks;
} IMIDITime;

typedef struct {
    const struct SLMuteSoloItf_ *mItf;
    IObject *mThis;
    SLuint32 mMuteMask;
    SLuint32 mSoloMask;
    // const
    SLuint8 mNumChannels;
} IMuteSolo;

typedef struct {
    const struct SLOutputMixItf_ *mItf;
    IObject *mThis;
    slMixDeviceChangeCallback mCallback;
    void *mContext;
#ifdef USE_OUTPUTMIXEXT
    unsigned mActiveMask;   // 1 bit per active track
    struct Track mTracks[32];
#endif
} IOutputMix;

#ifdef USE_OUTPUTMIXEXT
typedef struct {
    const struct SLOutputMixExtItf_ *mItf;
    IObject *mThis;
} IOutputMixExt;
#endif

typedef struct {
    const struct SLPitchItf_ *mItf;
    IObject *mThis;
    SLpermille mPitch;
    // const
    SLpermille mMinPitch;
    SLpermille mMaxPitch;
} IPitch;

typedef struct Play_interface {
    const struct SLPlayItf_ *mItf;
    IObject *mThis;
    volatile SLuint32 mState;
    SLmillisecond mDuration;
    SLmillisecond mPosition;
    // unsigned mPositionSamples;  // position in sample units
    slPlayCallback mCallback;
    void *mContext;
    SLuint32 mEventFlags;
    SLmillisecond mMarkerPosition;
    SLmillisecond mPositionUpdatePeriod;
} IPlay;

typedef struct {
    const struct SLPlaybackRateItf_ *mItf;
    IObject *mThis;
    SLpermille mRate;
    SLuint32 mPropertyConstraints;
    SLuint32 mProperties;
    SLpermille mMinRate;
    SLpermille mMaxRate;
    SLpermille mStepSize;
    SLuint32 mCapabilities;
} IPlaybackRate;

typedef struct {
    const struct SLPrefetchStatusItf_ *mItf;
    IObject *mThis;
    SLuint32 mStatus;
    SLpermille mLevel;
    slPrefetchCallback mCallback;
    void *mContext;
    SLuint32 mCallbackEventsMask;
    SLpermille mFillUpdatePeriod;
} IPrefetchStatus;

typedef struct {
    const struct SLPresetReverbItf_ *mItf;
    IObject *mThis;
    SLuint16 mPreset;
} IPresetReverb;

typedef struct {
    const struct SLRatePitchItf_ *mItf;
    IObject *mThis;
    SLpermille mRate;
    SLpermille mMinRate;
    SLpermille mMaxRate;
} IRatePitch;

typedef struct {
    const struct SLRecordItf_ *mItf;
    IObject *mThis;
    SLuint32 mState;
    SLmillisecond mDurationLimit;
    SLmillisecond mPosition;
    slRecordCallback mCallback;
    void *mContext;
    SLuint32 mCallbackEventsMask;
    SLmillisecond mMarkerPosition;
    SLmillisecond mPositionUpdatePeriod;
} IRecord;

typedef struct {
    const struct SLSeekItf_ *mItf;
    IObject *mThis;
    SLmillisecond mPos;
    SLboolean mLoopEnabled;
    SLmillisecond mStartPos;
    SLmillisecond mEndPos;
} ISeek;

typedef struct {
    const struct SLThreadSyncItf_ *mItf;
    IObject *mThis;
    SLboolean mInCriticalSection;
    SLboolean mWaiting;
    pthread_t mOwner;
} IThreadSync;

typedef struct {
    const struct SLVibraItf_ *mItf;
    IObject *mThis;
    SLboolean mVibrate;
    SLmilliHertz mFrequency;
    SLpermille mIntensity;
} IVibra;

typedef struct {
    const struct SLVirtualizerItf_ *mItf;
    IObject *mThis;
    SLboolean mEnabled;
    SLpermille mStrength;
} IVirtualizer;

typedef struct {
    const struct SLVisualizationItf_ *mItf;
    IObject *mThis;
    slVisualizationCallback mCallback;
    void *mContext;
    SLmilliHertz mRate;
} IVisualization;

typedef struct {
    const struct SLVolumeItf_ *mItf;
    IObject *mThis;
    SLmillibel mLevel;
    SLboolean mMute;
    SLboolean mEnableStereoPosition;
    SLpermille mStereoPosition;
} IVolume;

/* Class structures */

typedef struct {
    IObject mObject;
    IDynamicInterfaceManagement mDynamicInterfaceManagement;
    I3DLocation m3DLocation;
    I3DDoppler m3DDoppler;
    I3DSource m3DSource;
    I3DMacroscopic m3DMacroscopic;
    // FIXME set of objects
} C3DGroup;

#ifdef USE_ANDROID
/*
 * Used to define the mapping from an OpenSL ES audio player to an Android
 * media framework object
 */
enum AndroidObject_type {
    INVALID_TYPE     =-1,
    MEDIAPLAYER      = 0,
    AUDIOTRACK_PUSH  = 1,
    AUDIOTRACK_PULL  = 2,
    NUM_AUDIOPLAYER_MAP_TYPES
};
#endif

typedef struct {
    IObject mObject;
    IDynamicInterfaceManagement mDynamicInterfaceManagement;
    IPlay mPlay;
    I3DDoppler m3DDoppler;
    I3DGrouping m3DGrouping;
    I3DLocation m3DLocation;
    I3DSource m3DSource;
    IBufferQueue mBufferQueue;
    IEffectSend mEffectSend;
    IMuteSolo mMuteSolo;
    IMetadataExtraction mMetadataExtraction;
    IMetadataTraversal mMetadataTraversal;
    IPrefetchStatus mPrefetchStatus;
    IRatePitch mRatePitch;
    ISeek mSeek;
    IVolume mVolume;
    // optional interfaces
    I3DMacroscopic m3DMacroscopic;
    IBassBoost mBassBoost;
    IDynamicSource mDynamicSource;
    IEnvironmentalReverb mEnvironmentalReverb;
    IEqualizer mEqualizer;
    IPitch mPitch;
    IPresetReverb mPresetReverb;
    IPlaybackRate mPlaybackRate;
    IVirtualizer mVirtualizer;
    IVisualization mVisualization;
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
} CAudioPlayer;

typedef struct {
    // mandated interfaces
    IObject mObject;
    IDynamicInterfaceManagement mDynamicInterfaceManagement;
    IRecord mRecord;
    IAudioEncoder mAudioEncoder;
    // optional interfaces
    IBassBoost mBassBoost;
    IDynamicSource mDynamicSource;
    IEqualizer mEqualizer;
    IVisualization mVisualization;
    IVolume mVolume;
} CAudioRecorder;

typedef struct {
    // mandated implicit interfaces
    IObject mObject;
    IDynamicInterfaceManagement mDynamicInterfaceManagement;
    IEngine mEngine;
    IEngineCapabilities mEngineCapabilities;
    IThreadSync mThreadSync;
    // mandated explicit interfaces
    IAudioIODeviceCapabilities mAudioIODeviceCapabilities;
    IAudioDecoderCapabilities mAudioDecoderCapabilities;
    IAudioEncoderCapabilities mAudioEncoderCapabilities;
    I3DCommit m3DCommit;
    // optional interfaces
    IDeviceVolume mDeviceVolume;
    pthread_t mFrameThread;
} CEngine;

typedef struct {
    // mandated interfaces
    IObject mObject;
    IDynamicInterfaceManagement mDynamicInterfaceManagement;
    ILEDArray mLEDArray;
    SLuint32 mDeviceID;
} CLEDDevice;

typedef struct {
    // mandated interfaces
    IObject mObject;
    IDynamicInterfaceManagement mDynamicInterfaceManagement;
    I3DDoppler m3DDoppler;
    I3DLocation m3DLocation;
} CListener;

typedef struct {
    // mandated interfaces
    IObject mObject;
    IDynamicInterfaceManagement mDynamicInterfaceManagement;
    IDynamicSource mDynamicSource;
    IMetadataExtraction mMetadataExtraction;
    IMetadataTraversal mMetadataTraversal;
} CMetadataExtractor;

typedef struct {
    // mandated interfaces
    IObject mObject;
    IDynamicInterfaceManagement mDynamicInterfaceManagement;
    IPlay mPlay;
    I3DDoppler m3DDoppler;
    I3DGrouping m3DGrouping;
    I3DLocation m3DLocation;
    I3DSource m3DSource;
    IBufferQueue mBufferQueue;
    IEffectSend mEffectSend;
    IMuteSolo mMuteSolo;
    IMetadataExtraction mMetadataExtraction;
    IMetadataTraversal mMetadataTraversal;
    IMIDIMessage mMIDIMessage;
    IMIDITime mMIDITime;
    IMIDITempo mMIDITempo;
    IMIDIMuteSolo mMIDIMuteSolo;
    IPrefetchStatus mPrefetchStatus;
    ISeek mSeek;
    IVolume mVolume;
    // optional interfaces
    I3DMacroscopic m3DMacroscopic;
    IBassBoost mBassBoost;
    IDynamicSource mDynamicSource;
    IEnvironmentalReverb mEnvironmentalReverb;
    IEqualizer mEqualizer;
    IPitch mPitch;
    IPresetReverb mPresetReverb;
    IPlaybackRate mPlaybackRate;
    IVirtualizer mVirtualizer;
    IVisualization mVisualization;
} CMidiPlayer;

typedef struct OutputMix_class {
    // mandated interfaces
    IObject mObject;
    IDynamicInterfaceManagement mDynamicInterfaceManagement;
    IOutputMix mOutputMix;
#ifdef USE_OUTPUTMIXEXT
    IOutputMixExt mOutputMixExt;
#endif
    IEnvironmentalReverb mEnvironmentalReverb;
    IEqualizer mEqualizer;
    IPresetReverb mPresetReverb;
    IVirtualizer mVirtualizer;
    IVolume mVolume;
    // optional interfaces
    IBassBoost mBassBoost;
    IVisualization mVisualization;
} COutputMix;

typedef struct {
    // mandated interfaces
    IObject mObject;
    IDynamicInterfaceManagement mDynamicInterfaceManagement;
    IVibra mVibra;
    //
    SLuint32 mDeviceID;
} CVibraDevice;

extern const ClassTable C3DGroup_class;

struct MPH_init {
    // unsigned char mMPH;
    VoidHook mInit;
    VoidHook mDeinit;
};

#ifdef __cplusplus
extern "C" {
#endif

extern /*static*/ int IID_to_MPH(const SLInterfaceID iid);
extern /*static*/ const struct MPH_init MPH_init_table[MPH_MAX];

#ifdef __cplusplus
}
#endif
