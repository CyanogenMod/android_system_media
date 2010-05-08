/* Copyright 2010 The Android Open Source Project */

/* OpenSL ES prototype */

#include "OpenSLES.h"
#include <stddef.h> // offsetof
#include <stdlib.h> // malloc
#include <string.h> // memcmp
#include <stdio.h>  // debugging
#include <assert.h> // debugging

#include "MPH.h"
#include "MPH_to.h"

#ifdef USE_SNDFILE
#include <sndfile.h>
#endif // USE_SNDFILE

#ifdef USE_SDL
#include <SDL/SDL_audio.h>
#endif // USE_SDL

#ifdef USE_ANDROID
#include <pthread.h>
#include <unistd.h>
#include "media/AudioSystem.h"
#include "media/AudioTrack.h"
#endif

#ifdef USE_OUTPUTMIXEXT
#include "OutputMixExt.h"
#endif

/* Forward declarations */

extern const struct SLInterfaceID_ SL_IID_array[MPH_MAX];

#ifdef __cplusplus
#define this this_
#endif

/* Private types */

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
    unsigned char mInterface;
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
    // Non-const here and below should be moved to separate struct,
    // as each engine is its own universe.
    // FIXME not yet used, actually should be per engine, no?
    // SLuint32 mInstanceCount;
    // append per-class data here
    StatusHook mRealize;
    VoidHook mDestroy;
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

/* Device table (change this when you port!) */

#define DEVICE_ID_HEADSET 1

static const SLAudioOutputDescriptor AudioOutputDescriptor_headset = {
    (SLchar *) "headset",
    SL_DEVCONNECTION_ATTACHED_WIRED,
    SL_DEVSCOPE_USER,
    SL_DEVLOCATION_HEADSET,
    SL_BOOLEAN_FALSE,
    SL_SAMPLINGRATE_44_1,
    SL_SAMPLINGRATE_44_1,
    SL_BOOLEAN_TRUE,
    NULL,
    0,
    2
};

#define DEVICE_ID_HANDSFREE 2

static const SLAudioOutputDescriptor AudioOutputDescriptor_handsfree = {
    (SLchar *) "handsfree",
    SL_DEVCONNECTION_INTEGRATED,
    SL_DEVSCOPE_ENVIRONMENT,
    SL_DEVLOCATION_HANDSET,
    SL_BOOLEAN_FALSE,
    SL_SAMPLINGRATE_44_1,
    SL_SAMPLINGRATE_44_1,
    SL_BOOLEAN_TRUE,
    NULL,
    0,
    2
};

/* Interface structures */

struct _3DCommit_interface {
    const struct SL3DCommitItf_ *mItf;
    void *this;
    SLboolean mDeferred;
};

struct _3DDoppler_interface {
    const struct SL3DDopplerItf_ *mItf;
    void *this;
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
    void *this;
    SLObjectItf mGroup;
};

struct _3DLocation_interface {
    const struct SL3DLocationItf_ *mItf;
    void *this;
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
    void *this;
};

struct _3DSource_interface {
    const struct SL3DSourceItf_ *mItf;
    void *this;
};

struct AudioDecoderCapabilities_interface {
    const struct SLAudioDecoderCapabilitiesItf_ *mItf;
    void *this;
};

struct AudioEncoder_interface {
    const struct SLAudioEncoderItf_ *mItf;
    void *this;
    SLAudioEncoderSettings mSettings;
};

struct AudioEncoderCapabilities_interface {
    const struct SLAudioEncoderCapabilitiesItf_ *mItf;
    void *this;
};

struct AudioIODeviceCapabilities_interface {
    const struct SLAudioIODeviceCapabilitiesItf_ *mItf;
    void *this;
};

struct BassBoost_interface {
    const struct SLBassBoostItf_ *mItf;
    void *this;
    SLboolean mEnabled;
    SLpermille mStrength;
};

struct BufferQueue_interface {
    const struct SLBufferQueueItf_ *mItf;
    void *this;
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
    void *this;
    SLint32 mVolume;     // FIXME should be per-device
};

struct DynamicInterfaceManagement_interface {
    const struct SLDynamicInterfaceManagementItf_ *mItf;
    void *this;
    unsigned mAddedMask;    // added interfaces, a subset of exposed interfaces
    slDynamicInterfaceManagementCallback mCallback;
    void *mContext;
};

struct DynamicSource_interface {
    const struct SLDynamicSourceItf_ *mItf;
    void *this;
};

struct EffectSend_interface {
    const struct SLEffectSendItf_ *mItf;
    void *this;
};

struct Engine_interface {
    const struct SLEngineItf_ *mItf;
    void *this;
    // FIXME Per-class non-const data such as vector of created objects
};

struct EngineCapabilities_interface {
    const struct SLEngineCapabilitiesItf_ *mItf;
    void *this;
};

struct EnvironmentalReverb_interface {
    const struct SLEnvironmentalReverbItf_ *mItf;
    void *this;
};

struct Equalizer_interface {
    const struct SLEqualizerItf_ *mItf;
    void *this;
};

struct LEDArray_interface {
    const struct SLLEDArrayItf_ *mItf;
    void *this;
};

struct MetaDataExtraction_interface {
    const struct SLMetaDataExractionItf_ *mItf;
    void *this;
};

struct MetaDataTraversal_interface {
    const struct SLMetaDataTraversalItf_ *mItf;
    void *this;
};

struct MIDIMessage_interface {
    const struct SLMIDIMessageItf_ *mItf;
    void *this;
};

struct MIDIMuteSolo_interface {
    const struct SLMIDIMuteSoloItf_ *mItf;
    void *this;
};

struct MIDITempo_interface {
    const struct SLMIDITempoItf_ *mItf;
    void *this;
};

struct MIDITime_interface {
    const struct SLMIDITimeItf_ *mItf;
    void *this;
};

struct MuteSolo_interface {
    const struct SLMuteSoloItf_ *mItf;
    void *this;
};

struct Object_interface {
    const struct SLObjectItf_ *mItf;
    // probably not needed for an Object, as it is always first
    void *this;
    const struct class_ *mClass;
    volatile SLuint32 mState;
    slObjectCallback mCallback;
    void *mContext;
    unsigned mExposedMask;  // exposed interfaces
    SLint32 mPriority;
    SLboolean mPreemptable;
    // FIXME a thread lock would go here
    // FIXME also an object ID for RPC
    // FIXME and a human-readable name for debugging
};

struct OutputMix_interface {
    const struct SLOutputMixItf_ *mItf;
    void *this;
#ifdef USE_OUTPUTMIXEXT
    unsigned mActiveMask;   // 1 bit per active track
    struct Track mTracks[32];
#endif
};

#ifdef USE_OUTPUTMIXEXT
struct OutputMixExt_interface {
    const struct SLOutputMixExtItf_ *mItf;
    void *this;
};
#endif

struct Pitch_interface {
    const struct SLPitchItf_ *mItf;
    void *this;
};

struct Play_interface {
    const struct SLPlayItf_ *mItf;
    void *this;
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
    void *this;
};

struct PrefetchStatus_interface {
    const struct SLPrefetchStatusItf_ *mItf;
    void *this;
};

struct PresetReverb_interface {
    const struct SLPresetReverbItf_ *mItf;
    void *this;
    SLuint16 mPreset;
};

struct RatePitch_interface {
    const struct SLRatePitchItf_ *mItf;
    void *this;
};

struct Record_interface {
    const struct SLRecordItf_ *mItf;
    void *this;
};

struct Seek_interface {
    const struct SLSeekItf_ *mItf;
    void *this;
    SLmillisecond mPos;
    SLboolean mLoopEnabled;
    SLmillisecond mStartPos;
    SLmillisecond mEndPos;
};

struct ThreadSync_interface {
    const struct SLThreadSyncItf_ *mItf;
    void *this;
};

struct Vibra_interface {
    const struct SLVibraItf_ *mItf;
    void *this;
};

struct Virtualizer_interface {
    const struct SLVirtualizerItf_ *mItf;
    void *this;
    SLboolean mEnabled;
    SLpermille mStrength;
};

struct Visualization_interface {
    const struct SLVisualizationItf_ *mItf;
    void *this;
    slVisualizationCallback mCallback;
    void *mContext;
    SLmilliHertz mRate;
};

struct Volume_interface {
    const struct SLVolumeItf_ *mItf;
    void *this;
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
    struct MetaDataExtraction_interface mMetaDataExtraction;
    struct MetaDataTraversal_interface mMetaDataTraversal;
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
    // FIXME union per kind of AudioPlayer: MediaPlayer, AudioTrack, etc.
    android::AudioTrack *mAudioTrack;
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
    // additional fields not associated with interfaces
    SLboolean mThreadSafe;
    SLboolean mLossOfControl;
};

struct LEDDevice_class {
    // mandated interfaces
    struct Object_interface mObject;
    struct DynamicInterfaceManagement_interface mDynamicInterfaceManagement;
    struct LEDArray_interface mLED;
    //
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
    struct MetaDataExtraction_interface mMetaDataExtraction;
    struct MetaDataTraversal_interface mMetaDataTraversal;
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
    struct MetaDataExtraction_interface mMetaDataExtraction;
    struct MetaDataTraversal_interface mMetaDataTraversal;
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

/* Private functions */

// Map SLInterfaceID to its minimal perfect hash (MPH), or -1 if unknown

static int IID_to_MPH(const SLInterfaceID iid)
{
    if (&SL_IID_array[0] <= iid && &SL_IID_array[MPH_MAX] > iid)
        return iid - &SL_IID_array[0];
    if (NULL != iid) {
        // FIXME Replace this linear search by a good MPH algorithm
        const struct SLInterfaceID_ *srch = &SL_IID_array[0];
        unsigned MPH;
        for (MPH = 0; MPH < MPH_MAX; ++MPH, ++srch)
            if (!memcmp(iid, srch, sizeof(struct SLInterfaceID_)))
                return MPH;
    }
    return -1;
}

static SLresult checkInterfaces(const struct class_ *class__,
    SLuint32 numInterfaces, const SLInterfaceID *pInterfaceIds,
    const SLboolean *pInterfaceRequired, unsigned *pExposedMask)
{
    assert(NULL != class__ && NULL != pExposedMask);
    unsigned exposedMask = 0;
    const struct iid_vtable *interfaces = class__->mInterfaces;
    SLuint32 interfaceCount = class__->mInterfaceCount;
    SLuint32 i;
    // FIXME This section could be pre-computed per class
    for (i = 0; i < interfaceCount; ++i) {
        switch (interfaces[i].mInterface) {
        case INTERFACE_IMPLICIT:
            exposedMask |= 1 << i;
            break;
        default:
            break;
        }
    }
    if (0 < numInterfaces) {
        if (NULL == pInterfaceIds || NULL == pInterfaceRequired)
            return SL_RESULT_PARAMETER_INVALID;
        for (i = 0; i < numInterfaces; ++i) {
            SLInterfaceID iid = pInterfaceIds[i];
            if (NULL == iid)
                return SL_RESULT_PARAMETER_INVALID;
            int mph = IID_to_MPH(iid);
            if (mph < 0) {
                if (pInterfaceRequired[i])
                    return SL_RESULT_FEATURE_UNSUPPORTED;
                continue;
            }
            int interfaceIndex = class__->mMPH_to_index[mph];
            if (interfaceIndex < 0) {
                if (pInterfaceRequired[i])
                    return SL_RESULT_FEATURE_UNSUPPORTED;
                continue;
            }
            if (exposedMask & (1 << interfaceIndex))
#if 0 // FIXME this seems a bit strong? what is correct logic?
// we are requesting a duplicate explicit interface,
// or we are requesting one which is already implicit ?
                return SL_RESULT_PARAMETER_INVALID;
#else
                continue;
#endif
            exposedMask |= (1 << interfaceIndex);
        }
    }
    *pExposedMask = exposedMask;
    return SL_RESULT_SUCCESS;
}

/* Interface initialization hooks */

extern const struct SL3DCommitItf_ _3DCommit_3DCommitItf;

static void _3DCommit_init(void *self)
{
    struct _3DCommit_interface *this = (struct _3DCommit_interface *) self;
    this->mItf = &_3DCommit_3DCommitItf;
#ifndef NDEBUG
    this->mDeferred = SL_BOOLEAN_FALSE;
#endif
}

static void _3DDoppler_init(void *self)
{
    //struct _3DDoppler_interface *this =
        // (struct _3DDoppler_interface *) self;
}

static void _3DGrouping_init(void *self)
{
    //struct _3DGrouping_interface *this =
        // (struct _3DGrouping_interface *) self;
}

static void _3DLocation_init(void *self)
{
    //struct _3DLocation_interface *this =
        // (struct _3DLocation_interface *) self;
}

static void _3DMacroscopic_init(void *self)
{
    //struct _3DMacroscopic_interface *this =
        // (struct _3DMacroscopic_interface *) self;
}

static void _3DSource_init(void *self)
{
    //struct _3DSource_interface *this =
        // (struct _3DSource_interface *) self;
}

extern const struct SLAudioDecoderCapabilitiesItf_
    AudioDecoderCapabilities_AudioDecoderCapabilitiesItf;

static void AudioDecoderCapabilities_init(void *self)
{
    struct AudioDecoderCapabilities_interface *this =
        (struct AudioDecoderCapabilities_interface *) self;
    this->mItf = &AudioDecoderCapabilities_AudioDecoderCapabilitiesItf;
}

extern const struct SLAudioEncoderCapabilitiesItf_
    AudioEncoderCapabilities_AudioEncoderCapabilitiesItf;

static void AudioEncoderCapabilities_init(void *self)
{
    struct AudioEncoderCapabilities_interface *this =
        (struct AudioEncoderCapabilities_interface *) self;
    this->mItf = &AudioEncoderCapabilities_AudioEncoderCapabilitiesItf;
}

static void AudioEncoder_init(void *self)
{
    //struct AudioEncoder_interface *this =
        // (struct AudioEncoder_interface *) self;
}

extern const struct SLAudioIODeviceCapabilitiesItf_
    AudioIODeviceCapabilities_AudioIODeviceCapabilitiesItf;

static void AudioIODeviceCapabilities_init(void *self)
{
    struct AudioIODeviceCapabilities_interface *this =
        (struct AudioIODeviceCapabilities_interface *) self;
    this->mItf = &AudioIODeviceCapabilities_AudioIODeviceCapabilitiesItf;
}

static void BassBoost_init(void *self)
{
    //struct BassBoost_interface *this =
        // (struct BassBoost_interface *) self;
}

extern const struct SLBufferQueueItf_ BufferQueue_BufferQueueItf;

static void BufferQueue_init(void *self)
{
    struct BufferQueue_interface *this = (struct BufferQueue_interface *) self;
    this->mItf = &BufferQueue_BufferQueueItf;
#ifndef NDEBUG
    this->mState.count = 0;
    this->mState.playIndex = 0;
    this->mCallback = NULL;
    this->mContext = NULL;
    this->mNumBuffers = 0;
    this->mArray = NULL;
    this->mFront = NULL;
    this->mRear = NULL;
#endif
}

extern const struct SLDeviceVolumeItf_ DeviceVolume_DeviceVolumeItf;

static void DeviceVolume_init(void *self)
{
    struct DeviceVolume_interface *this =
        (struct DeviceVolume_interface *) self;
    this->mItf = &DeviceVolume_DeviceVolumeItf;
}

extern const struct SLDynamicInterfaceManagementItf_
    DynamicInterfaceManagement_DynamicInterfaceManagementItf;

static void DynamicInterfaceManagement_init(void *self)
{
    struct DynamicInterfaceManagement_interface *this =
        (struct DynamicInterfaceManagement_interface *) self;
    this->mItf =
        &DynamicInterfaceManagement_DynamicInterfaceManagementItf;
#ifndef NDEBUG
    this->mAddedMask = 0;
    this->mCallback = NULL;
    this->mContext = NULL;
#endif
}

static void DynamicSource_init(void *self)
{
    //struct DynamicSource_interface *this =
        // (struct DynamicSource_interface *) self;
}

static void EffectSend_init(void *self)
{
    //struct EffectSend_interface *this =
        // (struct EffectSend_interface *) self;
}

extern const struct SLEngineItf_ Engine_EngineItf;

static void Engine_init(void *self)
{
    struct Engine_interface *this = (struct Engine_interface *) self;
    this->mItf = &Engine_EngineItf;
}

extern const struct SLEngineCapabilitiesItf_
    EngineCapabilities_EngineCapabilitiesItf;

static void EngineCapabilities_init(void *self)
{
    struct EngineCapabilities_interface *this =
        (struct EngineCapabilities_interface *) self;
    this->mItf = &EngineCapabilities_EngineCapabilitiesItf;
}

static void EnvironmentalReverb_init(void *self)
{
    //struct EnvironmentalReverb_interface *this =
        //(struct EnvironmentalReverb_interface *) self;
}

static void Equalizer_init(void *self)
{
    //struct Equalizer_interface *this =
        // (struct Equalizer_interface *) self;
}

extern const struct SLLEDArrayItf_ LEDArray_LEDArrayItf;

static void LEDArray_init(void *self)
{
    struct LEDArray_interface *this = (struct LEDArray_interface *) self;
    this->mItf = &LEDArray_LEDArrayItf;
}

static void MetaDataExtraction_init(void *self)
{
    //struct MetaDataExtraction_interface *this =
        // (struct MetaDataExtraction_interface *) self;
}

static void MetaDataTraversal_init(void *self)
{
    //struct MetaDataTraversal_interface *this =
        // (struct MetaDataTraversal_interface *) self;
}

static void MIDIMessage_init(void *self)
{
    //struct MIDIMessage_interface *this =
        // (struct MIDIMessage_interface *) self;
}

static void MIDIMuteSolo_init(void *self)
{
    //struct MIDIMuteSolo_interface *this =
        // (struct MIDIMuteSolo_interface *) self;
}

static void MIDITempo_init(void *self)
{
    //struct MIDITempo_interface *this =
        // (struct MIDITempo_interface *) self;
}

static void MIDITime_init(void *self)
{
    //struct MIDITime_interface *this =
        // (struct MIDITime_interface *) self;
}

static void MuteSolo_init(void *self)
{
    //struct MuteSolo_interface *this =
        // (struct MuteSolo_interface *) self;
}

static void PrefetchStatus_init(void *self)
{
    //struct PrefetchStatus_interface *this =
        // (struct PrefetchStatus_interface *) self;
}

extern const struct SLObjectItf_ Object_ObjectItf;

static void Object_init(void *self)
{
    struct Object_interface *this = (struct Object_interface *) self;
    this->mItf = &Object_ObjectItf;
    this->mState = SL_OBJECT_STATE_UNREALIZED;
#ifndef NDEBUG
    this->mCallback = NULL;
    this->mContext = NULL;
    this->mPriority = 0;
    this->mPreemptable = SL_BOOLEAN_FALSE;
#endif
}

extern const struct SLOutputMixItf_ OutputMix_OutputMixItf;

static void OutputMix_init(void *self)
{
    struct OutputMix_interface *this = (struct OutputMix_interface *) self;
    this->mItf = &OutputMix_OutputMixItf;
#ifndef NDEBUG
    this->mActiveMask = 0;
    struct Track *track = &this->mTracks[0];
    // FIXME O(n)
    // FIXME magic number
    unsigned i;
    for (i = 0; i < 32; ++i, ++track)
        track->mPlay = NULL;
#endif
}

#ifdef USE_OUTPUTMIXEXT
extern const struct SLOutputMixExtItf_ OutputMixExt_OutputMixExtItf;

static void OutputMixExt_init(void *self)
{
    struct OutputMixExt_interface *this =
        (struct OutputMixExt_interface *) self;
    this->mItf = &OutputMixExt_OutputMixExtItf;
}
#endif // USE_OUTPUTMIXEXT

static void Pitch_init(void *self)
{
    //struct Pitch_interface *this =
        // (struct Pitch_interface *) self;
}

extern const struct SLPlayItf_ Play_PlayItf;

static void Play_init(void *self)
{
    struct Play_interface *this = (struct Play_interface *) self;
    this->mItf = &Play_PlayItf;
    this->mState = SL_PLAYSTATE_STOPPED;
    this->mDuration = SL_TIME_UNKNOWN;
#ifndef NDEBUG
    this->mPosition = (SLmillisecond) 0;
    // this->mPlay.mPositionSamples = 0;
    this->mCallback = NULL;
    this->mContext = NULL;
    this->mEventFlags = 0;
    this->mMarkerPosition = 0;
    this->mPositionUpdatePeriod = 0;
#endif
}

static void PlaybackRate_init(void *self)
{
    //struct PlaybackRate_interface *this =
        // (struct PlaybackRate_interface *) self;
}

static void PresetReverb_init(void *self)
{
    //struct PresetReverb_interface *this =
        //(struct PresetReverb_interface *) self;
}

static void RatePitch_init(void *self)
{
    //struct RatePitch_interface *this =
        // (struct RatePitch_interface *) self;
}

static void Record_init(void *self)
{
    //struct Record_interface *this = (struct Record_interface *) self;
}

extern const struct SLSeekItf_ Seek_SeekItf;

static void Seek_init(void *self)
{
    struct Seek_interface *this = (struct Seek_interface *) self;
    this->mItf = &Seek_SeekItf;
    this->mPos = (SLmillisecond) -1;
    this->mStartPos = (SLmillisecond) -1;
    this->mEndPos = (SLmillisecond) -1;
#ifndef NDEBUG
    this->mLoopEnabled = SL_BOOLEAN_FALSE;
#endif
}

extern const struct SLThreadSyncItf_ ThreadSync_ThreadSyncItf;

static void ThreadSync_init(void *self)
{
    struct ThreadSync_interface *this =
        (struct ThreadSync_interface *) self;
    this->mItf = &ThreadSync_ThreadSyncItf;
}

static void Virtualizer_init(void *self)
{
    //struct Virtualizer_interface *this =
        // (struct Virtualizer_interface *) self;
}

extern const struct SLVibraItf_ Vibra_VibraItf;

static void Vibra_init(void *self)
{
    struct Vibra_interface *this = (struct Vibra_interface *) self;
    this->mItf = &Vibra_VibraItf;
}

extern const struct SLVisualizationItf_ Visualization_VisualizationItf;

static void Visualization_init(void *self)
{
    struct Visualization_interface *this =
        (struct Visualization_interface *) self;
    this->mItf = &Visualization_VisualizationItf;
#ifndef NDEBUG
    this->mCallback = NULL;
    this->mContext = NULL;
    this->mRate = 0;
#endif
}

extern const struct SLVolumeItf_ Volume_VolumeItf;

static void Volume_init(void *self)
{
    struct Volume_interface *this = (struct Volume_interface *) self;
    this->mItf = &Volume_VolumeItf;
#ifndef NDEBUG
    this->mLevel = 0; // FIXME correct ?
    this->mMute = SL_BOOLEAN_FALSE;
    this->mEnableStereoPosition = SL_BOOLEAN_FALSE;
    this->mStereoPosition = 0;
#endif
}

static const struct MPH_init {
    // unsigned char mMPH;
    VoidHook mInit;
    VoidHook mDeinit;
} MPH_init_table[MPH_MAX] = {
    { /* MPH_3DCOMMIT, */ _3DCommit_init, NULL },
    { /* MPH_3DDOPPLER, */ _3DDoppler_init, NULL },
    { /* MPH_3DGROUPING, */ _3DGrouping_init, NULL },
    { /* MPH_3DLOCATION, */ _3DLocation_init, NULL },
    { /* MPH_3DMACROSCOPIC, */ _3DMacroscopic_init, NULL },
    { /* MPH_3DSOURCE, */ _3DSource_init, NULL },
    { /* MPH_AUDIODECODERCAPABILITIES, */ AudioDecoderCapabilities_init, NULL },
    { /* MPH_AUDIOENCODER, */ AudioEncoder_init, NULL },
    { /* MPH_AUDIOENCODERCAPABILITIES, */ AudioEncoderCapabilities_init, NULL },
    { /* MPH_AUDIOIODEVICECAPABILITIES, */ AudioIODeviceCapabilities_init,
        NULL },
    { /* MPH_BASSBOOST, */ BassBoost_init, NULL },
    { /* MPH_BUFFERQUEUE, */ BufferQueue_init, NULL },
    { /* MPH_DEVICEVOLUME, */ DeviceVolume_init, NULL },
    { /* MPH_DYNAMICINTERFACEMANAGEMENT, */ DynamicInterfaceManagement_init,
        NULL },
    { /* MPH_DYNAMICSOURCE, */ DynamicSource_init, NULL },
    { /* MPH_EFFECTSEND, */ EffectSend_init, NULL },
    { /* MPH_ENGINE, */ Engine_init, NULL },
    { /* MPH_ENGINECAPABILITIES, */ EngineCapabilities_init, NULL },
    { /* MPH_ENVIRONMENTALREVERB, */ EnvironmentalReverb_init, NULL },
    { /* MPH_EQUALIZER, */ Equalizer_init, NULL },
    { /* MPH_LED, */ LEDArray_init, NULL },
    { /* MPH_METADATAEXTRACTION, */ MetaDataExtraction_init, NULL },
    { /* MPH_METADATATRAVERSAL, */ MetaDataTraversal_init, NULL },
    { /* MPH_MIDIMESSAGE, */ MIDIMessage_init, NULL },
    { /* MPH_MIDITIME, */ MIDITime_init, NULL },
    { /* MPH_MIDITEMPO, */ MIDITempo_init, NULL },
    { /* MPH_MIDIMUTESOLO, */ MIDIMuteSolo_init, NULL },
    { /* MPH_MUTESOLO, */ MuteSolo_init, NULL },
    { /* MPH_NULL, */ NULL, NULL },
    { /* MPH_OBJECT, */ Object_init, NULL },
    { /* MPH_OUTPUTMIX, */ OutputMix_init, NULL },
    { /* MPH_PITCH, */ Pitch_init, NULL },
    { /* MPH_PLAY, */ Play_init, NULL },
    { /* MPH_PLAYBACKRATE, */ PlaybackRate_init, NULL },
    { /* MPH_PREFETCHSTATUS, */ PrefetchStatus_init, NULL },
    { /* MPH_PRESETREVERB, */ PresetReverb_init, NULL },
    { /* MPH_RATEPITCH, */ RatePitch_init, NULL },
    { /* MPH_RECORD, */ Record_init, NULL },
    { /* MPH_SEEK, */ Seek_init, NULL },
    { /* MPH_THREADSYNC, */ ThreadSync_init, NULL },
    { /* MPH_VIBRA, */ Vibra_init, NULL },
    { /* MPH_VIRTUALIZER, */ Virtualizer_init, NULL },
    { /* MPH_VISUALIZATION, */ Visualization_init, NULL },
    { /* MPH_VOLUME, */ Volume_init, NULL },
    { /* MPH_OUTPUTMIXEXT, */
#ifdef USE_OUTPUTMIXEXT
        OutputMixExt_init, NULL
#else
        NULL, NULL
#endif
        }
};

/* Classes vs. interfaces */

// 3DGroup class

static const struct iid_vtable _3DGroup_interfaces[] = {
    {MPH_OBJECT, INTERFACE_IMPLICIT,
        offsetof(struct _3DGroup_class, mObject)},
    {MPH_DYNAMICINTERFACEMANAGEMENT, INTERFACE_IMPLICIT,
        offsetof(struct _3DGroup_class, mDynamicInterfaceManagement)},
    {MPH_3DLOCATION, INTERFACE_IMPLICIT,
        offsetof(struct _3DGroup_class, m3DLocation)},
    {MPH_3DDOPPLER, INTERFACE_DYNAMIC_GAME,
        offsetof(struct _3DGroup_class, m3DDoppler)},
    {MPH_3DSOURCE, INTERFACE_GAME,
        offsetof(struct _3DGroup_class, m3DSource)},
    {MPH_3DMACROSCOPIC, INTERFACE_OPTIONAL,
        offsetof(struct _3DGroup_class, m3DMacroscopic)},
};

static const struct class_ _3DGroup_class = {
    _3DGroup_interfaces,
    sizeof(_3DGroup_interfaces)/sizeof(_3DGroup_interfaces[0]),
    MPH_to_3DGroup,
    //"3DGroup",
    sizeof(struct _3DGroup_class),
    SL_OBJECTID_3DGROUP,
    NULL,
    NULL
};

// AudioPlayer class

/* AudioPlayer private functions */

#ifdef USE_SNDFILE

// FIXME should run this asynchronously esp. for socket fd, not on mix thread
static void SLAPIENTRY SndFile_Callback(SLBufferQueueItf caller, void *pContext)
{
    struct SndFile *this = (struct SndFile *) pContext;
    SLresult result;
    if (NULL != this->mRetryBuffer && 0 < this->mRetrySize) {
        result = (*caller)->Enqueue(caller, this->mRetryBuffer,
            this->mRetrySize);
        if (SL_RESULT_BUFFER_INSUFFICIENT == result)
            return;     // what, again?
        assert(SL_RESULT_SUCCESS == result);
        this->mRetryBuffer = NULL;
        this->mRetrySize = 0;
        return;
    }
    short *pBuffer = this->mIs0 ? this->mBuffer0 : this->mBuffer1;
    this->mIs0 ^= SL_BOOLEAN_TRUE;
    sf_count_t count;
    // FIXME magic number
    count = sf_read_short(this->mSNDFILE, pBuffer, (sf_count_t) 512);
    if (0 < count) {
        SLuint32 size = count * sizeof(short);
        // FIXME if we had an internal API, could call this directly
        result = (*caller)->Enqueue(caller, pBuffer, size);
        if (SL_RESULT_BUFFER_INSUFFICIENT == result) {
            this->mRetryBuffer = pBuffer;
            this->mRetrySize = size;
            return;
        }
        assert(SL_RESULT_SUCCESS == result);
    }
}

static SLboolean SndFile_IsSupported(const SF_INFO *sfinfo)
{
    switch (sfinfo->format & SF_FORMAT_TYPEMASK) {
    case SF_FORMAT_WAV:
        break;
    default:
        return SL_BOOLEAN_FALSE;
    }
    switch (sfinfo->format & SF_FORMAT_SUBMASK) {
    case SF_FORMAT_PCM_16:
        break;
    default:
        return SL_BOOLEAN_FALSE;
    }
    switch (sfinfo->samplerate) {
    case 44100:
        break;
    default:
        return SL_BOOLEAN_FALSE;
    }
    switch (sfinfo->channels) {
    case 2:
        break;
    default:
        return SL_BOOLEAN_FALSE;
    }
    return SL_BOOLEAN_TRUE;
}

#endif // USE_SNDFILE

static SLresult AudioPlayer_Realize(void *self)
{
    struct AudioPlayer_class *this = (struct AudioPlayer_class *) self;
    SLresult result = SL_RESULT_SUCCESS;
    // for Android here is where we do setDataSource etc. for MediaPlayer
#ifdef USE_SNDFILE
    if (NULL != this->mSndFile.mPathname) {
        SF_INFO sfinfo;
        sfinfo.format = 0;
        this->mSndFile.mSNDFILE = sf_open(
            (const char *) this->mSndFile.mPathname, SFM_READ, &sfinfo);
        if (NULL == this->mSndFile.mSNDFILE) {
            result = SL_RESULT_CONTENT_NOT_FOUND;
        } else if (!SndFile_IsSupported(&sfinfo)) {
            sf_close(this->mSndFile.mSNDFILE);
            this->mSndFile.mSNDFILE = NULL;
            result = SL_RESULT_CONTENT_UNSUPPORTED;
        } else {
            SLBufferQueueItf bufferQueue = &this->mBufferQueue.mItf;
            // FIXME should use a private internal API, and disallow
            // application to have access to our buffer queue
            // FIXME if we had an internal API, could call this directly
            result = (*bufferQueue)->RegisterCallback(bufferQueue,
                SndFile_Callback, &this->mSndFile);
        }
    }
#endif // USE_SNDFILE
    return result;
}

static void AudioPlayer_Destroy(void *self)
{
    struct AudioPlayer_class *this = (struct AudioPlayer_class *) self;
    // FIXME stop the player in a way that app can't restart it
    // Free the buffer queue, if it was larger than typical
    if (NULL != this->mBufferQueue.mArray &&
        this->mBufferQueue.mArray != this->mBufferQueue.mTypical) {
        free(this->mBufferQueue.mArray);
        this->mBufferQueue.mArray = NULL;
    }
#ifdef USE_SNDFILE
    if (NULL != this->mSndFile.mSNDFILE) {
        sf_close(this->mSndFile.mSNDFILE);
        this->mSndFile.mSNDFILE = NULL;
    }
#endif // USE_SNDFILE
}

static const struct iid_vtable AudioPlayer_interfaces[] = {
    {MPH_OBJECT, INTERFACE_IMPLICIT,
        offsetof(struct AudioPlayer_class, mObject)},
    {MPH_DYNAMICINTERFACEMANAGEMENT, INTERFACE_IMPLICIT,
        offsetof(struct AudioPlayer_class, mDynamicInterfaceManagement)},
    {MPH_PLAY, INTERFACE_IMPLICIT,
        offsetof(struct AudioPlayer_class, mPlay)},
    {MPH_3DDOPPLER, INTERFACE_DYNAMIC_GAME,
        offsetof(struct AudioPlayer_class, m3DDoppler)},
    {MPH_3DGROUPING, INTERFACE_GAME,
        offsetof(struct AudioPlayer_class, m3DGrouping)},
    {MPH_3DLOCATION, INTERFACE_GAME,
        offsetof(struct AudioPlayer_class, m3DLocation)},
    {MPH_3DSOURCE, INTERFACE_GAME,
        offsetof(struct AudioPlayer_class, m3DSource)},
    // FIXME Currently we create an internal buffer queue for playing files
    {MPH_BUFFERQUEUE, /* INTERFACE_GAME */ INTERFACE_IMPLICIT,
        offsetof(struct AudioPlayer_class, mBufferQueue)},
    {MPH_EFFECTSEND, INTERFACE_MUSIC_GAME,
        offsetof(struct AudioPlayer_class, mEffectSend)},
    {MPH_MUTESOLO, INTERFACE_GAME,
        offsetof(struct AudioPlayer_class, mMuteSolo)},
    {MPH_METADATAEXTRACTION, INTERFACE_MUSIC_GAME,
        offsetof(struct AudioPlayer_class, mMetaDataExtraction)},
    {MPH_METADATATRAVERSAL, INTERFACE_MUSIC_GAME,
        offsetof(struct AudioPlayer_class, mMetaDataTraversal)},
    {MPH_PREFETCHSTATUS, INTERFACE_TBD,
        offsetof(struct AudioPlayer_class, mPrefetchStatus)},
    {MPH_RATEPITCH, INTERFACE_DYNAMIC_GAME,
        offsetof(struct AudioPlayer_class, mRatePitch)},
    {MPH_SEEK, INTERFACE_TBD,
        offsetof(struct AudioPlayer_class, mSeek)},
    {MPH_VOLUME, INTERFACE_TBD,
        offsetof(struct AudioPlayer_class, mVolume)},
    {MPH_3DMACROSCOPIC, INTERFACE_OPTIONAL,
        offsetof(struct AudioPlayer_class, m3DMacroscopic)},
    {MPH_BASSBOOST, INTERFACE_OPTIONAL,
        offsetof(struct AudioPlayer_class, mBassBoost)},
    {MPH_DYNAMICSOURCE, INTERFACE_OPTIONAL,
        offsetof(struct AudioPlayer_class, mDynamicSource)},
    {MPH_ENVIRONMENTALREVERB, INTERFACE_OPTIONAL,
        offsetof(struct AudioPlayer_class, mEnvironmentalReverb)},
    {MPH_EQUALIZER, INTERFACE_OPTIONAL,
        offsetof(struct AudioPlayer_class, mEqualizer)},
    {MPH_PITCH, INTERFACE_OPTIONAL,
        offsetof(struct AudioPlayer_class, mPitch)},
    {MPH_PRESETREVERB, INTERFACE_OPTIONAL,
        offsetof(struct AudioPlayer_class, mPresetReverb)},
    {MPH_PLAYBACKRATE, INTERFACE_OPTIONAL,
        offsetof(struct AudioPlayer_class, mPlaybackRate)},
    {MPH_VIRTUALIZER, INTERFACE_OPTIONAL,
        offsetof(struct AudioPlayer_class, mVirtualizer)},
    {MPH_VISUALIZATION, INTERFACE_OPTIONAL,
        offsetof(struct AudioPlayer_class, mVisualization)}
};

static const struct class_ AudioPlayer_class = {
    AudioPlayer_interfaces,
    sizeof(AudioPlayer_interfaces)/sizeof(AudioPlayer_interfaces[0]),
    MPH_to_AudioPlayer,
    //"AudioPlayer",
    sizeof(struct AudioPlayer_class),
    SL_OBJECTID_AUDIOPLAYER,
    AudioPlayer_Realize,
    AudioPlayer_Destroy
};

// AudioRecorder class

static const struct iid_vtable AudioRecorder_interfaces[] = {
    {MPH_OBJECT, INTERFACE_IMPLICIT,
        offsetof(struct AudioRecorder_class, mObject)},
    {MPH_DYNAMICINTERFACEMANAGEMENT, INTERFACE_IMPLICIT,
        offsetof(struct AudioRecorder_class, mDynamicInterfaceManagement)},
    {MPH_RECORD, INTERFACE_IMPLICIT,
        offsetof(struct AudioRecorder_class, mRecord)},
    {MPH_AUDIOENCODER, INTERFACE_TBD,
        offsetof(struct AudioRecorder_class, mAudioEncoder)},
    {MPH_BASSBOOST, INTERFACE_OPTIONAL,
        offsetof(struct AudioRecorder_class, mBassBoost)},
    {MPH_DYNAMICSOURCE, INTERFACE_OPTIONAL,
        offsetof(struct AudioRecorder_class, mDynamicSource)},
    {MPH_EQUALIZER, INTERFACE_OPTIONAL,
        offsetof(struct AudioRecorder_class, mEqualizer)},
    {MPH_VISUALIZATION, INTERFACE_OPTIONAL,
        offsetof(struct AudioRecorder_class, mVisualization)},
    {MPH_VOLUME, INTERFACE_OPTIONAL,
        offsetof(struct AudioRecorder_class, mVolume)}
};

static const struct class_ AudioRecorder_class = {
    AudioRecorder_interfaces,
    sizeof(AudioRecorder_interfaces)/sizeof(AudioRecorder_interfaces[0]),
    MPH_to_AudioRecorder,
    //"AudioRecorder",
    sizeof(struct AudioRecorder_class),
    SL_OBJECTID_AUDIORECORDER,
    NULL,
    NULL
};

// Engine class

static const struct iid_vtable Engine_interfaces[] = {
    {MPH_OBJECT, INTERFACE_IMPLICIT,
        offsetof(struct Engine_class, mObject)},
    {MPH_DYNAMICINTERFACEMANAGEMENT, INTERFACE_IMPLICIT,
        offsetof(struct Engine_class, mDynamicInterfaceManagement)},
    {MPH_ENGINE, INTERFACE_IMPLICIT,
        offsetof(struct Engine_class, mEngine)},
    {MPH_ENGINECAPABILITIES, INTERFACE_IMPLICIT,
        offsetof(struct Engine_class, mEngineCapabilities)},
    {MPH_THREADSYNC, INTERFACE_IMPLICIT,
        offsetof(struct Engine_class, mThreadSync)},
    {MPH_AUDIOIODEVICECAPABILITIES, INTERFACE_IMPLICIT,
        offsetof(struct Engine_class, mAudioIODeviceCapabilities)},
    {MPH_AUDIODECODERCAPABILITIES, INTERFACE_EXPLICIT,
        offsetof(struct Engine_class, mAudioDecoderCapabilities)},
    {MPH_AUDIOENCODERCAPABILITIES, INTERFACE_EXPLICIT,
        offsetof(struct Engine_class, mAudioEncoderCapabilities)},
    {MPH_3DCOMMIT, INTERFACE_EXPLICIT_GAME,
        offsetof(struct Engine_class, m3DCommit)},
    {MPH_DEVICEVOLUME, INTERFACE_OPTIONAL,
        offsetof(struct Engine_class, mDeviceVolume)}
};

static const struct class_ Engine_class = {
    Engine_interfaces,
    sizeof(Engine_interfaces)/sizeof(Engine_interfaces[0]),
    MPH_to_Engine,
    //"Engine",
    sizeof(struct Engine_class),
    SL_OBJECTID_ENGINE,
    NULL,
    NULL
};

// LEDDevice class

static const struct iid_vtable LEDDevice_interfaces[] = {
    {MPH_OBJECT, INTERFACE_IMPLICIT,
        offsetof(struct LEDDevice_class, mObject)},
    {MPH_DYNAMICINTERFACEMANAGEMENT, INTERFACE_IMPLICIT,
        offsetof(struct LEDDevice_class, mDynamicInterfaceManagement)},
    {MPH_LED, INTERFACE_IMPLICIT,
        offsetof(struct LEDDevice_class, mLED)}
};

static const struct class_ LEDDevice_class = {
    LEDDevice_interfaces,
    sizeof(LEDDevice_interfaces)/sizeof(LEDDevice_interfaces[0]),
    MPH_to_LEDDevice,
    //"LEDDevice",
    sizeof(struct LEDDevice_class),
    SL_OBJECTID_LEDDEVICE,
    NULL,
    NULL
};

// Listener class

static const struct iid_vtable Listener_interfaces[] = {
    {MPH_OBJECT, INTERFACE_IMPLICIT,
        offsetof(struct Listener_class, mObject)},
    {MPH_DYNAMICINTERFACEMANAGEMENT, INTERFACE_IMPLICIT,
        offsetof(struct Listener_class, mDynamicInterfaceManagement)},
    {MPH_3DDOPPLER, INTERFACE_DYNAMIC_GAME,
        offsetof(struct _3DGroup_class, m3DDoppler)},
    {MPH_3DLOCATION, INTERFACE_EXPLICIT_GAME,
        offsetof(struct _3DGroup_class, m3DLocation)}
};

static const struct class_ Listener_class = {
    Listener_interfaces,
    sizeof(Listener_interfaces)/sizeof(Listener_interfaces[0]),
    MPH_to_Listener,
    //"Listener",
    sizeof(struct Listener_class),
    SL_OBJECTID_LISTENER,
    NULL,
    NULL
};

// MetadataExtractor class

static const struct iid_vtable MetadataExtractor_interfaces[] = {
    {MPH_OBJECT, INTERFACE_IMPLICIT,
        offsetof(struct MetadataExtractor_class, mObject)},
    {MPH_DYNAMICINTERFACEMANAGEMENT, INTERFACE_IMPLICIT,
        offsetof(struct MetadataExtractor_class, mDynamicInterfaceManagement)},
    {MPH_DYNAMICSOURCE, INTERFACE_IMPLICIT,
        offsetof(struct MetadataExtractor_class, mDynamicSource)},
    {MPH_METADATAEXTRACTION, INTERFACE_IMPLICIT,
        offsetof(struct MetadataExtractor_class, mMetaDataExtraction)},
    {MPH_METADATATRAVERSAL, INTERFACE_IMPLICIT,
        offsetof(struct MetadataExtractor_class, mMetaDataTraversal)}
};

static const struct class_ MetadataExtractor_class = {
    MetadataExtractor_interfaces,
    sizeof(MetadataExtractor_interfaces) /
        sizeof(MetadataExtractor_interfaces[0]),
    MPH_to_MetadataExtractor,
    //"MetadataExtractor",
    sizeof(struct MetadataExtractor_class),
    SL_OBJECTID_METADATAEXTRACTOR,
    NULL,
    NULL
};

// MidiPlayer class

static const struct iid_vtable MidiPlayer_interfaces[] = {
    {MPH_OBJECT, INTERFACE_IMPLICIT,
        offsetof(struct MidiPlayer_class, mObject)},
    {MPH_DYNAMICINTERFACEMANAGEMENT, INTERFACE_IMPLICIT,
        offsetof(struct MidiPlayer_class, mDynamicInterfaceManagement)},
    {MPH_PLAY, INTERFACE_IMPLICIT,
        offsetof(struct MidiPlayer_class, mPlay)},
    {MPH_3DDOPPLER, INTERFACE_DYNAMIC_GAME,
        offsetof(struct _3DGroup_class, m3DDoppler)},
    {MPH_3DGROUPING, INTERFACE_GAME,
        offsetof(struct MidiPlayer_class, m3DGrouping)},
    {MPH_3DLOCATION, INTERFACE_GAME,
        offsetof(struct MidiPlayer_class, m3DLocation)},
    {MPH_3DSOURCE, INTERFACE_GAME,
        offsetof(struct MidiPlayer_class, m3DSource)},
    {MPH_BUFFERQUEUE, INTERFACE_GAME,
        offsetof(struct MidiPlayer_class, mBufferQueue)},
    {MPH_EFFECTSEND, INTERFACE_GAME,
        offsetof(struct MidiPlayer_class, mEffectSend)},
    {MPH_MUTESOLO, INTERFACE_GAME,
        offsetof(struct MidiPlayer_class, mMuteSolo)},
    {MPH_METADATAEXTRACTION, INTERFACE_GAME,
        offsetof(struct MidiPlayer_class, mMetaDataExtraction)},
    {MPH_METADATATRAVERSAL, INTERFACE_GAME,
        offsetof(struct MidiPlayer_class, mMetaDataTraversal)},
    {MPH_MIDIMESSAGE, INTERFACE_PHONE_GAME,
        offsetof(struct MidiPlayer_class, mMIDIMessage)},
    {MPH_MIDITIME, INTERFACE_PHONE_GAME,
        offsetof(struct MidiPlayer_class, mMIDITime)},
    {MPH_MIDITEMPO, INTERFACE_PHONE_GAME,
        offsetof(struct MidiPlayer_class, mMIDITempo)},
    {MPH_MIDIMUTESOLO, INTERFACE_GAME,
        offsetof(struct MidiPlayer_class, mMIDIMuteSolo)},
    {MPH_PREFETCHSTATUS, INTERFACE_PHONE_GAME,
        offsetof(struct MidiPlayer_class, mPrefetchStatus)},
    {MPH_SEEK, INTERFACE_PHONE_GAME,
        offsetof(struct MidiPlayer_class, mSeek)},
    {MPH_VOLUME, INTERFACE_PHONE_GAME,
        offsetof(struct MidiPlayer_class, mVolume)},
    {MPH_3DMACROSCOPIC, INTERFACE_OPTIONAL,
        offsetof(struct MidiPlayer_class, m3DMacroscopic)},
    {MPH_BASSBOOST, INTERFACE_OPTIONAL,
        offsetof(struct MidiPlayer_class, mBassBoost)},
    {MPH_DYNAMICSOURCE, INTERFACE_OPTIONAL,
        offsetof(struct MidiPlayer_class, mDynamicSource)},
    {MPH_ENVIRONMENTALREVERB, INTERFACE_OPTIONAL,
        offsetof(struct MidiPlayer_class, mEnvironmentalReverb)},
    {MPH_EQUALIZER, INTERFACE_OPTIONAL,
        offsetof(struct MidiPlayer_class, mEqualizer)},
    {MPH_PITCH, INTERFACE_OPTIONAL,
        offsetof(struct MidiPlayer_class, mPitch)},
    {MPH_PRESETREVERB, INTERFACE_OPTIONAL,
        offsetof(struct MidiPlayer_class, mPresetReverb)},
    {MPH_PLAYBACKRATE, INTERFACE_OPTIONAL,
        offsetof(struct MidiPlayer_class, mPlaybackRate)},
    {MPH_VIRTUALIZER, INTERFACE_OPTIONAL,
        offsetof(struct MidiPlayer_class, mVirtualizer)},
    {MPH_VISUALIZATION, INTERFACE_OPTIONAL,
        offsetof(struct MidiPlayer_class, mVisualization)}
};

static const struct class_ MidiPlayer_class = {
    MidiPlayer_interfaces,
    sizeof(MidiPlayer_interfaces)/sizeof(MidiPlayer_interfaces[0]),
    MPH_to_MidiPlayer,
    //"MidiPlayer",
    sizeof(struct MidiPlayer_class),
    SL_OBJECTID_MIDIPLAYER,
    NULL,
    NULL
};

// OutputMix class

static const struct iid_vtable OutputMix_interfaces[] = {
    {MPH_OBJECT, INTERFACE_IMPLICIT,
        offsetof(struct OutputMix_class, mObject)},
    {MPH_DYNAMICINTERFACEMANAGEMENT, INTERFACE_IMPLICIT,
        offsetof(struct OutputMix_class, mDynamicInterfaceManagement)},
    {MPH_OUTPUTMIX, INTERFACE_IMPLICIT,
        offsetof(struct OutputMix_class, mOutputMix)},
#ifdef USE_OUTPUTMIXEXT
    {MPH_OUTPUTMIXEXT, INTERFACE_IMPLICIT,
        offsetof(struct OutputMix_class, mOutputMixExt)},
#else
    {MPH_OUTPUTMIXEXT, INTERFACE_TBD /*NOT AVAIL*/, 0},
#endif
    {MPH_ENVIRONMENTALREVERB, INTERFACE_DYNAMIC_GAME,
        offsetof(struct OutputMix_class, mEnvironmentalReverb)},
    {MPH_EQUALIZER, INTERFACE_DYNAMIC_MUSIC_GAME,
        offsetof(struct OutputMix_class, mEqualizer)},
    {MPH_PRESETREVERB, INTERFACE_DYNAMIC_MUSIC,
        offsetof(struct OutputMix_class, mPresetReverb)},
    {MPH_VIRTUALIZER, INTERFACE_DYNAMIC_MUSIC_GAME,
        offsetof(struct OutputMix_class, mVirtualizer)},
    {MPH_VOLUME, INTERFACE_GAME_MUSIC,
        offsetof(struct OutputMix_class, mVolume)},
    {MPH_BASSBOOST, INTERFACE_OPTIONAL_DYNAMIC,
        offsetof(struct OutputMix_class, mBassBoost)},
    {MPH_VISUALIZATION, INTERFACE_OPTIONAL,
        offsetof(struct OutputMix_class, mVisualization)}
};

static const struct class_ OutputMix_class = {
    OutputMix_interfaces,
    sizeof(OutputMix_interfaces)/sizeof(OutputMix_interfaces[0]),
    MPH_to_OutputMix,
    //"OutputMix",
    sizeof(struct OutputMix_class),
    SL_OBJECTID_OUTPUTMIX,
    NULL,
    NULL
};

// Vibra class

static const struct iid_vtable VibraDevice_interfaces[] = {
    {MPH_OBJECT, INTERFACE_OPTIONAL,
        offsetof(struct VibraDevice_class, mObject)},
    {MPH_DYNAMICINTERFACEMANAGEMENT, INTERFACE_OPTIONAL,
        offsetof(struct VibraDevice_class, mDynamicInterfaceManagement)},
    {MPH_VIBRA, INTERFACE_OPTIONAL,
        offsetof(struct VibraDevice_class, mVibra)}
};

static const struct class_ VibraDevice_class = {
    VibraDevice_interfaces,
    sizeof(VibraDevice_interfaces)/sizeof(VibraDevice_interfaces[0]),
    MPH_to_Vibra,
    //"VibraDevice",
    sizeof(struct VibraDevice_class),
    SL_OBJECTID_VIBRADEVICE,
    NULL,
    NULL
};

/* Map SL_OBJECTID to class */

static const struct class_ * const classes[] = {
    // Do not change order of these entries; they are in numerical order
    &Engine_class,
    &LEDDevice_class,
    &AudioPlayer_class,
    &AudioRecorder_class,
    &MidiPlayer_class,
    &Listener_class,
    &_3DGroup_class,
    &VibraDevice_class,
    &OutputMix_class,
    &MetadataExtractor_class
};

static const struct class_ *objectIDtoClass(SLuint32 objectID)
{
    SLuint32 objectID0 = classes[0]->mObjectID;
    if (objectID0 <= objectID &&
        classes[sizeof(classes)/sizeof(classes[0])-1]->mObjectID >= objectID)
        return classes[objectID - objectID0];
    return NULL;
}

// Construct a new instance of the specified class, exposing selected interfaces

static void *construct(const struct class_ *class__, unsigned exposedMask)
{
    void *this;
#ifndef NDEBUG
    this = malloc(class__->mSize);
#else
    this = calloc(1, class__->mSize);
#endif
    if (NULL != this) {
#ifndef NDEBUG
        // for debugging, to detect uninitialized fields
        memset(this, 0x55, class__->mSize);
#endif
        ((struct Object_interface *) this)->mClass = class__;
        ((struct Object_interface *) this)->mExposedMask = exposedMask;
        const struct iid_vtable *x = class__->mInterfaces;
        unsigned i;
        for (i = 0; exposedMask; ++i, ++x, exposedMask >>= 1) {
            if (exposedMask & 1) {
                unsigned MPH = x->mMPH;
                size_t offset = x->mOffset;
                void *self = (char *) this + offset;
                ((void **) self)[1] = this;
                VoidHook init = MPH_init_table[MPH].mInit;
                if (NULL != init)
                    (*init)(self);
            }
        }
    }
    return this;
}

/* Interface implementations */

// FIXME Sort by interface

/* Object implementation */

static SLresult Object_Realize(SLObjectItf self, SLboolean async)
{
    struct Object_interface *this = (struct Object_interface *) self;
    // FIXME locking needed here in case two threads call Realize at once
    if (SL_OBJECT_STATE_UNREALIZED != this->mState)
        return SL_RESULT_PRECONDITIONS_VIOLATED;
    const struct class_ *class__ = this->mClass;
    StatusHook realize = class__->mRealize;
    SLresult result;
    // FIXME This should be done asynchronously if requested
    result = NULL != realize ?  (*realize)(this) : SL_RESULT_SUCCESS;
    if (SL_RESULT_SUCCESS == result)
        this->mState = SL_OBJECT_STATE_REALIZED;
    if (async && NULL != this->mCallback)
        (*this->mCallback)(self, this->mContext,
        SL_OBJECT_EVENT_ASYNC_TERMINATION, result, this->mState, NULL);
    return result;
}

static SLresult Object_Resume(SLObjectItf self, SLboolean async)
{
    // FIXME process async callback
    return SL_RESULT_SUCCESS;
}

static SLresult Object_GetState(SLObjectItf self, SLuint32 *pState)
{
    if (NULL == pState)
        return SL_RESULT_PARAMETER_INVALID;
    struct Object_interface *this = (struct Object_interface *) self;
    *pState = this->mState;
    return SL_RESULT_SUCCESS;
}

static SLresult Object_GetInterface(SLObjectItf self, const SLInterfaceID iid,
    void *pInterface)
{
    if (NULL == iid || NULL == pInterface)
        return SL_RESULT_PARAMETER_INVALID;
    struct Object_interface *this = (struct Object_interface *) self;
    if (SL_OBJECT_STATE_REALIZED != this->mState)
        return SL_RESULT_PRECONDITIONS_VIOLATED;
    const struct class_ *class__ = this->mClass;
    int MPH = IID_to_MPH(iid);
    if (0 > MPH)
        return SL_RESULT_FEATURE_UNSUPPORTED;
    int index = class__->mMPH_to_index[MPH];
    if (0 > index)
        return SL_RESULT_FEATURE_UNSUPPORTED;
    unsigned mask = 1 << index;
    if (!(this->mExposedMask & mask))
        return SL_RESULT_FEATURE_UNSUPPORTED;
// FIXME code review on 2010/04/16
// I think it is "this->this" instead of "this" at line ### :
// *(void **)pInterface = (char *) this + class__->mInterfaces[index].offset;
    *(void **)pInterface = (char *) this + class__->mInterfaces[index].mOffset;
    // FIXME Should note that interface has been gotten,
    // and detect use of ill-gotten interfaces
    return SL_RESULT_SUCCESS;
}

static SLresult Object_RegisterCallback(SLObjectItf self,
    slObjectCallback callback, void *pContext)
{
    struct Object_interface *this = (struct Object_interface *) self;
    this->mCallback = callback;
    this->mContext = pContext;
    return SL_RESULT_SUCCESS;
}

static void Object_AbortAsyncOperation(SLObjectItf self)
{
}

static void Object_Destroy(SLObjectItf self)
{
    Object_AbortAsyncOperation(self);
    struct Object_interface *this = (struct Object_interface *) self;
    const struct class_ *class__ = this->mClass;
    VoidHook destroy = class__->mDestroy;
    if (NULL != destroy)
        (*destroy)(this);
    // FIXME call the deinitializer for each currently exposed interface,
    // whether it is implicit, explicit, optional, or dynamically added
#ifndef NDEBUG
    memset(this, 0x55, this->mClass->mSize);
#endif
    // redundant: this->mState = SL_OBJECT_STATE_UNREALIZED;
    free(this);
}

static SLresult Object_SetPriority(SLObjectItf self, SLint32 priority,
    SLboolean preemptable)
{
    struct Object_interface *this = (struct Object_interface *) self;
    this->mPriority = priority;
    this->mPreemptable = preemptable;
    return SL_RESULT_SUCCESS;
}

static SLresult Object_GetPriority(SLObjectItf self, SLint32 *pPriority,
    SLboolean *pPreemptable)
{
    if (NULL == pPriority || NULL == pPreemptable)
        return SL_RESULT_PARAMETER_INVALID;
    struct Object_interface *this = (struct Object_interface *) self;
    *pPriority = this->mPriority;
    *pPreemptable = this->mPreemptable;
    return SL_RESULT_SUCCESS;
}

static SLresult Object_SetLossOfControlInterfaces(SLObjectItf self,
    SLint16 numInterfaces, SLInterfaceID *pInterfaceIDs, SLboolean enabled)
{
    return SL_RESULT_SUCCESS;
}

/*static*/ const struct SLObjectItf_ Object_ObjectItf = {
    Object_Realize,
    Object_Resume,
    Object_GetState,
    Object_GetInterface,
    Object_RegisterCallback,
    Object_AbortAsyncOperation,
    Object_Destroy,
    Object_SetPriority,
    Object_GetPriority,
    Object_SetLossOfControlInterfaces,
};

/* DynamicInterfaceManagement implementation */

static SLresult DynamicInterfaceManagement_AddInterface(
    SLDynamicInterfaceManagementItf self, const SLInterfaceID iid,
    SLboolean async)
{
    if (NULL == iid)
        return SL_RESULT_PARAMETER_INVALID;
    struct DynamicInterfaceManagement_interface *this =
        (struct DynamicInterfaceManagement_interface *) self;
    struct Object_interface *thisObject =
        (struct Object_interface *) this->this;
    const struct class_ *class__ = thisObject->mClass;
    int MPH = IID_to_MPH(iid);
    if (0 > MPH)
        return SL_RESULT_FEATURE_UNSUPPORTED;
    int index = class__->mMPH_to_index[MPH];
    if (0 > index)
        return SL_RESULT_FEATURE_UNSUPPORTED;
    unsigned mask = 1 << index;
    if (thisObject->mExposedMask & mask)
        return SL_RESULT_PRECONDITIONS_VIOLATED;
    // FIXME Currently do initialization here, but might be asynchronous
    const struct iid_vtable *x = &class__->mInterfaces[index];
    size_t offset = x->mOffset;
    void *thisItf = (char *) thisObject + offset;
    size_t size = ((SLuint32) (index + 1) == class__->mInterfaceCount ?
        class__->mSize : x[1].mOffset) - offset;
#ifndef NDEBUG
// for debugging, to detect uninitialized fields
#define FILLER 0x55
#else
#define FILLER 0
#endif
    memset(thisItf, FILLER, size);
    ((void **) thisItf)[1] = thisObject;
    VoidHook init = MPH_init_table[MPH].mInit;
    if (NULL != init)
        (*init)(thisItf);
    thisObject->mExposedMask |= mask;
    this->mAddedMask |= mask;
    SLresult result = SL_RESULT_SUCCESS;
    if (async && NULL != this->mCallback) {
        (*this->mCallback)(self, this->mContext,
            SL_DYNAMIC_ITF_EVENT_RESOURCES_AVAILABLE, result, iid);
    }
    return result;
}

static SLresult DynamicInterfaceManagement_RemoveInterface(
    SLDynamicInterfaceManagementItf self, const SLInterfaceID iid)
{
    if (NULL == iid)
        return SL_RESULT_PARAMETER_INVALID;
    struct DynamicInterfaceManagement_interface *this =
        (struct DynamicInterfaceManagement_interface *) self;
    struct Object_interface *thisObject =
        (struct Object_interface *) this->this;
    const struct class_ *class__ = thisObject->mClass;
    int MPH = IID_to_MPH(iid);
    if (MPH < 0)
        return SL_RESULT_PRECONDITIONS_VIOLATED;
    int index = class__->mMPH_to_index[MPH];
    if (index < 0)
        return SL_RESULT_PRECONDITIONS_VIOLATED;
    unsigned mask = 1 << index;
    // disallow removal of non-dynamic interfaces
    if (!(this->mAddedMask & mask))
        return SL_RESULT_PRECONDITIONS_VIOLATED;
    // FIXME Currently do de-initialization here, but might be asynchronous
    const struct iid_vtable *x = &class__->mInterfaces[index];
    size_t offset = x->mOffset;
    void *thisItf = (char *) thisObject + offset;
    VoidHook deinit = MPH_init_table[MPH].mDeinit;
    if (NULL != deinit)
        (*deinit)(thisItf);
    size_t size = ((SLuint32) (index + 1) == class__->mInterfaceCount ?
        class__->mSize : x[1].mOffset) - offset;
#ifndef NDEBUG
    memset(thisItf, 0x55, size);
#endif
    thisObject->mExposedMask &= ~mask;
    this->mAddedMask &= ~mask;
    return SL_RESULT_SUCCESS;
}

static SLresult DynamicInterfaceManagement_ResumeInterface(
    SLDynamicInterfaceManagementItf self,
    const SLInterfaceID iid, SLboolean async)
{
    return SL_RESULT_SUCCESS;
}

static SLresult DynamicInterfaceManagement_RegisterCallback(
    SLDynamicInterfaceManagementItf self,
    slDynamicInterfaceManagementCallback callback, void *pContext)
{
    struct DynamicInterfaceManagement_interface *this =
        (struct DynamicInterfaceManagement_interface *) self;
    this->mCallback = callback;
    this->mContext = pContext;
    return SL_RESULT_SUCCESS;
}

/*static*/ const struct SLDynamicInterfaceManagementItf_
DynamicInterfaceManagement_DynamicInterfaceManagementItf = {
    DynamicInterfaceManagement_AddInterface,
    DynamicInterfaceManagement_RemoveInterface,
    DynamicInterfaceManagement_ResumeInterface,
    DynamicInterfaceManagement_RegisterCallback
};

/* Play implementation */

static SLresult Play_SetPlayState(SLPlayItf self, SLuint32 state)
{
    switch (state) {
    case SL_PLAYSTATE_STOPPED:
    case SL_PLAYSTATE_PAUSED:
    case SL_PLAYSTATE_PLAYING:
        break;
    default:
        return SL_RESULT_PARAMETER_INVALID;
    }
    struct Play_interface *this = (struct Play_interface *) self;
    this->mState = state;
    if (SL_PLAYSTATE_STOPPED == state) {
        this->mPosition = (SLmillisecond) 0;
        // this->mPositionSamples = 0;
    }
    return SL_RESULT_SUCCESS;
}

static SLresult Play_GetPlayState(SLPlayItf self, SLuint32 *pState)
{
    if (NULL == pState)
        return SL_RESULT_PARAMETER_INVALID;
    struct Play_interface *this = (struct Play_interface *) self;
    *pState = this->mState;
    return SL_RESULT_SUCCESS;
}

static SLresult Play_GetDuration(SLPlayItf self, SLmillisecond *pMsec)
{
    // FIXME: for SNDFILE only, check to see if already know duration
    // if so, good, otherwise save position,
    // read quickly to end of file, counting frames,
    // use sample rate to compute duration, then seek back to current position
    if (NULL == pMsec)
        return SL_RESULT_PARAMETER_INVALID;
    struct Play_interface *this = (struct Play_interface *) self;
    *pMsec = this->mDuration;
    return SL_RESULT_SUCCESS;
}

static SLresult Play_GetPosition(SLPlayItf self, SLmillisecond *pMsec)
{
    if (NULL == pMsec)
        return SL_RESULT_PARAMETER_INVALID;
    struct Play_interface *this = (struct Play_interface *) self;
    *pMsec = this->mPosition;
    // FIXME convert sample units to time units
    // SL_TIME_UNKNOWN == this->mPlay.mPosition = SL_TIME_UNKNOWN;
    return SL_RESULT_SUCCESS;
}

static SLresult Play_RegisterCallback(SLPlayItf self, slPlayCallback callback,
    void *pContext)
{
    struct Play_interface *this = (struct Play_interface *) self;
    this->mCallback = callback;
    this->mContext = pContext;
    return SL_RESULT_SUCCESS;
}

static SLresult Play_SetCallbackEventsMask(SLPlayItf self, SLuint32 eventFlags)
{
    struct Play_interface *this = (struct Play_interface *) self;
    this->mEventFlags = eventFlags;
    return SL_RESULT_SUCCESS;
}

static SLresult Play_GetCallbackEventsMask(SLPlayItf self,
    SLuint32 *pEventFlags)
{
    if (NULL == pEventFlags)
        return SL_RESULT_PARAMETER_INVALID;
    struct Play_interface *this = (struct Play_interface *) self;
    *pEventFlags = this->mEventFlags;
    return SL_RESULT_SUCCESS;
}

static SLresult Play_SetMarkerPosition(SLPlayItf self, SLmillisecond mSec)
{
    struct Play_interface *this = (struct Play_interface *) self;
    this->mMarkerPosition = mSec;
    return SL_RESULT_SUCCESS;
}

static SLresult Play_ClearMarkerPosition(SLPlayItf self)
{
    struct Play_interface *this = (struct Play_interface *) self;
    this->mMarkerPosition = 0;
    return SL_RESULT_SUCCESS;
}

static SLresult Play_GetMarkerPosition(SLPlayItf self, SLmillisecond *pMsec)
{
    if (NULL == pMsec)
        return SL_RESULT_PARAMETER_INVALID;
    struct Play_interface *this = (struct Play_interface *) self;
    *pMsec = this->mMarkerPosition;
    return SL_RESULT_SUCCESS;
}

static SLresult Play_SetPositionUpdatePeriod(SLPlayItf self, SLmillisecond mSec)
{
    struct Play_interface *this = (struct Play_interface *) self;
    this->mPositionUpdatePeriod = mSec;
    return SL_RESULT_SUCCESS;
}

static SLresult Play_GetPositionUpdatePeriod(SLPlayItf self,
    SLmillisecond *pMsec)
{
    if (NULL == pMsec)
        return SL_RESULT_PARAMETER_INVALID;
    struct Play_interface *this = (struct Play_interface *) self;
    *pMsec = this->mPositionUpdatePeriod;
    return SL_RESULT_SUCCESS;
}

/*static*/ const struct SLPlayItf_ Play_PlayItf = {
    Play_SetPlayState,
    Play_GetPlayState,
    Play_GetDuration,
    Play_GetPosition,
    Play_RegisterCallback,
    Play_SetCallbackEventsMask,
    Play_GetCallbackEventsMask,
    Play_SetMarkerPosition,
    Play_ClearMarkerPosition,
    Play_GetMarkerPosition,
    Play_SetPositionUpdatePeriod,
    Play_GetPositionUpdatePeriod
};

/* BufferQueue implementation */

static SLresult BufferQueue_Enqueue(SLBufferQueueItf self, const void *pBuffer,
    SLuint32 size)
{
    if (NULL == pBuffer)
        return SL_RESULT_PARAMETER_INVALID;
    struct BufferQueue_interface *this = (struct BufferQueue_interface *) self;
    // FIXME race condition need mutex
    struct BufferHeader *oldRear = this->mRear;
    struct BufferHeader *newRear = oldRear;
    if (++newRear == &this->mArray[this->mNumBuffers])
        newRear = this->mArray;
    if (newRear == this->mFront)
        return SL_RESULT_BUFFER_INSUFFICIENT;
    oldRear->mBuffer = pBuffer;
    oldRear->mSize = size;
    this->mRear = newRear;
    ++this->mState.count;
    return SL_RESULT_SUCCESS;
}

static SLresult BufferQueue_Clear(SLBufferQueueItf self)
{
    struct BufferQueue_interface *this = (struct BufferQueue_interface *) self;
    this->mFront = &this->mArray[0];
    this->mRear = &this->mArray[0];
    return SL_RESULT_SUCCESS;
}

static SLresult BufferQueue_GetState(SLBufferQueueItf self,
    SLBufferQueueState *pState)
{
    if (NULL == pState)
        return SL_RESULT_PARAMETER_INVALID;
    struct BufferQueue_interface *this = (struct BufferQueue_interface *) self;
#ifdef __cplusplus
    pState->count = this->mState.count;
    pState->playIndex = this->mState.playIndex;
#else
    *pState = this->mState;
#endif
    return SL_RESULT_SUCCESS;
}

static SLresult BufferQueue_RegisterCallback(SLBufferQueueItf self,
    slBufferQueueCallback callback, void *pContext)
{
    struct BufferQueue_interface *this = (struct BufferQueue_interface *) self;
    this->mCallback = callback;
    this->mContext = pContext;
    return SL_RESULT_SUCCESS;
}

/*static*/ const struct SLBufferQueueItf_ BufferQueue_BufferQueueItf = {
    BufferQueue_Enqueue,
    BufferQueue_Clear,
    BufferQueue_GetState,
    BufferQueue_RegisterCallback
};

/* Volume implementation */

static SLresult Volume_SetVolumeLevel(SLVolumeItf self, SLmillibel level)
{
    // stet despite warning because MIN and MAX might change, and because
    // some compilers allow a wider int type to be passed as a parameter
    if (!((SL_MILLIBEL_MIN <= level) && (SL_MILLIBEL_MAX >= level)))
        return SL_RESULT_PARAMETER_INVALID;
    struct Volume_interface *this = (struct Volume_interface *) self;
    this->mLevel = level;
    return SL_RESULT_SUCCESS;
}

static SLresult Volume_GetVolumeLevel(SLVolumeItf self, SLmillibel *pLevel)
{
    if (NULL == pLevel)
        return SL_RESULT_PARAMETER_INVALID;
    struct Volume_interface *this = (struct Volume_interface *) self;
    *pLevel = this->mLevel;
    return SL_RESULT_SUCCESS;
}

static SLresult Volume_GetMaxVolumeLevel(SLVolumeItf self,
    SLmillibel *pMaxLevel)
{
    if (NULL == pMaxLevel)
        return SL_RESULT_PARAMETER_INVALID;
    *pMaxLevel = SL_MILLIBEL_MAX;
    return SL_RESULT_SUCCESS;
}

static SLresult Volume_SetMute(SLVolumeItf self, SLboolean mute)
{
    struct Volume_interface *this = (struct Volume_interface *) self;
    this->mMute = mute;
    return SL_RESULT_SUCCESS;
}

static SLresult Volume_GetMute(SLVolumeItf self, SLboolean *pMute)
{
    if (NULL == pMute)
        return SL_RESULT_PARAMETER_INVALID;
    struct Volume_interface *this = (struct Volume_interface *) self;
    *pMute = this->mMute;
    return SL_RESULT_SUCCESS;
}

static SLresult Volume_EnableStereoPosition(SLVolumeItf self, SLboolean enable)
{
    struct Volume_interface *this = (struct Volume_interface *) self;
    this->mEnableStereoPosition = enable;
    return SL_RESULT_SUCCESS;
}

static SLresult Volume_IsEnabledStereoPosition(SLVolumeItf self,
    SLboolean *pEnable)
{
    if (NULL == pEnable)
        return SL_RESULT_PARAMETER_INVALID;
    struct Volume_interface *this = (struct Volume_interface *) self;
    *pEnable = this->mEnableStereoPosition;
    return SL_RESULT_SUCCESS;
}

static SLresult Volume_SetStereoPosition(SLVolumeItf self,
    SLpermille stereoPosition)
{
    struct Volume_interface *this = (struct Volume_interface *) self;
    if (!((-1000 <= stereoPosition) && (1000 >= stereoPosition)))
        return SL_RESULT_PARAMETER_INVALID;
    this->mStereoPosition = stereoPosition;
    return SL_RESULT_SUCCESS;
}

static SLresult Volume_GetStereoPosition(SLVolumeItf self,
    SLpermille *pStereoPosition)
{
    if (NULL == pStereoPosition)
        return SL_RESULT_PARAMETER_INVALID;
    struct Volume_interface *this = (struct Volume_interface *) self;
    *pStereoPosition = this->mStereoPosition;
    return SL_RESULT_SUCCESS;
}

/*static*/ const struct SLVolumeItf_ Volume_VolumeItf = {
    Volume_SetVolumeLevel,
    Volume_GetVolumeLevel,
    Volume_GetMaxVolumeLevel,
    Volume_SetMute,
    Volume_GetMute,
    Volume_EnableStereoPosition,
    Volume_IsEnabledStereoPosition,
    Volume_SetStereoPosition,
    Volume_GetStereoPosition
};

/* Engine implementation */

static SLresult Engine_CreateLEDDevice(SLEngineItf self, SLObjectItf *pDevice,
    SLuint32 deviceID, SLuint32 numInterfaces,
    const SLInterfaceID *pInterfaceIds, const SLboolean *pInterfaceRequired)
{
    if (NULL == pDevice || SL_DEFAULTDEVICEID_LED != deviceID)
        return SL_RESULT_PARAMETER_INVALID;
    *pDevice = NULL;
    unsigned exposedMask;
    SLresult result = checkInterfaces(&LEDDevice_class, numInterfaces,
        pInterfaceIds, pInterfaceRequired, &exposedMask);
    if (SL_RESULT_SUCCESS != result)
        return result;
    struct LEDDevice_class *this =
        (struct LEDDevice_class *) construct(&LEDDevice_class, exposedMask);
    if (NULL == this)
        return SL_RESULT_MEMORY_FAILURE;
    this->mDeviceID = deviceID;
    *pDevice = &this->mObject.mItf;
    return SL_RESULT_SUCCESS;
}

static SLresult Engine_CreateVibraDevice(SLEngineItf self,
    SLObjectItf *pDevice, SLuint32 deviceID, SLuint32 numInterfaces,
    const SLInterfaceID *pInterfaceIds, const SLboolean *pInterfaceRequired)
{
    if (NULL == pDevice || SL_DEFAULTDEVICEID_VIBRA != deviceID)
        return SL_RESULT_PARAMETER_INVALID;
    *pDevice = NULL;
    unsigned exposedMask;
    SLresult result = checkInterfaces(&VibraDevice_class, numInterfaces,
        pInterfaceIds, pInterfaceRequired, &exposedMask);
    if (SL_RESULT_SUCCESS != result)
        return result;
    struct VibraDevice_class *this =
        (struct VibraDevice_class *) construct(&VibraDevice_class, exposedMask);
    if (NULL == this)
        return SL_RESULT_MEMORY_FAILURE;
    this->mDeviceID = deviceID;
    *pDevice = &this->mObject.mItf;
    return SL_RESULT_SUCCESS;
}

#ifdef USE_ANDROID
static void *thread_body(void *arg)
{
    struct AudioPlayer_class *this = (struct AudioPlayer_class *) arg;
    android::AudioTrack *at = this->mAudioTrack;
#if 1
    at->start();
#endif
    struct BufferQueue_interface *bufferQueue = &this->mBufferQueue;
    for (;;) {
        // FIXME replace unsafe polling by a mutex and condition variable
        struct BufferHeader *oldFront = bufferQueue->mFront;
        struct BufferHeader *rear = bufferQueue->mRear;
        if (oldFront == rear) {
            usleep(10000);
            continue;
        }
        struct BufferHeader *newFront = &oldFront[1];
        if (newFront == &bufferQueue->mArray[bufferQueue->mNumBuffers])
            newFront = bufferQueue->mArray;
        at->write(oldFront->mBuffer, oldFront->mSize);
        assert(mState.count > 0);
        --bufferQueue->mState.count;
        ++bufferQueue->mState.playIndex;
        bufferQueue->mFront = newFront;
        slBufferQueueCallback callback = bufferQueue->mCallback;
        if (NULL != callback) {
            (*callback)((SLBufferQueueItf) bufferQueue,
                bufferQueue->mContext);
        }
    }
    // unreachable
    return NULL;
}

void my_handler(int x, void*y, void*z)
{
}
#endif

static SLresult Engine_CreateAudioPlayer(SLEngineItf self, SLObjectItf *pPlayer,
    SLDataSource *pAudioSrc, SLDataSink *pAudioSnk, SLuint32 numInterfaces,
    const SLInterfaceID *pInterfaceIds, const SLboolean *pInterfaceRequired)
{
    if (NULL == pPlayer)
        return SL_RESULT_PARAMETER_INVALID;
    *pPlayer = NULL;
    unsigned exposedMask;
    SLresult result = checkInterfaces(&AudioPlayer_class, numInterfaces,
        pInterfaceIds, pInterfaceRequired, &exposedMask);
    if (SL_RESULT_SUCCESS != result)
        return result;
    // check the audio source and sinks
    // FIXME move this to a separate function: check source, check locator, etc.
    if ((NULL == pAudioSrc) || (NULL == (SLuint32 *) pAudioSrc->pLocator) ||
        (NULL == pAudioSrc->pFormat))
        return SL_RESULT_PARAMETER_INVALID;
    SLuint32 locatorType = *(SLuint32 *)pAudioSrc->pLocator;
    SLuint32 formatType = *(SLuint32 *)pAudioSrc->pFormat;
    SLuint32 numBuffers = 0;
    SLDataFormat_PCM *df_pcm = NULL;
#ifdef USE_OUTPUTMIXEXT
    struct Track *track = NULL;
#endif
#ifdef USE_SNDFILE
    SLchar *pathname = NULL;
#endif // USE_SNDFILE
    switch (locatorType) {
    case SL_DATALOCATOR_BUFFERQUEUE:
        {
        SLDataLocator_BufferQueue *dl_bq =
            (SLDataLocator_BufferQueue *) pAudioSrc->pLocator;
        numBuffers = dl_bq->numBuffers;
        if (0 == numBuffers)
            return SL_RESULT_PARAMETER_INVALID;
        switch (formatType) {
        case SL_DATAFORMAT_PCM:
            {
            df_pcm = (SLDataFormat_PCM *) pAudioSrc->pFormat;
            switch (df_pcm->numChannels) {
            case 1:
            case 2:
                break;
            default:
                return SL_RESULT_CONTENT_UNSUPPORTED;
            }
            switch (df_pcm->samplesPerSec) {
            case 44100:
                break;
#if 1 // wrong units for samplesPerSec!
            case SL_SAMPLINGRATE_44_1:
                break;
#endif
            // others
            default:
                return SL_RESULT_CONTENT_UNSUPPORTED;
            }
            switch (df_pcm->bitsPerSample) {
            case SL_PCMSAMPLEFORMAT_FIXED_16:
                break;
            // others
            default:
                return SL_RESULT_CONTENT_UNSUPPORTED;
            }
            switch (df_pcm->containerSize) {
            case 16:
                break;
            // others
            default:
                return SL_RESULT_CONTENT_UNSUPPORTED;
            }
            switch (df_pcm->channelMask) {
            // needs work
            default:
                break;
            }
            switch (df_pcm->endianness) {
            case SL_BYTEORDER_LITTLEENDIAN:
                break;
            // others esp. big and native (new not in spec)
            default:
                return SL_RESULT_CONTENT_UNSUPPORTED;
            }
            }
            break;
        case SL_DATAFORMAT_MIME:
        case SL_DATAFORMAT_RESERVED3:
            return SL_RESULT_CONTENT_UNSUPPORTED;
        default:
            return SL_RESULT_PARAMETER_INVALID;
        }
        }
        break;
#ifdef USE_SNDFILE
    case SL_DATALOCATOR_URI:
        {
        SLDataLocator_URI *dl_uri = (SLDataLocator_URI *) pAudioSrc->pLocator;
        SLchar *uri = dl_uri->URI;
        if (NULL == uri)
            return SL_RESULT_PARAMETER_INVALID;
        if (strncmp((const char *) uri, "file:///", 8))
            return SL_RESULT_CONTENT_UNSUPPORTED;
        pathname = &uri[8];
        switch (formatType) {
        case SL_DATAFORMAT_MIME:
            {
            SLDataFormat_MIME *df_mime =
                (SLDataFormat_MIME *) pAudioSrc->pFormat;
            SLchar *mimeType = df_mime->mimeType;
            if (NULL == mimeType)
                return SL_RESULT_PARAMETER_INVALID;
            SLuint32 containerType = df_mime->containerType;
            if (!strcmp((const char *) mimeType, "audio/x-wav"))
                ;
            // else if (others)
            //    ;
            else
                return SL_RESULT_CONTENT_UNSUPPORTED;
            switch (containerType) {
            case SL_CONTAINERTYPE_WAV:
                break;
            // others
            default:
                return SL_RESULT_CONTENT_UNSUPPORTED;
            }
            }
            break;
        default:
            return SL_RESULT_CONTENT_UNSUPPORTED;
        }
        // FIXME magic number, should be configurable
        numBuffers = 2;
        }
        break;
#endif // USE_SNDFILE
    case SL_DATALOCATOR_ADDRESS:
    case SL_DATALOCATOR_IODEVICE:
    case SL_DATALOCATOR_OUTPUTMIX:
    case SL_DATALOCATOR_RESERVED5:
    case SL_DATALOCATOR_MIDIBUFFERQUEUE:
    case SL_DATALOCATOR_RESERVED8:
        return SL_RESULT_CONTENT_UNSUPPORTED;
    default:
        return SL_RESULT_PARAMETER_INVALID;
    }
    // check sink, again this should be a separate function
    if (NULL == pAudioSnk || (NULL == (SLuint32 *) pAudioSnk->pLocator))
        return SL_RESULT_PARAMETER_INVALID;
    switch (*(SLuint32 *)pAudioSnk->pLocator) {
    case SL_DATALOCATOR_OUTPUTMIX:
        {
        // pAudioSnk->pFormat is ignored
        SLDataLocator_OutputMix *dl_outmix =
            (SLDataLocator_OutputMix *) pAudioSnk->pLocator;
        SLObjectItf outputMix = dl_outmix->outputMix;
        // FIXME make this an operation on Object: GetClass
        if ((NULL == outputMix) || (&OutputMix_class !=
            ((struct Object_interface *) outputMix)->mClass))
            return SL_RESULT_PARAMETER_INVALID;
#ifdef USE_OUTPUTMIXEXT
        struct OutputMix_interface *om =
            &((struct OutputMix_class *) outputMix)->mOutputMix;
        // allocate an entry within OutputMix for this track
        // FIXME O(n)
        unsigned i;
        for (i = 0, track = &om->mTracks[0]; i < 32; ++i, ++track) {
            if (om->mActiveMask & (1 << i))
                continue;
            om->mActiveMask |= 1 << i;
            break;
        }
        if (32 <= i) {
            // FIXME Need a better error code for all slots full in output mix
            return SL_RESULT_MEMORY_FAILURE;
        }
        // FIXME replace the above for Android - do not use our own mixer!
#endif
        }
        break;
    case SL_DATALOCATOR_BUFFERQUEUE:
    case SL_DATALOCATOR_URI:
    case SL_DATALOCATOR_ADDRESS:
    case SL_DATALOCATOR_IODEVICE:
    case SL_DATALOCATOR_RESERVED5:
    case SL_DATALOCATOR_MIDIBUFFERQUEUE:
    case SL_DATALOCATOR_RESERVED8:
        return SL_RESULT_CONTENT_UNSUPPORTED;
    default:
        return SL_RESULT_PARAMETER_INVALID;
    }
    // Construct our new instance
    struct AudioPlayer_class *this =
        (struct AudioPlayer_class *) construct(&AudioPlayer_class, exposedMask);
    if (NULL == this)
        return SL_RESULT_MEMORY_FAILURE;
    // FIXME numBuffers is unavailable for URL, must make a default !
    assert(0 < numBuffers);
    this->mBufferQueue.mNumBuffers = numBuffers;
    // inline allocation of circular mArray, up to a typical max
    if (BUFFER_HEADER_TYPICAL >= numBuffers) {
        this->mBufferQueue.mArray = this->mBufferQueue.mTypical;
    } else {
        // FIXME integer overflow possible during multiplication
        this->mBufferQueue.mArray = (struct BufferHeader *)
            malloc((numBuffers + 1) * sizeof(struct BufferHeader));
        if (NULL == this->mBufferQueue.mArray) {
            free(this);
            return SL_RESULT_MEMORY_FAILURE;
        }
    }
    this->mBufferQueue.mFront = this->mBufferQueue.mArray;
    this->mBufferQueue.mRear = this->mBufferQueue.mArray;
#ifdef USE_SNDFILE
    this->mSndFile.mPathname = pathname;
    this->mSndFile.mIs0 = SL_BOOLEAN_TRUE;
#ifndef NDEBUG
    this->mSndFile.mSNDFILE = NULL;
    this->mSndFile.mRetryBuffer = NULL;
    this->mSndFile.mRetrySize = 0;
#endif
#endif // USE_SNDFILE
#ifdef USE_OUTPUTMIXEXT
    // link track to player (NOT for Android!!)
    track->mDfPcm = df_pcm;
    track->mBufferQueue = &this->mBufferQueue;
    track->mPlay = &this->mPlay;
    // next 2 fields must be initialized explicitly (not part of this)
    track->mReader = NULL;
    track->mAvail = 0;
#endif
#ifdef USE_ANDROID
    this->mAudioTrack = new android::AudioTrack(
        android::AudioSystem::MUSIC, // streamType
        44100,                       // sampleRate
        android::AudioSystem::PCM_16_BIT,    // format
        // FIXME should be stereo, but mono gives more audio output for testing
        android::AudioSystem::CHANNEL_OUT_MONO,                           // channels
        256 * 20,                         // frameCount
        0,                           // flags
        /*NULL*/ my_handler,                        // cbf (callback)
        (void *) self,               // user
        256 * 20);                        // notificationFrame
    assert(this->mAudioTrack != NULL);
    // FIXME should call checkStatus after new
    int ok;
    // should happen at Realize, not now
    ok = pthread_create(&this->mThread, (const pthread_attr_t *) NULL,
        thread_body, this);
    assert(ok == 0);
#endif
    // return the new audio player object
    *pPlayer = &this->mObject.mItf;
    return SL_RESULT_SUCCESS;
}

static SLresult Engine_CreateAudioRecorder(SLEngineItf self,
    SLObjectItf *pRecorder, SLDataSource *pAudioSrc, SLDataSink *pAudioSnk,
    SLuint32 numInterfaces, const SLInterfaceID *pInterfaceIds,
    const SLboolean *pInterfaceRequired)
{
    if (NULL == pRecorder)
        return SL_RESULT_PARAMETER_INVALID;
    *pRecorder = NULL;
    unsigned exposedMask;
    SLresult result = checkInterfaces(&AudioRecorder_class, numInterfaces,
        pInterfaceIds, pInterfaceRequired, &exposedMask);
    if (SL_RESULT_SUCCESS != result)
        return result;
    return SL_RESULT_SUCCESS;
}

static SLresult Engine_CreateMidiPlayer(SLEngineItf self, SLObjectItf *pPlayer,
    SLDataSource *pMIDISrc, SLDataSource *pBankSrc, SLDataSink *pAudioOutput,
    SLDataSink *pVibra, SLDataSink *pLEDArray, SLuint32 numInterfaces,
    const SLInterfaceID *pInterfaceIds, const SLboolean *pInterfaceRequired)
{
    if (NULL == pPlayer)
        return SL_RESULT_PARAMETER_INVALID;
    *pPlayer = NULL;
    unsigned exposedMask;
    SLresult result = checkInterfaces(&MidiPlayer_class, numInterfaces,
        pInterfaceIds, pInterfaceRequired, &exposedMask);
    if (SL_RESULT_SUCCESS != result)
        return result;
    if (NULL == pMIDISrc || NULL == pAudioOutput)
        return SL_RESULT_PARAMETER_INVALID;
    struct MidiPlayer_class *this =
        (struct MidiPlayer_class *) construct(&MidiPlayer_class, exposedMask);
    if (NULL == this)
        return SL_RESULT_MEMORY_FAILURE;
    // return the new MIDI player object
    *pPlayer = &this->mObject.mItf;
    return SL_RESULT_SUCCESS;
}

static SLresult Engine_CreateListener(SLEngineItf self, SLObjectItf *pListener,
    SLuint32 numInterfaces, const SLInterfaceID *pInterfaceIds,
    const SLboolean *pInterfaceRequired)
{
    if (NULL == pListener)
        return SL_RESULT_PARAMETER_INVALID;
    *pListener = NULL;
    unsigned exposedMask;
    SLresult result = checkInterfaces(&Listener_class, numInterfaces,
        pInterfaceIds, pInterfaceRequired, &exposedMask);
    if (SL_RESULT_SUCCESS != result)
        return result;
    return SL_RESULT_FEATURE_UNSUPPORTED;
}

static SLresult Engine_Create3DGroup(SLEngineItf self, SLObjectItf *pGroup,
    SLuint32 numInterfaces, const SLInterfaceID *pInterfaceIds,
    const SLboolean *pInterfaceRequired)
{
    if (NULL == pGroup)
        return SL_RESULT_PARAMETER_INVALID;
    *pGroup = NULL;
    unsigned exposedMask;
    SLresult result = checkInterfaces(&_3DGroup_class, numInterfaces,
        pInterfaceIds, pInterfaceRequired, &exposedMask);
    if (SL_RESULT_SUCCESS != result)
        return result;
    return SL_RESULT_FEATURE_UNSUPPORTED;
}

static SLresult Engine_CreateOutputMix(SLEngineItf self, SLObjectItf *pMix,
    SLuint32 numInterfaces, const SLInterfaceID *pInterfaceIds,
    const SLboolean *pInterfaceRequired)
{
    if (NULL == pMix)
        return SL_RESULT_PARAMETER_INVALID;
    *pMix = NULL;
    unsigned exposedMask;
    SLresult result = checkInterfaces(&OutputMix_class, numInterfaces,
        pInterfaceIds, pInterfaceRequired, &exposedMask);
    if (SL_RESULT_SUCCESS != result)
        return result;
    struct OutputMix_class *this =
        (struct OutputMix_class *) construct(&OutputMix_class, exposedMask);
    if (NULL == this)
        return SL_RESULT_MEMORY_FAILURE;
    *pMix = &this->mObject.mItf;
    return SL_RESULT_SUCCESS;
}

static SLresult Engine_CreateMetadataExtractor(SLEngineItf self,
    SLObjectItf *pMetadataExtractor, SLDataSource *pDataSource,
    SLuint32 numInterfaces, const SLInterfaceID *pInterfaceIds,
    const SLboolean *pInterfaceRequired)
{
    if (NULL == pMetadataExtractor)
        return SL_RESULT_PARAMETER_INVALID;
    *pMetadataExtractor = NULL;
    unsigned exposedMask;
    SLresult result = checkInterfaces(&MetadataExtractor_class, numInterfaces,
        pInterfaceIds, pInterfaceRequired, &exposedMask);
    if (SL_RESULT_SUCCESS != result)
        return result;
    struct MetadataExtractor_class *this = (struct MetadataExtractor_class *)
        construct(&MetadataExtractor_class, exposedMask);
    if (NULL == this)
        return SL_RESULT_MEMORY_FAILURE;
    *pMetadataExtractor = &this->mObject.mItf;
    return SL_RESULT_SUCCESS;
}

static SLresult Engine_CreateExtensionObject(SLEngineItf self,
    SLObjectItf *pObject, void *pParameters, SLuint32 objectID,
    SLuint32 numInterfaces, const SLInterfaceID *pInterfaceIds,
    const SLboolean *pInterfaceRequired)
{
    if (NULL == pObject)
        return SL_RESULT_PARAMETER_INVALID;
    *pObject = NULL;
    return SL_RESULT_FEATURE_UNSUPPORTED;
}

static SLresult Engine_QueryNumSupportedInterfaces(SLEngineItf self,
    SLuint32 objectID, SLuint32 *pNumSupportedInterfaces)
{
    if (NULL == pNumSupportedInterfaces)
        return SL_RESULT_PARAMETER_INVALID;
    const struct class_ *class__ = objectIDtoClass(objectID);
    if (NULL == class__)
        return SL_RESULT_FEATURE_UNSUPPORTED;
    *pNumSupportedInterfaces = class__->mInterfaceCount;
    return SL_RESULT_SUCCESS;
}

static SLresult Engine_QuerySupportedInterfaces(SLEngineItf self,
    SLuint32 objectID, SLuint32 index, SLInterfaceID *pInterfaceId)
{
    return SL_RESULT_SUCCESS;
}

static SLresult Engine_QueryNumSupportedExtensions(SLEngineItf self,
    SLuint32 *pNumExtensions)
{
    if (NULL == pNumExtensions)
        return SL_RESULT_PARAMETER_INVALID;
    *pNumExtensions = 0;
    return SL_RESULT_SUCCESS;
}

static SLresult Engine_QuerySupportedExtension(SLEngineItf self,
    SLuint32 index, SLchar *pExtensionName, SLint16 *pNameLength)
{
    // any index >= 0 will be >= number of supported extensions
    return SL_RESULT_PARAMETER_INVALID;
}

static SLresult Engine_IsExtensionSupported(SLEngineItf self,
    const SLchar *pExtensionName, SLboolean *pSupported)
{
    if (NULL == pExtensionName || NULL == pSupported)
        return SL_RESULT_PARAMETER_INVALID;
    *pSupported = SL_BOOLEAN_FALSE;
    return SL_RESULT_SUCCESS;
}

/*static*/ const struct SLEngineItf_ Engine_EngineItf = {
    Engine_CreateLEDDevice,
    Engine_CreateVibraDevice,
    Engine_CreateAudioPlayer,
    Engine_CreateAudioRecorder,
    Engine_CreateMidiPlayer,
    Engine_CreateListener,
    Engine_Create3DGroup,
    Engine_CreateOutputMix,
    Engine_CreateMetadataExtractor,
    Engine_CreateExtensionObject,
    Engine_QueryNumSupportedInterfaces,
    Engine_QuerySupportedInterfaces,
    Engine_QueryNumSupportedExtensions,
    Engine_QuerySupportedExtension,
    Engine_IsExtensionSupported
};

/* AudioIODeviceCapabilities implementation */

static SLresult AudioIODeviceCapabilities_GetAvailableAudioInputs(
    SLAudioIODeviceCapabilitiesItf self, SLint32 *pNumInputs,
    SLuint32 *pInputDeviceIDs)
{
    return SL_RESULT_SUCCESS;
}

static SLresult AudioIODeviceCapabilities_QueryAudioInputCapabilities(
    SLAudioIODeviceCapabilitiesItf self, SLuint32 deviceID,
    SLAudioInputDescriptor *pDescriptor)
{
    return SL_RESULT_SUCCESS;
}

static SLresult
    AudioIODeviceCapabilities_RegisterAvailableAudioInputsChangedCallback(
    SLAudioIODeviceCapabilitiesItf self,
    slAvailableAudioInputsChangedCallback callback, void *pContext)
{
    return SL_RESULT_SUCCESS;
}

static SLresult AudioIODeviceCapabilities_GetAvailableAudioOutputs(
    SLAudioIODeviceCapabilitiesItf self, SLint32 *pNumOutputs,
    SLuint32 *pOutputDeviceIDs)
{
    if (NULL == pNumOutputs)
        return SL_RESULT_PARAMETER_INVALID;
    if (NULL != pOutputDeviceIDs) {
        // FIXME should be OEM-configurable
        if (2 > *pNumOutputs)
            return SL_RESULT_BUFFER_INSUFFICIENT;
        pOutputDeviceIDs[0] = DEVICE_ID_HEADSET;
        pOutputDeviceIDs[1] = DEVICE_ID_HANDSFREE;
    }
    *pNumOutputs = 2;
    return SL_RESULT_SUCCESS;
}

static SLresult AudioIODeviceCapabilities_QueryAudioOutputCapabilities(
    SLAudioIODeviceCapabilitiesItf self, SLuint32 deviceID,
    SLAudioOutputDescriptor *pDescriptor)
{
    if (NULL == pDescriptor)
        return SL_RESULT_PARAMETER_INVALID;
    switch (deviceID) {
    // FIXME should be OEM-configurable
    case DEVICE_ID_HEADSET:
        *pDescriptor = AudioOutputDescriptor_headset;
        break;
    case DEVICE_ID_HANDSFREE:
        *pDescriptor = AudioOutputDescriptor_handsfree;
        break;
    default:
        return SL_RESULT_IO_ERROR;
    }
    return SL_RESULT_SUCCESS;
}

static SLresult
    AudioIODeviceCapabilities_RegisterAvailableAudioOutputsChangedCallback(
    SLAudioIODeviceCapabilitiesItf self,
    slAvailableAudioOutputsChangedCallback callback, void *pContext)
{
    return SL_RESULT_SUCCESS;
}

static SLresult
    AudioIODeviceCapabilities_RegisterDefaultDeviceIDMapChangedCallback(
    SLAudioIODeviceCapabilitiesItf self,
    slDefaultDeviceIDMapChangedCallback callback, void *pContext)
{
    return SL_RESULT_SUCCESS;
}

static SLresult AudioIODeviceCapabilities_GetAssociatedAudioInputs(
    SLAudioIODeviceCapabilitiesItf self, SLuint32 deviceID,
    SLint32 *pNumAudioInputs, SLuint32 *pAudioInputDeviceIDs)
{
    return SL_RESULT_SUCCESS;
}

static SLresult AudioIODeviceCapabilities_GetAssociatedAudioOutputs(
    SLAudioIODeviceCapabilitiesItf self, SLuint32 deviceID,
    SLint32 *pNumAudioOutputs, SLuint32 *pAudioOutputDeviceIDs)
{
    return SL_RESULT_SUCCESS;
}

static SLresult AudioIODeviceCapabilities_GetDefaultAudioDevices(
    SLAudioIODeviceCapabilitiesItf self, SLuint32 defaultDeviceID,
    SLint32 *pNumAudioDevices, SLuint32 *pAudioDeviceIDs)
{
    return SL_RESULT_SUCCESS;
}

static SLresult AudioIODeviceCapabilities_QuerySampleFormatsSupported(
    SLAudioIODeviceCapabilitiesItf self, SLuint32 deviceID,
    SLmilliHertz samplingRate, SLint32 *pSampleFormats,
    SLint32 *pNumOfSampleFormats)
{
    return SL_RESULT_SUCCESS;
}

/*static*/ const struct SLAudioIODeviceCapabilitiesItf_
    AudioIODeviceCapabilities_AudioIODeviceCapabilitiesItf = {
    AudioIODeviceCapabilities_GetAvailableAudioInputs,
    AudioIODeviceCapabilities_QueryAudioInputCapabilities,
    AudioIODeviceCapabilities_RegisterAvailableAudioInputsChangedCallback,
    AudioIODeviceCapabilities_GetAvailableAudioOutputs,
    AudioIODeviceCapabilities_QueryAudioOutputCapabilities,
    AudioIODeviceCapabilities_RegisterAvailableAudioOutputsChangedCallback,
    AudioIODeviceCapabilities_RegisterDefaultDeviceIDMapChangedCallback,
    AudioIODeviceCapabilities_GetAssociatedAudioInputs,
    AudioIODeviceCapabilities_GetAssociatedAudioOutputs,
    AudioIODeviceCapabilities_GetDefaultAudioDevices,
    AudioIODeviceCapabilities_QuerySampleFormatsSupported
};

/* OutputMix implementation */

static SLresult OutputMix_GetDestinationOutputDeviceIDs(SLOutputMixItf self,
   SLint32 *pNumDevices, SLuint32 *pDeviceIDs)
{
    return SL_RESULT_SUCCESS;
}

static SLresult OutputMix_RegisterDeviceChangeCallback(SLOutputMixItf self,
    slMixDeviceChangeCallback callback, void *pContext)
{
    return SL_RESULT_SUCCESS;
}

static SLresult OutputMix_ReRoute(SLOutputMixItf self, SLint32 numOutputDevices,
    SLuint32 *pOutputDeviceIDs)
{
    return SL_RESULT_SUCCESS;
}

/*static*/ const struct SLOutputMixItf_ OutputMix_OutputMixItf = {
    OutputMix_GetDestinationOutputDeviceIDs,
    OutputMix_RegisterDeviceChangeCallback,
    OutputMix_ReRoute
};

/* Seek implementation */

static SLresult Seek_SetPosition(SLSeekItf self, SLmillisecond pos,
    SLuint32 seekMode)
{
    switch (seekMode) {
    case SL_SEEKMODE_FAST:
    case SL_SEEKMODE_ACCURATE:
        break;
    default:
        return SL_RESULT_PARAMETER_INVALID;
    }
    struct Seek_interface *this = (struct Seek_interface *) self;
    this->mPos = pos;
    return SL_RESULT_SUCCESS;
}

static SLresult Seek_SetLoop(SLSeekItf self, SLboolean loopEnable,
    SLmillisecond startPos, SLmillisecond endPos)
{
    struct Seek_interface *this = (struct Seek_interface *) self;
    this->mLoopEnabled = loopEnable;
    this->mStartPos = startPos;
    this->mEndPos = endPos;
    return SL_RESULT_SUCCESS;
}

static SLresult Seek_GetLoop(SLSeekItf self, SLboolean *pLoopEnabled,
    SLmillisecond *pStartPos, SLmillisecond *pEndPos)
{
    if (NULL == pLoopEnabled || NULL == pStartPos || NULL == pEndPos)
        return SL_RESULT_PARAMETER_INVALID;
    struct Seek_interface *this = (struct Seek_interface *) self;
    *pLoopEnabled = this->mLoopEnabled;
    *pStartPos = this->mStartPos;
    *pEndPos = this->mEndPos;
    return SL_RESULT_SUCCESS;
}

/*static*/ const struct SLSeekItf_ Seek_SeekItf = {
    Seek_SetPosition,
    Seek_SetLoop,
    Seek_GetLoop
};

/* 3DCommit implementation */

static SLresult _3DCommit_Commit(SL3DCommitItf self)
{
    return SL_RESULT_SUCCESS;
}

static SLresult _3DCommit_SetDeferred(SL3DCommitItf self, SLboolean deferred)
{
    struct _3DCommit_interface *this = (struct _3DCommit_interface *) self;
    this->mDeferred = deferred;
    return SL_RESULT_SUCCESS;
}

/*static*/ const struct SL3DCommitItf_ _3DCommit_3DCommitItf = {
    _3DCommit_Commit,
    _3DCommit_SetDeferred
};

/* 3DDoppler implementation */

static SLresult _3DDoppler_SetVelocityCartesian(SL3DDopplerItf self,
    const SLVec3D *pVelocity)
{
    if (NULL == pVelocity)
        return SL_RESULT_PARAMETER_INVALID;
    struct _3DDoppler_interface *this = (struct _3DDoppler_interface *) self;
    this->mVelocity.mCartesian = *pVelocity;
    return SL_RESULT_SUCCESS;
}

static SLresult _3DDoppler_SetVelocitySpherical(SL3DDopplerItf self,
    SLmillidegree azimuth, SLmillidegree elevation, SLmillimeter speed)
{
    struct _3DDoppler_interface *this = (struct _3DDoppler_interface *) self;
    this->mVelocity.mSpherical.mAzimuth = azimuth;
    this->mVelocity.mSpherical.mElevation = elevation;
    this->mVelocity.mSpherical.mSpeed = speed;
    return SL_RESULT_SUCCESS;
}

static SLresult _3DDoppler_GetVelocityCartesian(SL3DDopplerItf self,
    SLVec3D *pVelocity)
{
    if (NULL == pVelocity)
        return SL_RESULT_PARAMETER_INVALID;
    struct _3DDoppler_interface *this = (struct _3DDoppler_interface *) self;
    *pVelocity = this->mVelocity.mCartesian;
    return SL_RESULT_SUCCESS;
}

static SLresult _3DDoppler_SetDopplerFactor(SL3DDopplerItf self,
    SLpermille dopplerFactor)
{
    struct _3DDoppler_interface *this = (struct _3DDoppler_interface *) self;
    this->mDopplerFactor = dopplerFactor;
    return SL_RESULT_SUCCESS;
}

static SLresult _3DDoppler_GetDopplerFactor(SL3DDopplerItf self,
    SLpermille *pDopplerFactor)
{
    if (NULL == pDopplerFactor)
        return SL_RESULT_PARAMETER_INVALID;
    struct _3DDoppler_interface *this = (struct _3DDoppler_interface *) self;
    *pDopplerFactor = this->mDopplerFactor;
    return SL_RESULT_SUCCESS;
}

/*static*/ const struct SL3DDopplerItf_ _3DDoppler_3DDopplerItf = {
    _3DDoppler_SetVelocityCartesian,
    _3DDoppler_SetVelocitySpherical,
    _3DDoppler_GetVelocityCartesian,
    _3DDoppler_SetDopplerFactor,
    _3DDoppler_GetDopplerFactor
};

/* 3DLocation implementation */

static SLresult _3DLocation_SetLocationCartesian(SL3DLocationItf self,
    const SLVec3D *pLocation)
{
    if (NULL == pLocation)
        return SL_RESULT_PARAMETER_INVALID;
    struct _3DLocation_interface *this = (struct _3DLocation_interface *) self;
    this->mLocation.mCartesian = *pLocation;
    return SL_RESULT_SUCCESS;
}

static SLresult _3DLocation_SetLocationSpherical(SL3DLocationItf self,
    SLmillidegree azimuth, SLmillidegree elevation, SLmillimeter distance)
{
    struct _3DLocation_interface *this = (struct _3DLocation_interface *) self;
    this->mLocation.mSpherical.mAzimuth = azimuth;
    this->mLocation.mSpherical.mElevation = elevation;
    this->mLocation.mSpherical.mDistance = distance;
    return SL_RESULT_SUCCESS;
}

static SLresult _3DLocation_Move(SL3DLocationItf self, const SLVec3D *pMovement)
{
    if (NULL == pMovement)
        return SL_RESULT_PARAMETER_INVALID;
    struct _3DLocation_interface *this = (struct _3DLocation_interface *) self;
    this->mLocation.mCartesian.x += pMovement->x;
    this->mLocation.mCartesian.y += pMovement->y;
    this->mLocation.mCartesian.z += pMovement->z;
    return SL_RESULT_SUCCESS;
}

static SLresult _3DLocation_GetLocationCartesian(SL3DLocationItf self,
    SLVec3D *pLocation)
{
    if (NULL == pLocation)
        return SL_RESULT_PARAMETER_INVALID;
    struct _3DLocation_interface *this = (struct _3DLocation_interface *) self;
    *pLocation = this->mLocation.mCartesian;
    return SL_RESULT_SUCCESS;
}

static SLresult _3DLocation_SetOrientationVectors(SL3DLocationItf self,
    const SLVec3D *pFront, const SLVec3D *pAbove)
{
    if (NULL == pFront || NULL == pAbove)
        return SL_RESULT_PARAMETER_INVALID;
    struct _3DLocation_interface *this = (struct _3DLocation_interface *) self;
    this->mFront = *pFront;
    this->mAbove = *pAbove;
    return SL_RESULT_SUCCESS;
}

static SLresult _3DLocation_SetOrientationAngles(SL3DLocationItf self,
    SLmillidegree heading, SLmillidegree pitch, SLmillidegree roll)
{
    struct _3DLocation_interface *this = (struct _3DLocation_interface *) self;
    this->mHeading = heading;
    this->mPitch = pitch;
    this->mRoll = roll;
    return SL_RESULT_SUCCESS;
}

static SLresult _3DLocation_Rotate(SL3DLocationItf self, SLmillidegree theta,
    const SLVec3D *pAxis)
{
    if (NULL == pAxis)
        return SL_RESULT_PARAMETER_INVALID;
    return SL_RESULT_SUCCESS;
}

static SLresult _3DLocation_GetOrientationVectors(SL3DLocationItf self,
    SLVec3D *pFront, SLVec3D *pUp)
{
    if (NULL == pFront || NULL == pUp)
        return SL_RESULT_PARAMETER_INVALID;
    struct _3DLocation_interface *this = (struct _3DLocation_interface *) self;
    *pFront = this->mFront;
    *pUp = this->mUp;
    return SL_RESULT_SUCCESS;
}

/*static*/ const struct SL3DLocationItf_ _3DLocation_3DLocationItf = {
    _3DLocation_SetLocationCartesian,
    _3DLocation_SetLocationSpherical,
    _3DLocation_Move,
    _3DLocation_GetLocationCartesian,
    _3DLocation_SetOrientationVectors,
    _3DLocation_SetOrientationAngles,
    _3DLocation_Rotate,
    _3DLocation_GetOrientationVectors
};

/* AudioDecoderCapabilities implementation */

// FIXME should build this table from Caps table below
static const SLuint32 Our_Decoder_IDs[] = {
    SL_AUDIOCODEC_PCM,
    SL_AUDIOCODEC_MP3,
    SL_AUDIOCODEC_AMR,
    SL_AUDIOCODEC_AMRWB,
    SL_AUDIOCODEC_AMRWBPLUS,
    SL_AUDIOCODEC_AAC,
    SL_AUDIOCODEC_WMA,
    SL_AUDIOCODEC_REAL,
// FIXME not in 1.0.1 header file
#define SL_AUDIOCODEC_VORBIS   ((SLuint32) 0x00000009)
    SL_AUDIOCODEC_VORBIS
};
#define MAX_DECODERS (sizeof(Our_Decoder_IDs) / sizeof(Our_Decoder_IDs[0]))

static SLresult AudioDecoderCapabilities_GetAudioDecoders(
    SLAudioDecoderCapabilitiesItf self, SLuint32 *pNumDecoders,
    SLuint32 *pDecoderIds)
{
    if (NULL == pNumDecoders)
        return SL_RESULT_PARAMETER_INVALID;
    if (NULL == pDecoderIds)
        *pNumDecoders = MAX_DECODERS;
    else {
        SLuint32 numDecoders = *pNumDecoders;
        if (MAX_DECODERS < numDecoders) {
            numDecoders = MAX_DECODERS;
            *pNumDecoders = MAX_DECODERS;
        }
        memcpy(pDecoderIds, Our_Decoder_IDs, numDecoders * sizeof(SLuint32));
    }
    return SL_RESULT_SUCCESS;
}

static const SLmilliHertz Sample_Rates_PCM[] = {
    SL_SAMPLINGRATE_8,
    SL_SAMPLINGRATE_11_025,
    SL_SAMPLINGRATE_12,
    SL_SAMPLINGRATE_16,
    SL_SAMPLINGRATE_22_05,
    SL_SAMPLINGRATE_24,
    SL_SAMPLINGRATE_32,
    SL_SAMPLINGRATE_44_1,
    SL_SAMPLINGRATE_48
};

static const SLAudioCodecDescriptor Caps_PCM[] = {
    {
    2,                   // maxChannels
    8,                   // minBitsPerSample
    16,                  // maxBitsPerSample
    SL_SAMPLINGRATE_8,   // minSampleRate
    SL_SAMPLINGRATE_48,  // maxSampleRate
    SL_BOOLEAN_FALSE,    // isFreqRangeContinuous
    (SLmilliHertz *) Sample_Rates_PCM,
                         // pSampleRatesSupported;
    sizeof(Sample_Rates_PCM)/sizeof(Sample_Rates_PCM[0]),
                         // numSampleRatesSupported
    1,                   // minBitRate
    ~0,                  // maxBitRate
    SL_BOOLEAN_TRUE,     // isBitrateRangeContinuous
    NULL,                // pBitratesSupported
    0,                   // numBitratesSupported
    SL_AUDIOPROFILE_PCM, // profileSetting
    0                    // modeSetting
    }
};

static const struct AudioDecoderCapabilities {
    SLuint32 mDecoderID;
    SLuint32 mNumCapabilities;
    const SLAudioCodecDescriptor *mDescriptors;
} Our_Decoder_Capabilities[] = {
#define ENTRY(x) \
    {SL_AUDIOCODEC_##x, sizeof(Caps_##x)/sizeof(Caps_##x[0]), Caps_##x}
    ENTRY(PCM)
#if 0
    ENTRY(MP3),
    ENTRY(AMR),
    ENTRY(AMRWB),
    ENTRY(AMRWBPLUS),
    ENTRY(AAC),
    ENTRY(WMA),
    ENTRY(REAL),
    ENTRY(VORBIS)
#endif
};

static SLresult AudioDecoderCapabilities_GetAudioDecoderCapabilities(
    SLAudioDecoderCapabilitiesItf self, SLuint32 decoderId, SLuint32 *pIndex,
    SLAudioCodecDescriptor *pDescriptor)
{
    return SL_RESULT_SUCCESS;
}

/*static*/ const struct SLAudioDecoderCapabilitiesItf_
    AudioDecoderCapabilities_AudioDecoderCapabilitiesItf = {
    AudioDecoderCapabilities_GetAudioDecoders,
    AudioDecoderCapabilities_GetAudioDecoderCapabilities
};

/* AudioEncoder implementation */

static SLresult AudioEncoder_SetEncoderSettings(SLAudioEncoderItf self,
    SLAudioEncoderSettings  *pSettings)
{
    if (NULL == pSettings)
        return SL_RESULT_PARAMETER_INVALID;
    struct AudioEncoder_interface *this =
        (struct AudioEncoder_interface *) self;
    this->mSettings = *pSettings;
    return SL_RESULT_SUCCESS;
}

static SLresult AudioEncoder_GetEncoderSettings(SLAudioEncoderItf self,
    SLAudioEncoderSettings *pSettings)
{
    if (NULL == pSettings)
        return SL_RESULT_PARAMETER_INVALID;
    struct AudioEncoder_interface *this =
        (struct AudioEncoder_interface *) self;
    *pSettings = this->mSettings;
    return SL_RESULT_SUCCESS;
}

/*static*/ const struct SLAudioEncoderItf_ AudioEncoder_AudioEncoderItf = {
    AudioEncoder_SetEncoderSettings,
    AudioEncoder_GetEncoderSettings
};

/* AudioEncoderCapabilities implementation */

static SLresult AudioEncoderCapabilities_GetAudioEncoders(
    SLAudioEncoderCapabilitiesItf self, SLuint32 *pNumEncoders,
    SLuint32 *pEncoderIds)
{
    return SL_RESULT_SUCCESS;
}

static SLresult AudioEncoderCapabilities_GetAudioEncoderCapabilities(
    SLAudioEncoderCapabilitiesItf self, SLuint32 encoderId, SLuint32 *pIndex,
    SLAudioCodecDescriptor *pDescriptor)
{
    return SL_RESULT_SUCCESS;
}

/*static*/ const struct SLAudioEncoderCapabilitiesItf_
    AudioEncoderCapabilities_AudioEncoderCapabilitiesItf = {
    AudioEncoderCapabilities_GetAudioEncoders,
    AudioEncoderCapabilities_GetAudioEncoderCapabilities
};

/* DeviceVolume implementation */

static SLresult DeviceVolume_GetVolumeScale(SLDeviceVolumeItf self,
    SLuint32 deviceID, SLint32 *pMinValue, SLint32 *pMaxValue,
    SLboolean *pIsMillibelScale)
{
    if (NULL != pMinValue)
        *pMinValue = 0;
    if (NULL != pMaxValue)
        *pMaxValue = 10;
    if (NULL != pIsMillibelScale)
        *pIsMillibelScale = SL_BOOLEAN_FALSE;
    return SL_RESULT_SUCCESS;
}

static SLresult DeviceVolume_SetVolume(SLDeviceVolumeItf self,
    SLuint32 deviceID, SLint32 volume)
{
    struct DeviceVolume_interface *this =
        (struct DeviceVolume_interface *) self;
    this->mVolume = volume;
    return SL_RESULT_SUCCESS;
}

static SLresult DeviceVolume_GetVolume(SLDeviceVolumeItf self,
    SLuint32 deviceID, SLint32 *pVolume)
{
    if (NULL == pVolume)
        return SL_RESULT_PARAMETER_INVALID;
    struct DeviceVolume_interface *this =
        (struct DeviceVolume_interface *) self;
    *pVolume = this->mVolume;
    return SL_RESULT_SUCCESS;
}

/*static*/ const struct SLDeviceVolumeItf_ DeviceVolume_DeviceVolumeItf = {
    DeviceVolume_GetVolumeScale,
    DeviceVolume_SetVolume,
    DeviceVolume_GetVolume
};

/* DynamicSource implementation */

static SLresult DynamicSource_SetSource(SLDynamicSourceItf self,
    SLDataSource *pDataSource)
{
    return SL_RESULT_SUCCESS;
}

/*static*/ const struct SLDynamicSourceItf_ DynamicSource_DynamicSourceItf = {
    DynamicSource_SetSource
};

/* EffectSend implementation */

static SLresult EffectSend_EnableEffectSend(SLEffectSendItf self,
    const void *pAuxEffect, SLboolean enable, SLmillibel initialLevel)
{
    return SL_RESULT_SUCCESS;
}

static SLresult EffectSend_IsEnabled(SLEffectSendItf self,
    const void *pAuxEffect, SLboolean *pEnable)
{
    return SL_RESULT_SUCCESS;
}

static SLresult EffectSend_SetDirectLevel(SLEffectSendItf self,
    SLmillibel directLevel)
{
    return SL_RESULT_SUCCESS;
}

static SLresult EffectSend_GetDirectLevel(SLEffectSendItf self,
    SLmillibel *pDirectLevel)
{
    return SL_RESULT_SUCCESS;
}

static SLresult EffectSend_SetSendLevel(SLEffectSendItf self,
    const void *pAuxEffect, SLmillibel sendLevel)
{
    return SL_RESULT_SUCCESS;
}

static SLresult EffectSend_GetSendLevel(SLEffectSendItf self,
    const void *pAuxEffect, SLmillibel *pSendLevel)
{
    return SL_RESULT_SUCCESS;
}

/*static*/ const struct SLEffectSendItf_ EffectSend_EffectSendItf = {
    EffectSend_EnableEffectSend,
    EffectSend_IsEnabled,
    EffectSend_SetDirectLevel,
    EffectSend_GetDirectLevel,
    EffectSend_SetSendLevel,
    EffectSend_GetSendLevel
};

/* EngineCapabilities implementation */

static SLresult EngineCapabilities_QuerySupportedProfiles(
    SLEngineCapabilitiesItf self, SLuint16 *pProfilesSupported)
{
    if (NULL == pProfilesSupported)
        return SL_RESULT_PARAMETER_INVALID;
    // FIXME This is pessimistic as it omits the unofficial driver profile
    // SL_PROFILES_PHONE | SL_PROFILES_MUSIC | SL_PROFILES_GAME
    *pProfilesSupported = 0;
    return SL_RESULT_SUCCESS;
}

static SLresult EngineCapabilities_QueryAvailableVoices(
    SLEngineCapabilitiesItf self, SLuint16 voiceType, SLint16 *pNumMaxVoices,
    SLboolean *pIsAbsoluteMax, SLint16 *pNumFreeVoices)
{
    switch (voiceType) {
    case SL_VOICETYPE_2D_AUDIO:
    case SL_VOICETYPE_MIDI:
    case SL_VOICETYPE_3D_AUDIO:
    case SL_VOICETYPE_3D_MIDIOUTPUT:
        break;
    default:
        return SL_RESULT_PARAMETER_INVALID;
    }
    if (NULL != pNumMaxVoices)
        *pNumMaxVoices = 0;
    if (NULL != pIsAbsoluteMax)
        *pIsAbsoluteMax = SL_BOOLEAN_TRUE;
    if (NULL != pNumFreeVoices)
        *pNumFreeVoices = 0;
    return SL_RESULT_SUCCESS;
}

static SLresult EngineCapabilities_QueryNumberOfMIDISynthesizers(
    SLEngineCapabilitiesItf self, SLint16 *pNum)
{
    if (NULL == pNum)
        return SL_RESULT_PARAMETER_INVALID;
    *pNum = 0;
    return SL_RESULT_SUCCESS;
}

static SLresult EngineCapabilities_QueryAPIVersion(SLEngineCapabilitiesItf self,
    SLint16 *pMajor, SLint16 *pMinor, SLint16 *pStep)
{
    if (!(NULL != pMajor && NULL != pMinor && NULL != pStep))
        return SL_RESULT_PARAMETER_INVALID;
    *pMajor = 1;
    *pMinor = 0;
    *pStep = 1;
    return SL_RESULT_SUCCESS;
}

static SLresult EngineCapabilities_QueryLEDCapabilities(
    SLEngineCapabilitiesItf self, SLuint32 *pIndex, SLuint32 *pLEDDeviceID,
    SLLEDDescriptor *pDescriptor)
{
    return SL_RESULT_SUCCESS;
}

static SLresult EngineCapabilities_QueryVibraCapabilities(
    SLEngineCapabilitiesItf self, SLuint32 *pIndex, SLuint32 *pVibraDeviceID,
    SLVibraDescriptor *pDescriptor)
{
    return SL_RESULT_SUCCESS;
}

static SLresult EngineCapabilities_IsThreadSafe(SLEngineCapabilitiesItf self,
    SLboolean *pIsThreadSafe)
{
    if (NULL == pIsThreadSafe)
        return SL_RESULT_PARAMETER_INVALID;
    // FIXME For now
    *pIsThreadSafe = SL_BOOLEAN_FALSE;
    return SL_RESULT_SUCCESS;
}

/*static*/ const struct SLEngineCapabilitiesItf_
    EngineCapabilities_EngineCapabilitiesItf = {
    EngineCapabilities_QuerySupportedProfiles,
    EngineCapabilities_QueryAvailableVoices,
    EngineCapabilities_QueryNumberOfMIDISynthesizers,
    EngineCapabilities_QueryAPIVersion,
    EngineCapabilities_QueryLEDCapabilities,
    EngineCapabilities_QueryVibraCapabilities,
    EngineCapabilities_IsThreadSafe
};

/* LEDArray implementation */

static SLresult LEDArray_ActivateLEDArray(SLLEDArrayItf self,
    SLuint32 lightMask)
{
    return SL_RESULT_SUCCESS;
}

static SLresult LEDArray_IsLEDArrayActivated(SLLEDArrayItf self,
    SLuint32 *lightMask)
{
    return SL_RESULT_SUCCESS;
}

static SLresult LEDArray_SetColor(SLLEDArrayItf self, SLuint8 index,
    const SLHSL *color)
{
    return SL_RESULT_SUCCESS;
}

static SLresult LEDArray_GetColor(SLLEDArrayItf self, SLuint8 index,
    SLHSL *color)
{
    return SL_RESULT_SUCCESS;
}

/*static*/ const struct SLLEDArrayItf_ LEDArray_LEDArrayItf = {
    LEDArray_ActivateLEDArray,
    LEDArray_IsLEDArrayActivated,
    LEDArray_SetColor,
    LEDArray_GetColor,
};

/* MetadataExtraction implementation */

static SLresult MetadataExtraction_GetItemCount(SLMetadataExtractionItf self,
    SLuint32 *pItemCount)
{
    return SL_RESULT_SUCCESS;
}

static SLresult MetadataExtraction_GetKeySize(SLMetadataExtractionItf self,
    SLuint32 index, SLuint32 *pKeySize)
{
    return SL_RESULT_SUCCESS;
}

static SLresult MetadataExtraction_GetKey(SLMetadataExtractionItf self,
    SLuint32 index, SLuint32 keySize, SLMetadataInfo *pKey)
{
    return SL_RESULT_SUCCESS;
}

static SLresult MetadataExtraction_GetValueSize(SLMetadataExtractionItf self,
    SLuint32 index, SLuint32 *pValueSize)
{
    return SL_RESULT_SUCCESS;
}

static SLresult MetadataExtraction_GetValue(SLMetadataExtractionItf self,
    SLuint32 index, SLuint32 valueSize, SLMetadataInfo *pValue)
{
    return SL_RESULT_SUCCESS;
}

static SLresult MetadataExtraction_AddKeyFilter(SLMetadataExtractionItf self,
    SLuint32 keySize, const void *pKey, SLuint32 keyEncoding,
    const SLchar *pValueLangCountry, SLuint32 valueEncoding, SLuint8 filterMask)
{
    return SL_RESULT_SUCCESS;
}

static SLresult MetadataExtraction_ClearKeyFilter(SLMetadataExtractionItf self)
{
    return SL_RESULT_SUCCESS;
}

/*static*/ const struct SLMetadataExtractionItf_
    MetadataExtraction_MetadataExtractionItf = {
    MetadataExtraction_GetItemCount,
    MetadataExtraction_GetKeySize,
    MetadataExtraction_GetKey,
    MetadataExtraction_GetValueSize,
    MetadataExtraction_GetValue,
    MetadataExtraction_AddKeyFilter,
    MetadataExtraction_ClearKeyFilter
};

/* MetadataTraversal implementation */

static SLresult MetadataTraversal_SetMode(SLMetadataTraversalItf self,
    SLuint32 mode)
{
    return SL_RESULT_SUCCESS;
}

static SLresult MetadataTraversal_GetChildCount(SLMetadataTraversalItf self,
    SLuint32 *pCount)
{
    return SL_RESULT_SUCCESS;
}

static SLresult MetadataTraversal_GetChildMIMETypeSize(
    SLMetadataTraversalItf self, SLuint32 index, SLuint32 *pSize)
{
    return SL_RESULT_SUCCESS;
}

static SLresult MetadataTraversal_GetChildInfo(SLMetadataTraversalItf self,
    SLuint32 index, SLint32 *pNodeID, SLuint32 *pType, SLuint32 size,
    SLchar *pMimeType)
{
    return SL_RESULT_SUCCESS;
}

static SLresult MetadataTraversal_SetActiveNode(SLMetadataTraversalItf self,
    SLuint32 index)
{
    return SL_RESULT_SUCCESS;
}

/*static*/ const struct SLMetadataTraversalItf_
    MetadataTraversal_MetadataTraversalItf = {
    MetadataTraversal_SetMode,
    MetadataTraversal_GetChildCount,
    MetadataTraversal_GetChildMIMETypeSize,
    MetadataTraversal_GetChildInfo,
    MetadataTraversal_SetActiveNode
};

/* MuteSolo implementation */

static SLresult MuteSolo_SetChannelMute(SLMuteSoloItf self, SLuint8 chan,
   SLboolean mute)
{
    return SL_RESULT_SUCCESS;
}

static SLresult MuteSolo_GetChannelMute(SLMuteSoloItf self, SLuint8 chan,
    SLboolean *pMute)
{
    return SL_RESULT_SUCCESS;
}

static SLresult MuteSolo_SetChannelSolo(SLMuteSoloItf self, SLuint8 chan,
    SLboolean solo)
{
    return SL_RESULT_SUCCESS;
}

static SLresult MuteSolo_GetChannelSolo(SLMuteSoloItf self, SLuint8 chan,
    SLboolean *pSolo)
{
    return SL_RESULT_SUCCESS;
}

static SLresult MuteSolo_GetNumChannels(SLMuteSoloItf self,
    SLuint8 *pNumChannels)
{
    return SL_RESULT_SUCCESS;
}

/*static*/ const struct SLMuteSoloItf_ MuteSolo_MuteSoloItf = {
    MuteSolo_SetChannelMute,
    MuteSolo_GetChannelMute,
    MuteSolo_SetChannelSolo,
    MuteSolo_GetChannelSolo,
    MuteSolo_GetNumChannels
};

/* Pitch implementation */

static SLresult Pitch_SetPitch(SLPitchItf self, SLpermille pitch)
{
    return SL_RESULT_SUCCESS;
}

static SLresult Pitch_GetPitch(SLPitchItf self, SLpermille *pPitch)
{
    return SL_RESULT_SUCCESS;
}

static SLresult Pitch_GetPitchCapabilities(SLPitchItf self,
    SLpermille *pMinPitch, SLpermille *pMaxPitch)
{
    return SL_RESULT_SUCCESS;
}

/*static*/ const struct SLPitchItf_ Pitch_PitchItf = {
    Pitch_SetPitch,
    Pitch_GetPitch,
    Pitch_GetPitchCapabilities
};

/* PlaybackRate implementation */

static SLresult PlaybackRate_SetRate(SLPlaybackRateItf self, SLpermille rate)
{
    return SL_RESULT_SUCCESS;
}

static SLresult PlaybackRate_GetRate(SLPlaybackRateItf self, SLpermille *pRate)
{
    return SL_RESULT_SUCCESS;
}

static SLresult PlaybackRate_SetPropertyConstraints(SLPlaybackRateItf self,
    SLuint32 constraints)
{
    return SL_RESULT_SUCCESS;
}

static SLresult PlaybackRate_GetProperties(SLPlaybackRateItf self,
    SLuint32 *pProperties)
{
    return SL_RESULT_SUCCESS;
}

static SLresult PlaybackRate_GetCapabilitiesOfRate(SLPlaybackRateItf self,
    SLpermille rate, SLuint32 *pCapabilities)
{
    return SL_RESULT_SUCCESS;
}

static SLresult PlaybackRate_GetRateRange(SLPlaybackRateItf self, SLuint8 index,
    SLpermille *pMinRate, SLpermille *pMaxRate, SLpermille *pStepSize,
    SLuint32 *pCapabilities)
{
    return SL_RESULT_SUCCESS;
}

/*static*/ const struct SLPlaybackRateItf_ PlaybackRate_PlaybackRateItf = {
    PlaybackRate_SetRate,
    PlaybackRate_GetRate,
    PlaybackRate_SetPropertyConstraints,
    PlaybackRate_GetProperties,
    PlaybackRate_GetCapabilitiesOfRate,
    PlaybackRate_GetRateRange
};

/* PrefetchStatus implementation */

static SLresult PrefetchStatus_GetPrefetchStatus(SLPrefetchStatusItf self,
    SLuint32 *pStatus)
{
    return SL_RESULT_SUCCESS;
}

static SLresult PrefetchStatus_GetFillLevel(SLPrefetchStatusItf self,
    SLpermille *pLevel)
{
    return SL_RESULT_SUCCESS;
}

static SLresult PrefetchStatus_RegisterCallback(SLPrefetchStatusItf self,
    slPrefetchCallback callback, void *pContext)
{
    return SL_RESULT_SUCCESS;
}

static SLresult PrefetchStatus_SetCallbackEventsMask(SLPrefetchStatusItf self,
    SLuint32 eventFlags)
{
    return SL_RESULT_SUCCESS;
}

static SLresult PrefetchStatus_GetCallbackEventsMask(SLPrefetchStatusItf self,
    SLuint32 *pEventFlags)
{
    return SL_RESULT_SUCCESS;
}

static SLresult PrefetchStatus_SetFillUpdatePeriod(SLPrefetchStatusItf self,
    SLpermille period)
{
    return SL_RESULT_SUCCESS;
}

static SLresult PrefetchStatus_GetFillUpdatePeriod(SLPrefetchStatusItf self,
    SLpermille *pPeriod)
{
    return SL_RESULT_SUCCESS;
}

/*static*/ const struct SLPrefetchStatusItf_
PrefetchStatus_PrefetchStatusItf = {
    PrefetchStatus_GetPrefetchStatus,
    PrefetchStatus_GetFillLevel,
    PrefetchStatus_RegisterCallback,
    PrefetchStatus_SetCallbackEventsMask,
    PrefetchStatus_GetCallbackEventsMask,
    PrefetchStatus_SetFillUpdatePeriod,
    PrefetchStatus_GetFillUpdatePeriod
};

/* RatePitch implementation */

static SLresult RatePitch_SetRate(SLRatePitchItf self, SLpermille rate)
{
    return SL_RESULT_SUCCESS;
}

static SLresult RatePitch_GetRate(SLRatePitchItf self, SLpermille *pRate)
{
    return SL_RESULT_SUCCESS;
}

static SLresult RatePitch_GetRatePitchCapabilities(SLRatePitchItf self,
    SLpermille *pMinRate, SLpermille *pMaxRate)
{
    return SL_RESULT_SUCCESS;
}

/*static*/ const struct SLRatePitchItf_ RatePitch_RatePitchItf = {
    RatePitch_SetRate,
    RatePitch_GetRate,
    RatePitch_GetRatePitchCapabilities
};

/* Record implementation */

static SLresult Record_SetRecordState(SLRecordItf self, SLuint32 state)
{
    return SL_RESULT_SUCCESS;
}

static SLresult Record_GetRecordState(SLRecordItf self, SLuint32 *pState)
{
    return SL_RESULT_SUCCESS;
}

static SLresult Record_SetDurationLimit(SLRecordItf self, SLmillisecond msec)
{
    return SL_RESULT_SUCCESS;
}

static SLresult Record_GetPosition(SLRecordItf self, SLmillisecond *pMsec)
{
    return SL_RESULT_SUCCESS;
}

static SLresult Record_RegisterCallback(SLRecordItf self,
    slRecordCallback callback, void *pContext)
{
    return SL_RESULT_SUCCESS;
}

static SLresult Record_SetCallbackEventsMask(SLRecordItf self,
    SLuint32 eventFlags)
{
    return SL_RESULT_SUCCESS;
}

static SLresult Record_GetCallbackEventsMask(SLRecordItf self,
    SLuint32 *pEventFlags)
{
    return SL_RESULT_SUCCESS;
}

static SLresult Record_SetMarkerPosition(SLRecordItf self, SLmillisecond mSec)
{
    return SL_RESULT_SUCCESS;
}

static SLresult Record_ClearMarkerPosition(SLRecordItf self)
{
    return SL_RESULT_SUCCESS;
}

static SLresult Record_GetMarkerPosition(SLRecordItf self, SLmillisecond *pMsec)
{
    return SL_RESULT_SUCCESS;
}

static SLresult Record_SetPositionUpdatePeriod(SLRecordItf self,
    SLmillisecond mSec)
{
    return SL_RESULT_SUCCESS;
}

static SLresult Record_GetPositionUpdatePeriod(SLRecordItf self,
    SLmillisecond *pMsec)
{
    return SL_RESULT_SUCCESS;
}

/*static*/ const struct SLRecordItf_ Record_RecordItf = {
    Record_SetRecordState,
    Record_GetRecordState,
    Record_SetDurationLimit,
    Record_GetPosition,
    Record_RegisterCallback,
    Record_SetCallbackEventsMask,
    Record_GetCallbackEventsMask,
    Record_SetMarkerPosition,
    Record_ClearMarkerPosition,
    Record_GetMarkerPosition,
    Record_SetPositionUpdatePeriod,
    Record_GetPositionUpdatePeriod
};

/* ThreadSync implementation */

static SLresult ThreadSync_EnterCriticalSection(SLThreadSyncItf self)
{
    return SL_RESULT_SUCCESS;
}

static SLresult ThreadSync_ExitCriticalSection(SLThreadSyncItf self)
{
    return SL_RESULT_SUCCESS;
}

/*static*/ const struct SLThreadSyncItf_ ThreadSync_ThreadSyncItf = {
    ThreadSync_EnterCriticalSection,
    ThreadSync_ExitCriticalSection
};

/* Vibra implementation */

static SLresult Vibra_Vibrate(SLVibraItf self, SLboolean vibrate)
{
    return SL_RESULT_SUCCESS;
}

static SLresult Vibra_IsVibrating(SLVibraItf self, SLboolean *pVibrating)
{
    return SL_RESULT_SUCCESS;
}

static SLresult Vibra_SetFrequency(SLVibraItf self, SLmilliHertz frequency)
{
    return SL_RESULT_SUCCESS;
}

static SLresult Vibra_GetFrequency(SLVibraItf self, SLmilliHertz *pFrequency)
{
    return SL_RESULT_SUCCESS;
}

static SLresult Vibra_SetIntensity(SLVibraItf self, SLpermille intensity)
{
    return SL_RESULT_SUCCESS;
}

static SLresult Vibra_GetIntensity(SLVibraItf self, SLpermille *pIntensity)
{
    return SL_RESULT_SUCCESS;
}

/*static*/ const struct SLVibraItf_ Vibra_VibraItf = {
    Vibra_Vibrate,
    Vibra_IsVibrating,
    Vibra_SetFrequency,
    Vibra_GetFrequency,
    Vibra_SetIntensity,
    Vibra_GetIntensity
};

/* Visualization implementation */

static SLresult Visualization_RegisterVisualizationCallback(
    SLVisualizationItf self, slVisualizationCallback callback, void *pContext,
    SLmilliHertz rate)
{
    struct Visualization_interface *this =
        (struct Visualization_interface *) self;
    this->mCallback = callback;
    this->mContext = pContext;
    this->mRate = rate;
    return SL_RESULT_SUCCESS;
}

static SLresult Visualization_GetMaxRate(SLVisualizationItf self,
    SLmilliHertz *pRate)
{
    if (NULL == pRate)
        return SL_RESULT_PARAMETER_INVALID;
    *pRate = 0;
    return SL_RESULT_SUCCESS;
}

/*static*/ const struct SLVisualizationItf_ Visualization_VisualizationItf = {
    Visualization_RegisterVisualizationCallback,
    Visualization_GetMaxRate
};

/* BassBoost implementation */

static SLresult BassBoost_SetEnabled(SLBassBoostItf self, SLboolean enabled)
{
    struct BassBoost_interface *this = (struct BassBoost_interface *) self;
    this->mEnabled = enabled;
    return SL_RESULT_SUCCESS;
}

static SLresult BassBoost_IsEnabled(SLBassBoostItf self, SLboolean *pEnabled)
{
    if (NULL == pEnabled)
        return SL_RESULT_PARAMETER_INVALID;
    struct BassBoost_interface *this = (struct BassBoost_interface *) self;
    *pEnabled = this->mEnabled;
    return SL_RESULT_SUCCESS;
}

static SLresult BassBoost_SetStrength(SLBassBoostItf self, SLpermille strength)
{
    struct BassBoost_interface *this = (struct BassBoost_interface *) self;
    this->mStrength = strength;
    return SL_RESULT_SUCCESS;
}

static SLresult BassBoost_GetRoundedStrength(SLBassBoostItf self,
    SLpermille *pStrength)
{
    if (NULL == pStrength)
        return SL_RESULT_PARAMETER_INVALID;
    struct BassBoost_interface *this = (struct BassBoost_interface *) self;
    *pStrength = this->mStrength;
    return SL_RESULT_SUCCESS;
}

static SLresult BassBoost_IsStrengthSupported(SLBassBoostItf self,
    SLboolean *pSupported)
{
    if (NULL == pSupported)
        return SL_RESULT_PARAMETER_INVALID;
    *pSupported = SL_BOOLEAN_TRUE;
    return SL_RESULT_SUCCESS;
}

/*static*/ const struct SLBassBoostItf_ BassBoost_BassBoostItf = {
    BassBoost_SetEnabled,
    BassBoost_IsEnabled,
    BassBoost_SetStrength,
    BassBoost_GetRoundedStrength,
    BassBoost_IsStrengthSupported
};

/* 3DGrouping implementation */

static SLresult _3DGrouping_Set3DGroup(SL3DGroupingItf self, SLObjectItf group)
{
    struct _3DGrouping_interface *this = (struct _3DGrouping_interface *) self;
    this->mGroup = group;
    return SL_RESULT_SUCCESS;
}

static SLresult _3DGrouping_Get3DGroup(SL3DGroupingItf self,
    SLObjectItf *pGroup)
{
    if (NULL == pGroup)
        return SL_RESULT_PARAMETER_INVALID;
    struct _3DGrouping_interface *this = (struct _3DGrouping_interface *) self;
    *pGroup = this->mGroup;
    return SL_RESULT_SUCCESS;
}

/*static*/ const struct SL3DGroupingItf_ _3DGrouping_3DGroupingItf = {
    _3DGrouping_Set3DGroup,
    _3DGrouping_Get3DGroup
};

/* 3DMacroscopic implementation */

static SLresult _3DMacroscopic_SetSize(SL3DMacroscopicItf self,
    SLmillimeter width, SLmillimeter height, SLmillimeter depth)
{
    return SL_RESULT_SUCCESS;
}

static SLresult _3DMacroscopic_GetSize(SL3DMacroscopicItf self,
    SLmillimeter *pWidth, SLmillimeter *pHeight, SLmillimeter *pDepth)
{
    return SL_RESULT_SUCCESS;
}

static SLresult _3DMacroscopic_SetOrientationAngles(SL3DMacroscopicItf self,
    SLmillidegree heading, SLmillidegree pitch, SLmillidegree roll)
{
    return SL_RESULT_SUCCESS;
}

static SLresult _3DMacroscopic_SetOrientationVectors(SL3DMacroscopicItf self,
    const SLVec3D *pFront, const SLVec3D *pAbove)
{
    return SL_RESULT_SUCCESS;
}

static SLresult _3DMacroscopic_Rotate(SL3DMacroscopicItf self,
    SLmillidegree theta, const SLVec3D *pAxis)
{
    return SL_RESULT_SUCCESS;
}

static SLresult _3DMacroscopic_GetOrientationVectors(SL3DMacroscopicItf self,
    SLVec3D *pFront, SLVec3D *pUp)
{
    return SL_RESULT_SUCCESS;
}

/*static*/ const struct SL3DMacroscopicItf_ _3DMacroscopic_3DMacroscopicItf = {
    _3DMacroscopic_SetSize,
    _3DMacroscopic_GetSize,
    _3DMacroscopic_SetOrientationAngles,
    _3DMacroscopic_SetOrientationVectors,
    _3DMacroscopic_Rotate,
    _3DMacroscopic_GetOrientationVectors
};

/* 3DSource implementation */

static SLresult _3DSource_SetHeadRelative(SL3DSourceItf self,
    SLboolean headRelative)
{
    return SL_RESULT_SUCCESS;
}

static SLresult _3DSource_GetHeadRelative(SL3DSourceItf self,
    SLboolean *pHeadRelative)
{
    return SL_RESULT_SUCCESS;
}

static SLresult _3DSource_SetRolloffDistances(SL3DSourceItf self,
    SLmillimeter minDistance, SLmillimeter maxDistance)
{
    return SL_RESULT_SUCCESS;
}

static SLresult _3DSource_GetRolloffDistances(SL3DSourceItf self,
    SLmillimeter *pMinDistance, SLmillimeter *pMaxDistance)
{
    return SL_RESULT_SUCCESS;
}

static SLresult _3DSource_SetRolloffMaxDistanceMute(SL3DSourceItf self,
    SLboolean mute)
{
    return SL_RESULT_SUCCESS;
}

static SLresult _3DSource_GetRolloffMaxDistanceMute(SL3DSourceItf self,
    SLboolean *pMute)
{
    return SL_RESULT_SUCCESS;
}

static SLresult _3DSource_SetRolloffFactor(SL3DSourceItf self,
    SLpermille rolloffFactor)
{
    return SL_RESULT_SUCCESS;
}

static SLresult _3DSource_GetRolloffFactor(SL3DSourceItf self,
    SLpermille *pRolloffFactor)
{
    return SL_RESULT_SUCCESS;
}

static SLresult _3DSource_SetRoomRolloffFactor(SL3DSourceItf self,
    SLpermille roomRolloffFactor)
{
    return SL_RESULT_SUCCESS;
}

static SLresult _3DSource_GetRoomRolloffFactor(SL3DSourceItf self,
    SLpermille *pRoomRolloffFactor)
{
    return SL_RESULT_SUCCESS;
}

static SLresult _3DSource_SetRolloffModel(SL3DSourceItf self, SLuint8 model)
{
    return SL_RESULT_SUCCESS;
}

static SLresult _3DSource_GetRolloffModel(SL3DSourceItf self, SLuint8 *pModel)
{
    return SL_RESULT_SUCCESS;
}

static SLresult _3DSource_SetCone(SL3DSourceItf self, SLmillidegree innerAngle,
    SLmillidegree outerAngle, SLmillibel outerLevel)
{
    return SL_RESULT_SUCCESS;
}

static SLresult _3DSource_GetCone(SL3DSourceItf self,
    SLmillidegree *pInnerAngle, SLmillidegree *pOuterAngle,
    SLmillibel *pOuterLevel)
{
    return SL_RESULT_SUCCESS;
}

/*static*/ const struct SL3DSourceItf_ _3DSource_3DSourceItf = {
    _3DSource_SetHeadRelative,
    _3DSource_GetHeadRelative,
    _3DSource_SetRolloffDistances,
    _3DSource_GetRolloffDistances,
    _3DSource_SetRolloffMaxDistanceMute,
    _3DSource_GetRolloffMaxDistanceMute,
    _3DSource_SetRolloffFactor,
    _3DSource_GetRolloffFactor,
    _3DSource_SetRoomRolloffFactor,
    _3DSource_GetRoomRolloffFactor,
    _3DSource_SetRolloffModel,
    _3DSource_GetRolloffModel,
    _3DSource_SetCone,
    _3DSource_GetCone
};

/* Equalizer implementation */

static SLresult Equalizer_SetEnabled(SLEqualizerItf self, SLboolean enabled)
{
    return SL_RESULT_SUCCESS;
}

static SLresult Equalizer_IsEnabled(SLEqualizerItf self, SLboolean *pEnabled)
{
    return SL_RESULT_SUCCESS;
}

static SLresult Equalizer_GetNumberOfBands(SLEqualizerItf self,
    SLuint16 *pNumBands)
{
    return SL_RESULT_SUCCESS;
}

static SLresult Equalizer_GetBandLevelRange(SLEqualizerItf self,
    SLmillibel *pMin, SLmillibel *pMax)
{
    return SL_RESULT_SUCCESS;
}

static SLresult Equalizer_SetBandLevel(SLEqualizerItf self, SLuint16 band,
    SLmillibel level)
{
    return SL_RESULT_SUCCESS;
}

static SLresult Equalizer_GetBandLevel(SLEqualizerItf self, SLuint16 band,
    SLmillibel *pLevel)
{
    return SL_RESULT_SUCCESS;
}

static SLresult Equalizer_GetCenterFreq(SLEqualizerItf self, SLuint16 band,
    SLmilliHertz *pCenter)
{
    return SL_RESULT_SUCCESS;
}

static SLresult Equalizer_GetBandFreqRange(SLEqualizerItf self, SLuint16 band,
    SLmilliHertz *pMin, SLmilliHertz *pMax)
{
    return SL_RESULT_SUCCESS;
}

static SLresult Equalizer_GetBand(SLEqualizerItf self, SLmilliHertz frequency,
    SLuint16 *pBand)
{
    return SL_RESULT_SUCCESS;
}

static SLresult Equalizer_GetCurrentPreset(SLEqualizerItf self,
    SLuint16 *pPreset)
{
    return SL_RESULT_SUCCESS;
}

static SLresult Equalizer_UsePreset(SLEqualizerItf self, SLuint16 index)
{
    return SL_RESULT_SUCCESS;
}

static SLresult Equalizer_GetNumberOfPresets(SLEqualizerItf self,
    SLuint16 *pNumPresets)
{
    return SL_RESULT_SUCCESS;
}

static SLresult Equalizer_GetPresetName(SLEqualizerItf self, SLuint16 index,
    const SLchar ** ppName)
{
    return SL_RESULT_SUCCESS;
}

/*static*/ const struct SLEqualizerItf_ Equalizer_EqualizerItf = {
    Equalizer_SetEnabled,
    Equalizer_IsEnabled,
    Equalizer_GetNumberOfBands,
    Equalizer_GetBandLevelRange,
    Equalizer_SetBandLevel,
    Equalizer_GetBandLevel,
    Equalizer_GetCenterFreq,
    Equalizer_GetBandFreqRange,
    Equalizer_GetBand,
    Equalizer_GetCurrentPreset,
    Equalizer_UsePreset,
    Equalizer_GetNumberOfPresets,
    Equalizer_GetPresetName
};

// PresetReverb implementation

static SLresult PresetReverb_SetPreset(SLPresetReverbItf self,
    SLuint16 preset)
{
    struct PresetReverb_interface *this =
        (struct PresetReverb_interface *) self;
    this->mPreset = preset;
    return SL_RESULT_SUCCESS;
}

static SLresult PresetReverb_GetPreset(SLPresetReverbItf self,
    SLuint16 *pPreset)
{
    if (NULL == pPreset)
        return SL_RESULT_PARAMETER_INVALID;
    struct PresetReverb_interface *this =
        (struct PresetReverb_interface *) self;
    *pPreset = this->mPreset;
    return SL_RESULT_SUCCESS;
}

/*static*/ const struct SLPresetReverbItf_ PresetReverb_PresetReverbItf = {
    PresetReverb_SetPreset,
    PresetReverb_GetPreset
};

// EnvironmentalReverb implementation

static SLresult EnvironmentalReverb_SetRoomLevel(SLEnvironmentalReverbItf self,
    SLmillibel room)
{
    return SL_RESULT_SUCCESS;
}

static SLresult EnvironmentalReverb_GetRoomLevel(SLEnvironmentalReverbItf self,
    SLmillibel *pRoom)
{
    return SL_RESULT_SUCCESS;
}

static SLresult EnvironmentalReverb_SetRoomHFLevel(
    SLEnvironmentalReverbItf self, SLmillibel roomHF)
{
    return SL_RESULT_SUCCESS;
}

static SLresult EnvironmentalReverb_GetRoomHFLevel(
    SLEnvironmentalReverbItf self, SLmillibel *pRoomHF)
{
    return SL_RESULT_SUCCESS;
}

static SLresult EnvironmentalReverb_SetDecayTime(
    SLEnvironmentalReverbItf self, SLmillisecond decayTime)
{
    return SL_RESULT_SUCCESS;
}

static SLresult EnvironmentalReverb_GetDecayTime(
    SLEnvironmentalReverbItf self, SLmillisecond *pDecayTime)
{
    return SL_RESULT_SUCCESS;
}

static SLresult EnvironmentalReverb_SetDecayHFRatio(
    SLEnvironmentalReverbItf self, SLpermille decayHFRatio)
{
    return SL_RESULT_SUCCESS;
}

static SLresult EnvironmentalReverb_GetDecayHFRatio(
    SLEnvironmentalReverbItf self, SLpermille *pDecayHFRatio)
{
    return SL_RESULT_SUCCESS;
}

static SLresult EnvironmentalReverb_SetReflectionsLevel(
    SLEnvironmentalReverbItf self, SLmillibel reflectionsLevel)
{
    return SL_RESULT_SUCCESS;
}

static SLresult EnvironmentalReverb_GetReflectionsLevel(
    SLEnvironmentalReverbItf self, SLmillibel *pReflectionsLevel)
{
    return SL_RESULT_SUCCESS;
}

static SLresult EnvironmentalReverb_SetReflectionsDelay(
    SLEnvironmentalReverbItf self, SLmillisecond reflectionsDelay)
{
    return SL_RESULT_SUCCESS;
}

static SLresult EnvironmentalReverb_GetReflectionsDelay(
    SLEnvironmentalReverbItf self, SLmillisecond *pReflectionsDelay)
{
    return SL_RESULT_SUCCESS;
}

static SLresult EnvironmentalReverb_SetReverbLevel(
    SLEnvironmentalReverbItf self, SLmillibel reverbLevel)
{
    return SL_RESULT_SUCCESS;
}

static SLresult EnvironmentalReverb_GetReverbLevel(
    SLEnvironmentalReverbItf self, SLmillibel *pReverbLevel)
{
    return SL_RESULT_SUCCESS;
}

static SLresult EnvironmentalReverb_SetReverbDelay(
    SLEnvironmentalReverbItf self, SLmillisecond reverbDelay)
{
    return SL_RESULT_SUCCESS;
}

static SLresult EnvironmentalReverb_GetReverbDelay(
    SLEnvironmentalReverbItf self, SLmillisecond *pReverbDelay)
{
    return SL_RESULT_SUCCESS;
}

static SLresult EnvironmentalReverb_SetDiffusion(
    SLEnvironmentalReverbItf self, SLpermille diffusion)
{
    return SL_RESULT_SUCCESS;
}

static SLresult EnvironmentalReverb_GetDiffusion(SLEnvironmentalReverbItf self,
     SLpermille *pDiffusion)
{
    return SL_RESULT_SUCCESS;
}

static SLresult EnvironmentalReverb_SetDensity(SLEnvironmentalReverbItf self,
    SLpermille density)
{
    return SL_RESULT_SUCCESS;
}

static SLresult EnvironmentalReverb_GetDensity(SLEnvironmentalReverbItf self,
    SLpermille *pDensity)
{
    return SL_RESULT_SUCCESS;
}

static SLresult EnvironmentalReverb_SetEnvironmentalReverbProperties(
    SLEnvironmentalReverbItf self,
    const SLEnvironmentalReverbSettings *pProperties)
{
    return SL_RESULT_SUCCESS;
}

static SLresult EnvironmentalReverb_GetEnvironmentalReverbProperties(
    SLEnvironmentalReverbItf self, SLEnvironmentalReverbSettings *pProperties)
{
    return SL_RESULT_SUCCESS;
}

/*static*/ const struct SLEnvironmentalReverbItf_
    EnvironmentalReverb_EnvironmentalReverbItf = {
    EnvironmentalReverb_SetRoomLevel,
    EnvironmentalReverb_GetRoomLevel,
    EnvironmentalReverb_SetRoomHFLevel,
    EnvironmentalReverb_GetRoomHFLevel,
    EnvironmentalReverb_SetDecayTime,
    EnvironmentalReverb_GetDecayTime,
    EnvironmentalReverb_SetDecayHFRatio,
    EnvironmentalReverb_GetDecayHFRatio,
    EnvironmentalReverb_SetReflectionsLevel,
    EnvironmentalReverb_GetReflectionsLevel,
    EnvironmentalReverb_SetReflectionsDelay,
    EnvironmentalReverb_GetReflectionsDelay,
    EnvironmentalReverb_SetReverbLevel,
    EnvironmentalReverb_GetReverbLevel,
    EnvironmentalReverb_SetReverbDelay,
    EnvironmentalReverb_GetReverbDelay,
    EnvironmentalReverb_SetDiffusion,
    EnvironmentalReverb_GetDiffusion,
    EnvironmentalReverb_SetDensity,
    EnvironmentalReverb_GetDensity,
    EnvironmentalReverb_SetEnvironmentalReverbProperties,
    EnvironmentalReverb_GetEnvironmentalReverbProperties
};

// Virtualizer implementation

static SLresult Virtualizer_SetEnabled(SLVirtualizerItf self, SLboolean enabled)
{
    struct Virtualizer_interface *this = (struct Virtualizer_interface *) self;
    this->mEnabled = enabled;
    return SL_RESULT_SUCCESS;
}

static SLresult Virtualizer_IsEnabled(SLVirtualizerItf self,
    SLboolean *pEnabled)
{
    if (NULL == pEnabled)
        return SL_RESULT_PARAMETER_INVALID;
    struct Virtualizer_interface *this = (struct Virtualizer_interface *) self;
    *pEnabled = this->mEnabled;
    return SL_RESULT_SUCCESS;
}

static SLresult Virtualizer_SetStrength(SLVirtualizerItf self,
    SLpermille strength)
{
    struct Virtualizer_interface *this = (struct Virtualizer_interface *) self;
    this->mStrength = strength;
    return SL_RESULT_SUCCESS;
}

static SLresult Virtualizer_GetRoundedStrength(SLVirtualizerItf self,
    SLpermille *pStrength)
{
    if (NULL == pStrength)
        return SL_RESULT_PARAMETER_INVALID;
    struct Virtualizer_interface *this = (struct Virtualizer_interface *) self;
    *pStrength = this->mStrength;
    return SL_RESULT_SUCCESS;
}

static SLresult Virtualizer_IsStrengthSupported(SLVirtualizerItf self,
    SLboolean *pSupported)
{
    if (NULL == pSupported)
        return SL_RESULT_PARAMETER_INVALID;
    *pSupported = SL_BOOLEAN_TRUE;
    return SL_RESULT_SUCCESS;
}

/*static*/ const struct SLVirtualizerItf_ Virtualizer_VirtualizerItf = {
    Virtualizer_SetEnabled,
    Virtualizer_IsEnabled,
    Virtualizer_SetStrength,
    Virtualizer_GetRoundedStrength,
    Virtualizer_IsStrengthSupported
};

/* Initial entry points */

SLresult SLAPIENTRY slCreateEngine(SLObjectItf *pEngine, SLuint32 numOptions,
    const SLEngineOption *pEngineOptions, SLuint32 numInterfaces,
    const SLInterfaceID *pInterfaceIds, const SLboolean *pInterfaceRequired)
{
    if (NULL == pEngine)
        return SL_RESULT_PARAMETER_INVALID;
    *pEngine = NULL;
    // default values
    SLboolean threadSafe = SL_BOOLEAN_TRUE;
    SLboolean lossOfControl = SL_BOOLEAN_FALSE;
    if (NULL != pEngineOptions) {
        SLuint32 i;
		const SLEngineOption *option = pEngineOptions;
        for (i = 0; i < numOptions; ++i, ++option) {
            switch (option->feature) {
            case SL_ENGINEOPTION_THREADSAFE:
                threadSafe = (SLboolean) option->data;
                break;
            case SL_ENGINEOPTION_LOSSOFCONTROL:
                lossOfControl = (SLboolean) option->data;
                break;
            default:
                return SL_RESULT_PARAMETER_INVALID;
            }
        }
    }
    unsigned exposedMask;
    SLresult result = checkInterfaces(&Engine_class, numInterfaces,
        pInterfaceIds, pInterfaceRequired, &exposedMask);
    if (SL_RESULT_SUCCESS != result)
        return result;
    struct Engine_class *this =
        (struct Engine_class *) construct(&Engine_class, exposedMask);
    if (NULL == this)
        return SL_RESULT_MEMORY_FAILURE;
    this->mThreadSafe = threadSafe;
    this->mLossOfControl = lossOfControl;
    *pEngine = &this->mObject.mItf;
    return SL_RESULT_SUCCESS;
}

SLresult SLAPIENTRY slQueryNumSupportedEngineInterfaces(
    SLuint32 *pNumSupportedInterfaces)
{
    if (NULL == pNumSupportedInterfaces)
        return SL_RESULT_PARAMETER_INVALID;
    *pNumSupportedInterfaces = Engine_class.mInterfaceCount;
    return SL_RESULT_SUCCESS;
}

SLresult SLAPIENTRY slQuerySupportedEngineInterfaces(SLuint32 index,
    SLInterfaceID *pInterfaceId)
{
    if (sizeof(Engine_interfaces)/sizeof(Engine_interfaces[0]) < index)
        return SL_RESULT_PARAMETER_INVALID;
    if (NULL == pInterfaceId)
        return SL_RESULT_PARAMETER_INVALID;
    *pInterfaceId = &SL_IID_array[Engine_interfaces[index].mMPH];
    return SL_RESULT_SUCCESS;
}

#ifdef USE_OUTPUTMIXEXT

/* OutputMixExt implementation */

// Used by SDL and Android but not specific to or dependent on either platform

static void OutputMixExt_FillBuffer(SLOutputMixExtItf self, void *pBuffer,
    SLuint32 size)
{
    // Force to be a multiple of a frame, assumes stereo 16-bit PCM
    size &= ~3;
    struct OutputMixExt_interface *thisExt =
        (struct OutputMixExt_interface *) self;
    struct OutputMix_interface *this =
        &((struct OutputMix_class *) thisExt->this)->mOutputMix;
    unsigned activeMask = this->mActiveMask;
    struct Track *track = &this->mTracks[0];
    unsigned i;
    SLboolean mixBufferHasData = SL_BOOLEAN_FALSE;
    // FIXME O(32) loop even when few tracks are active.
    // To avoid loop, use activeMask to check for active track(s)
    // and decide whether we actually need to copy or mix.
    for (i = 0; 0 != activeMask; ++i, ++track, activeMask >>= 1) {
        assert(i < 32);
        if (!(activeMask & 1))
            continue;
        // track is allocated
        struct Play_interface *play = track->mPlay;
        if (NULL == play)
            continue;
        // track is initialized
        if (SL_PLAYSTATE_PLAYING != play->mState)
            continue;
        // track is playing
        void *dstWriter = pBuffer;
        unsigned desired = size;
        SLboolean trackContributedToMix = SL_BOOLEAN_FALSE;
        while (desired > 0) {
            struct BufferQueue_interface *bufferQueue;
            const struct BufferHeader *oldFront, *newFront, *rear;
            unsigned actual = desired;
            if (track->mAvail < actual)
                actual = track->mAvail;
            // force actual to be a frame multiple
            if (actual > 0) {
                // FIXME check for either mute or volume 0
                // in which case skip the input buffer processing
                assert(NULL != track->mReader);
                // FIXME && gain == 1.0
                if (mixBufferHasData) {
                    stereo *mixBuffer = (stereo *) dstWriter;
                    const stereo *source = (const stereo *) track->mReader;
                    unsigned j;
                    for (j = 0; j < actual; j += sizeof(stereo), ++mixBuffer,
                        ++source) {
                        // apply gain here
                        mixBuffer->left += source->left;
                        mixBuffer->right += source->right;
                    }
                } else {
                    memcpy(dstWriter, track->mReader, actual);
                    trackContributedToMix = SL_BOOLEAN_TRUE;
                }
                dstWriter = (char *) dstWriter + actual;
                desired -= actual;
                track->mReader = (char *) track->mReader + actual;
                track->mAvail -= actual;
                if (track->mAvail == 0) {
                    bufferQueue = track->mBufferQueue;
                    if (NULL != bufferQueue) {
                        oldFront = bufferQueue->mFront;
                        rear = bufferQueue->mRear;
                        assert(oldFront != rear);
                        newFront = oldFront;
                        if (++newFront ==
                            &bufferQueue->mArray[bufferQueue->mNumBuffers])
                            newFront = bufferQueue->mArray;
                        bufferQueue->mFront = (struct BufferHeader *) newFront;
                        assert(0 < bufferQueue->mState.count);
                        --bufferQueue->mState.count;
                        // FIXME here or in Enqueue?
                        ++bufferQueue->mState.playIndex;
                        // FIXME a good time to do an early warning
                        // callback depending on buffer count
                    }
                }
                continue;
            }
            // actual == 0
            bufferQueue = track->mBufferQueue;
            if (NULL != bufferQueue) {
                oldFront = bufferQueue->mFront;
                rear = bufferQueue->mRear;
                if (oldFront != rear) {
got_one:
                    assert(0 < bufferQueue->mState.count);
                    track->mReader = oldFront->mBuffer;
                    track->mAvail = oldFront->mSize;
                    continue;
                }
                // FIXME should be able to configure when to
                // kick off the callback e.g. high/low water-marks etc.
                // need data but none available, attempt a desperate callback
                slBufferQueueCallback callback = bufferQueue->mCallback;
                if (NULL != callback) {
                    (*callback)((SLBufferQueueItf) bufferQueue,
                        bufferQueue->mContext);
                    // if lucky, the callback enqueued a buffer
                    if (rear != bufferQueue->mRear)
                        goto got_one;
                    // unlucky, queue still empty, the callback failed
                }
                // here on underflow due to no callback, or failed callback
                // FIXME underflow, send silence (or previous buffer?)
                // we did a callback to try to kick start again but failed
                // should log this
            }
            // no buffer queue or underflow, clear out rest of partial buffer
            if (!mixBufferHasData && trackContributedToMix)
                memset(dstWriter, 0, actual);
            break;
        }
        if (trackContributedToMix)
            mixBufferHasData = SL_BOOLEAN_TRUE;
    }
    // No active tracks, so output silence
    if (!mixBufferHasData)
        memset(pBuffer, 0, size);
}

/*static*/ const struct SLOutputMixExtItf_ OutputMixExt_OutputMixExtItf = {
    OutputMixExt_FillBuffer
};

#endif // USE_OUTPUTMIXEXT

#ifdef USE_SDL

// FIXME move to separate source file

/* SDL platform implementation */

static void SDLCALL SDL_callback(void *context, Uint8 *stream, int len)
{
    assert(len > 0);
    OutputMixExt_FillBuffer((SLOutputMixExtItf) context, stream,
        (SLuint32) len);
}

void SDL_start(SLObjectItf self)
{
    //assert(self != NULL);
    // FIXME make this an operation on Object: GetClass
    //struct Object_interface *this = (struct Object_interface *) self;
    //assert(&OutputMix_class == this->mClass);
    SLresult result;
    SLOutputMixExtItf OutputMixExt;
    result = (*self)->GetInterface(self, SL_IID_OUTPUTMIXEXT, &OutputMixExt);
    assert(SL_RESULT_SUCCESS == result);

    SDL_AudioSpec fmt;
    fmt.freq = 44100;
    fmt.format = AUDIO_S16;
    fmt.channels = 2;
    fmt.samples = 256;
    fmt.callback = SDL_callback;
    // FIXME should be a GetInterface
    // fmt.userdata = &((struct OutputMix_class *) this)->mOutputMixExt;
    fmt.userdata = (void *) OutputMixExt;

    if (SDL_OpenAudio(&fmt, NULL) < 0) {
        fprintf(stderr, "Unable to open audio: %s\n", SDL_GetError());
        exit(1);
    }
    SDL_PauseAudio(0);
}

#endif // USE_SDL

/* End */
