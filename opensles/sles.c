/* Copyright 2010 The Android Open Source Project */

/* OpenSL ES prototype */

#include "OpenSLES.h"
#include <stddef.h> // offsetof
#include <stdlib.h> // malloc
#include <string.h> // memcmp
#include <stdio.h> // debugging
#include <assert.h>

#ifdef USE_SNDFILE
#include <sndfile.h>
#endif
#ifdef USE_SDL
#include <SDL/SDL_audio.h>
#endif

/* Forward declarations */

static const struct SLObjectItf_ Object_ObjectItf;
static const struct SLDynamicInterfaceManagementItf_
    DynamicInterfaceManagement_DynamicInterfaceManagementItf;
static const struct SLEngineItf_ Engine_EngineItf;
static const struct SLOutputMixItf_ OutputMix_OutputMixItf;
static const struct SLPlayItf_ Play_PlayItf;
static const struct SLVolumeItf_ Volume_VolumeItf;
static const struct SLBufferQueueItf_ BufferQueue_BufferQueueItf;
static const struct SLAudioIODeviceCapabilitiesItf_
    AudioIODeviceCapabilities_AudioIODeviceCapabilitiesItf;
static const struct SLSeekItf_ Seek_SeekItf;
static const struct SLLEDArrayItf_ LEDArray_LEDArrayItf;
static const struct SLVibraItf_ Vibra_VibraItf;
static const struct SL3DCommitItf_ _3DCommit_3DCommitItf;
static const struct SL3DDopplerItf_ _3DDoppler_3DDopplerItf;
static const struct SL3DLocationItf_ _3DLocation_3DLocationItf;
static const struct SLAudioDecoderCapabilitiesItf_
    AudioDecoderCapabilities_AudioDecoderCapabilitiesItf;
static const struct SLAudioEncoderItf_ AudioEncoder_AudioEncoderItf;
static const struct SLAudioEncoderCapabilitiesItf_
    AudioEncoderCapabilities_AudioEncoderCapabilitiesItf;
static const struct SLDeviceVolumeItf_ DeviceVolume_DeviceVolumeItf;
static const struct SLDynamicSourceItf_ DynamicSource_DynamicSourceItf;
static const struct SLEffectSendItf_ EffectSend_EffectSendItf;
static const struct SLEngineCapabilitiesItf_
    EngineCapabilities_EngineCapabilitiesItf;
static const struct SLMetadataExtractionItf_ MetadataExtraction_MetadataExtractionItf;
static const struct SLMetadataTraversalItf_ MetadataTraversal_MetadataTraversalItf;
static const struct SLMuteSoloItf_ MuteSolo_MuteSoloItf;
static const struct SLPitchItf_ Pitch_PitchItf;
static const struct SLPlaybackRateItf_ PlaybackRate_PlaybackRateItf;
static const struct SLPrefetchStatusItf_ PrefetchStatus_PrefetchStatusItf;
static const struct SLRatePitchItf_ RatePitch_RatePitchItf;
static const struct SLRecordItf_ Record_RecordItf;
static const struct SLThreadSyncItf_ ThreadSync_ThreadSyncItf;
static const struct SLVisualizationItf_ Visualization_VisualizationItf;

/* Private types */

struct iid_vtable {
    const SLInterfaceID *iid;
    size_t offset;
};

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

#define DEVICE_ID_LED 3
#define DEVICE_ID_VIBRA 4

/* Per-class data shared by all instances of the same class */

struct class {
    // needed by all classes (class class, the superclass of all classes)
    const struct iid_vtable *interfaces;
    const SLuint32 interfaceCount;
    const char * const name;
// FIXME not yet used
    const size_t size;
    // non-const here and below
// FIXME not yet used
    SLuint32 instanceCount;
    // append per-class data here
};

/* Interfaces */

struct Object_interface {
    const struct SLObjectItf_ *itf;
    // probably not needed for an Object, as it is always first
    void *this;
    // additional fields
    struct class *mClass;
    volatile SLuint32 mState;
    slObjectCallback mCallback;
    void *mContext;
    // FIXME a thread lock would go here
    // FIXME also an object ID for RPC and human-readable name for debugging
};

struct DynamicInterfaceManagement_interface {
    const struct SLDynamicInterfaceManagementItf_ *itf;
    void *this;
    // additional fields
};

struct Engine_interface {
    const struct SLEngineItf_ *itf;
    void *this;
    // additional fields
};

struct OutputMix_interface {
    const struct SLOutputMixItf_ *itf;
    void *this;
    // additional fields
    unsigned mActiveMask;   // 1 bit per active track
    struct Track {
        const SLDataFormat_PCM *mDfPcm;
        struct BufferQueue_interface *mBufferQueue;
        struct Play_interface *mPlay; // mixer will examine this track if non-NULL
        const void *mReader;    // pointer to next frame in BufferHeader.mBuffer
        SLuint32 mAvail;        // number of available bytes
    } mTracks[32];
};

struct Play_interface {
    const struct SLPlayItf_ *itf;
    void *this;
    // additional fields
    volatile SLuint32 mState;
    SLmillisecond mDuration;
    SLmillisecond mPosition;
    // unsigned mPositionSamples;  // position in sample units
};

struct BufferQueue_interface {
    const struct SLBufferQueueItf_ *itf;
    void *this;
    // additional fields
    volatile SLBufferQueueState mState;
    slBufferQueueCallback mCallback;
    void *mContext;
    SLuint32 mNumBuffers;
    // circular
    struct BufferHeader {
        const void *mBuffer;
        SLuint32 mSize;
    } *mArray, *mFront, *mRear;
    // FIXME inline alloc of mArray
};

struct EnvironmentalReverb_interface {
    const struct SLEnvironmentalReverbItf_ *itf;
    void *this;
    // additional fields
};

struct Equalizer_interface {
    const struct SLEqualizerItf_ *itf;
    void *this;
    // additional fields
};

struct PresetReverb_interface {
    const struct SLPresetReverbItf_ *itf;
    void *this;
    // additional fields
};

struct Virtualizer_interface {
    const struct SLVirtualizerItf_ *itf;
    void *this;
    // additional fields
};

struct ThreadSync_interface {
    const struct SLThreadSyncItf_ *itf;
    void *this;
    // additional fields
};

struct Volume_interface {
    const struct SLVolumeItf_ *itf;
    void *this;
    // additional fields
    SLmillibel mLevel;
    SLboolean mMute;
    SLboolean mEnableStereoPosition;
    SLpermille mStereoPosition;
};

struct AudioIODeviceCapabilities_interface {
    const struct SLAudioIODeviceCapabilitiesItf_ *itf;
    void *this;
    // additional fields
};

struct Seek_interface {
    const struct SLSeekItf_ *itf;
    void *this;
    // additional fields
    SLmillisecond mPos;
    SLboolean mLoopEnabled;
    SLmillisecond mStartPos;
    SLmillisecond mEndPos;
};

struct _3DCommit_interface {
    const struct SL3DCommitItf_ *itf;
    void *this;
    // additional fields
};

struct EngineCapabilities_interface {
    const struct SLEngineCapabilitiesItf_ *itf;
    void *this;
    // additional fields
};

struct AudioDecoderCapabilities_interface {
    const struct SLAudioDecoderCapabilitiesItf_ *itf;
    void *this;
    // additional fields
};

struct AudioEncoderCapabilities_interface {
    const struct SLAudioEncoderCapabilitiesItf_ *itf;
    void *this;
    // additional fields
};

struct DeviceVolume_interface {
    const struct SLDeviceVolumeItf_ *itf;
    void *this;
    // additional fields
};

struct LEDArray_interface {
    const struct SLLEDArrayItf_ *itf;
    void *this;
    // additional fields
};

struct Vibra_interface {
    const struct SLVibraItf_ *itf;
    void *this;
    // additional fields
};

struct PrefetchStatus_interface {
    const struct SLPrefetchStatusItf_ *itf;
    void *this;
    // additional fields
};

struct RatePitch_interface {
    const struct SLRatePitchItf_ *itf;
    void *this;
    // additional fields
};

struct EffectSend_interface {
    const struct SLEffectSendItf_ *itf;
    void *this;
    // additional fields
};

struct MuteSolo_interface {
    const struct SLMuteSoloItf_ *itf;
    void *this;
    // additional fields
};

struct MetaDataExtraction_interface {
    const struct SLMetaDataExractionItf_ *itf;
    void *this;
    // additional fields
};

struct MetaDataTraversal_interface {
    const struct SLMetaDataTraversalItf_ *itf;
    void *this;
    // additional fields
};

struct Record_interface {
    const struct SLRecordItf_ *itf;
    void *this;
    // additional fields
};

struct AudioEncoder_interface {
    const struct SLAudioEncoderItf_ *itf;
    void *this;
    // additional fields
};

struct _3DDoppler_interface {
    const struct SL3DDopplerItf_ *itf;
    void *this;
    // additional fields
};

struct _3DLocation_interface {
    const struct SL3DLocationItf_ *itf;
    void *this;
    // additional fields
};

struct DynamicSource_interface {
    const struct SLDynamicSourceItf_ *itf;
    void *this;
    // additional fields
};

/* Classes */

struct Engine_class {
    struct Object_interface mObject;
    struct DynamicInterfaceManagement_interface mDynamicInterfaceManagement;
    struct Engine_interface mEngine;
    struct EngineCapabilities_interface mEngineCapabilities;
    struct ThreadSync_interface mThreadSync;
    struct AudioIODeviceCapabilities_interface mAudioIODeviceCapabilities;
    struct AudioDecoderCapabilities_interface mAudioDecoderCapabilities;
    struct AudioEncoderCapabilities_interface mAudioEncoderCapabilities;
    struct _3DCommit_interface m3DCommit;
    struct DeviceVolume_interface mDeviceVolume;
    // additional fields not associated with interfaces goes here
};

struct OutputMix_class {
    struct Object_interface mObject;
    struct DynamicInterfaceManagement_interface mDynamicInterfaceManagement;
    struct OutputMix_interface mOutputMix;
    struct EnvironmentalReverb_interface mEnvironmentalReverb;
    struct Equalizer_interface mEqualizer;
    struct PresetReverb_interface mPresetReverb;
    struct Virtualizer_interface mVirtualizer;
    struct Volume_interface mVolume;
};

struct LEDDevice_class {
    struct Object_interface mObject;
    struct DynamicInterfaceManagement_interface mDynamicInterfaceManagement;
    struct LEDArray_interface mLED;
    //
    SLuint32 mDeviceID;
};

struct VibraDevice_class {
    struct Object_interface mObject;
    struct DynamicInterfaceManagement_interface mDynamicInterfaceManagement;
    struct Vibra_interface mVibra;
    //
    SLuint32 mDeviceID;
};

struct AudioPlayer_class {
    struct Object_interface mObject;
    struct DynamicInterfaceManagement_interface mDynamicInterfaceManagement;
    struct Play_interface mPlay;
    // 3D interfaces
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
    // rest of fields are not related to the interfaces
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
    } mSndFile;
#endif
};

struct AudioRecorder_class {
    struct Object_interface mObject;
    struct DynamicInterfaceManagement_interface mDynamicInterfaceManagement;
    struct Record_interface mRecord;
    struct AudioEncoder_interface mAudioEncoder;
    // optional
    struct Volume_interface mVolume;
};

struct Listener_class {
    struct Object_interface mObject;
    struct DynamicInterfaceManagement_interface mDynamicInterfaceManagement;
    struct _3DDoppler_interface m3DDoppler;
    struct _3DLocation_interface m3DLocation;
};

struct MetadataExtractor_class {
    struct Object_interface mObject;
    struct DynamicInterfaceManagement_interface mDynamicInterfaceManagement;
    struct DynamicSource_interface mDynamicSource;
    struct MetaDataExtraction_interface mMetaDataExtraction;
    struct MetaDataTraversal_interface mMetaDataTraversal;
};

struct MidiPlayer_class {
    struct Object_interface mObject;
    struct DynamicInterfaceManagement_interface mDynamicInterfaceManagement;
    struct Play_interface mPlay;
    struct Volume_interface mVolume;
    struct Seek_interface mSeek;
};

/* Private functions */

static SLresult checkInterfaces(const struct iid_vtable *interfaces,
    SLuint32 interfaceCount, SLuint32 numInterfaces,
    const SLInterfaceID *pInterfaceIds, const SLboolean *pInterfaceRequired)
{
    if (0 < numInterfaces) {
        if (NULL == pInterfaceIds || NULL == pInterfaceRequired)
            return SL_RESULT_PARAMETER_INVALID;
        // FIXME O(N^2)
        SLuint32 i, j;
        for (i = 0; i < numInterfaces; ++i) {
            for (j = 0; j < interfaceCount; ++j) {
                if (pInterfaceIds[i] == *interfaces[j].iid ||
                !memcmp(pInterfaceIds[i], *interfaces[j].iid,
                sizeof(struct SLInterfaceID_)))
                    goto found;
            }
            if (pInterfaceRequired[i])
                return SL_RESULT_FEATURE_UNSUPPORTED;
found:
            ;
        }
    }
    return SL_RESULT_SUCCESS;
}

/* Classes */

static const struct iid_vtable Engine_interfaces[] = {
    {&SL_IID_OBJECT, offsetof(struct Engine_class, mObject)},
    {&SL_IID_DYNAMICINTERFACEMANAGEMENT,
        offsetof(struct Engine_class, mDynamicInterfaceManagement)},
    {&SL_IID_ENGINE, offsetof(struct Engine_class, mEngine)},
    {&SL_IID_ENGINECAPABILITIES, offsetof(struct Engine_class, mEngineCapabilities)},
    {&SL_IID_THREADSYNC, offsetof(struct Engine_class, mThreadSync)},
    {&SL_IID_AUDIOIODEVICECAPABILITIES,
        offsetof(struct Engine_class, mAudioIODeviceCapabilities)},
    {&SL_IID_AUDIODECODERCAPABILITIES,
        offsetof(struct Engine_class, mAudioDecoderCapabilities)},
    {&SL_IID_AUDIOENCODERCAPABILITIES,
        offsetof(struct Engine_class, mAudioEncoderCapabilities)},
    {&SL_IID_3DCOMMIT, offsetof(struct Engine_class, m3DCommit)},
    {&SL_IID_DEVICEVOLUME, offsetof(struct Engine_class, mDeviceVolume)}
};

static struct class Engine_class = {
    Engine_interfaces,
    sizeof(Engine_interfaces)/sizeof(Engine_interfaces[0]),
    "Engine",
    sizeof(struct Engine_class),
    0
};

static const struct iid_vtable OutputMix_interfaces[] = {
    {&SL_IID_OBJECT, offsetof(struct OutputMix_class, mObject)},
    {&SL_IID_DYNAMICINTERFACEMANAGEMENT,
        offsetof(struct OutputMix_class, mDynamicInterfaceManagement)},
    {&SL_IID_OUTPUTMIX, offsetof(struct OutputMix_class, mOutputMix)},
    {&SL_IID_VOLUME, offsetof(struct OutputMix_class, mVolume)}
};

static struct class OutputMix_class = {
    OutputMix_interfaces,
    sizeof(OutputMix_interfaces)/sizeof(OutputMix_interfaces[0]),
    "OutputMix",
    sizeof(struct OutputMix_class),
    0
};

static const struct iid_vtable AudioPlayer_interfaces[] = {
    {&SL_IID_OBJECT, offsetof(struct AudioPlayer_class, mObject)},
    {&SL_IID_DYNAMICINTERFACEMANAGEMENT,
        offsetof(struct AudioPlayer_class, mDynamicInterfaceManagement)},
    {&SL_IID_PLAY, offsetof(struct AudioPlayer_class, mPlay)},
    {&SL_IID_BUFFERQUEUE, offsetof(struct AudioPlayer_class, mBufferQueue)},
    {&SL_IID_VOLUME, offsetof(struct AudioPlayer_class, mVolume)},
    {&SL_IID_SEEK, offsetof(struct AudioPlayer_class, mSeek)}
};

static struct class AudioPlayer_class = {
    AudioPlayer_interfaces,
    sizeof(AudioPlayer_interfaces)/sizeof(AudioPlayer_interfaces[0]),
    "AudioPlayer",
    sizeof(struct AudioPlayer_class),
    0
};

static const struct iid_vtable LEDDevice_interfaces[] = {
    {&SL_IID_OBJECT, offsetof(struct LEDDevice_class, mObject)},
    {&SL_IID_DYNAMICINTERFACEMANAGEMENT,
        offsetof(struct LEDDevice_class, mDynamicInterfaceManagement)},
    {&SL_IID_LED, offsetof(struct LEDDevice_class, mLED)},
};

static struct class LEDDevice_class = {
    LEDDevice_interfaces,
    sizeof(LEDDevice_interfaces)/sizeof(LEDDevice_interfaces[0]),
    "LEDDevice",
    sizeof(struct LEDDevice_class),
    0
};

static const struct iid_vtable VibraDevice_interfaces[] = {
    {&SL_IID_OBJECT, offsetof(struct VibraDevice_class, mObject)},
    {&SL_IID_DYNAMICINTERFACEMANAGEMENT,
        offsetof(struct VibraDevice_class, mDynamicInterfaceManagement)},
    {&SL_IID_VIBRA, offsetof(struct VibraDevice_class, mVibra)},
};

static struct class VibraDevice_class = {
    VibraDevice_interfaces,
    sizeof(VibraDevice_interfaces)/sizeof(VibraDevice_interfaces[0]),
    "VibraDevice",
    sizeof(struct VibraDevice_class),
    0
};

static const struct iid_vtable MidiPlayer_interfaces[] = {
    {&SL_IID_OBJECT, offsetof(struct MidiPlayer_class, mObject)},
    {&SL_IID_DYNAMICINTERFACEMANAGEMENT,
        offsetof(struct MidiPlayer_class, mDynamicInterfaceManagement)},
    {&SL_IID_PLAY, offsetof(struct MidiPlayer_class, mPlay)},
    {&SL_IID_VOLUME, offsetof(struct MidiPlayer_class, mVolume)},
    {&SL_IID_SEEK, offsetof(struct MidiPlayer_class, mSeek)}
};

static struct class MidiPlayer_class = {
    MidiPlayer_interfaces,
    sizeof(MidiPlayer_interfaces)/sizeof(MidiPlayer_interfaces[0]),
    "MidiPlayer",
    sizeof(struct MidiPlayer_class),
    0
};

/* Object implementation (generic) */

static SLresult Object_Realize(SLObjectItf self, SLboolean async)
{
    struct Object_interface *this = (struct Object_interface *) self;
    // FIXME locking needed here in case two threads call Realize at once
    if (SL_OBJECT_STATE_UNREALIZED != this->mState)
        return SL_RESULT_PRECONDITIONS_VIOLATED;
    SLresult result = SL_RESULT_SUCCESS;
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
    unsigned i;
    if (NULL == pInterface)
        return SL_RESULT_PARAMETER_INVALID;
    struct Object_interface *this = (struct Object_interface *) self;
    struct class *class = this->mClass;
    // FIXME O(n) - could be O(1) by hashing etc.
    for (i = 0; i < class->interfaceCount; ++i) {
        if (iid == *class->interfaces[i].iid ||
            !memcmp(iid, *class->interfaces[i].iid,
            sizeof(struct SLInterfaceID_))) {
// FIXME code review on 2010/04/16
// I think it is "this->this" instead of "this" at line 296 :
// *(void **)pInterface = (char *) this + class->interfaces[i].offset;
            *(void **)pInterface = (char *) this + class->interfaces[i].offset;
            return SL_RESULT_SUCCESS;
        }
    }
    return SL_RESULT_FEATURE_UNSUPPORTED;
}

static SLresult Object_RegisterCallback(SLObjectItf self,
    slObjectCallback callback, void *pContext)
{
    struct Object_interface *this = (struct Object_interface *) self;
    // FIXME thread safety
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
    this->mState = SL_OBJECT_STATE_UNREALIZED;
    free(this);
}

static SLresult Object_SetPriority(SLObjectItf self, SLint32 priority,
    SLboolean preemptable)
{
    return SL_RESULT_SUCCESS;
}

static SLresult Object_GetPriority(SLObjectItf self, SLint32 *pPriority,
    SLboolean *pPreemptable)
{
    return SL_RESULT_SUCCESS;
}

static SLresult Object_SetLossOfControlInterfaces(SLObjectItf self,
    SLint16 numInterfaces, SLInterfaceID *pInterfaceIDs, SLboolean enabled)
{
    return SL_RESULT_SUCCESS;
}

static const struct SLObjectItf_ Object_ObjectItf = {
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

/* AudioPlayer's implementation of Object interface */

#ifdef USE_SNDFILE

// FIXME should run this asynchronously esp. for socket fd, not on mix thread
static void SLAPIENTRY SndFile_Callback(SLBufferQueueItf caller, void *pContext)
{
    struct SndFile *this = (struct SndFile *) pContext;
    SLresult result;
    if (NULL != this->mRetryBuffer && 0 < this->mRetrySize) {
        result = (*caller)->Enqueue(caller, this->mRetryBuffer, this->mRetrySize);
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

#endif

static SLresult AudioPlayer_Realize(SLObjectItf self, SLboolean async)
{
    struct AudioPlayer_class *this = (struct AudioPlayer_class *) self;
    // FIXME locking needed here in case two threads call Realize at once
    if (SL_OBJECT_STATE_UNREALIZED != this->mObject.mState)
        return SL_RESULT_PRECONDITIONS_VIOLATED;
    SLresult result = SL_RESULT_SUCCESS;
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
            SLBufferQueueItf bufferQueue = &this->mBufferQueue.itf;
            // FIXME should use a private internal API, and disallow
            // application to have access to our buffer queue
            // FIXME if we had an internal API, could call this directly
            result = (*bufferQueue)->RegisterCallback(bufferQueue, SndFile_Callback, &this->mSndFile);
        }
    }
#endif
    if (SL_RESULT_SUCCESS == result)
        this->mObject.mState = SL_OBJECT_STATE_REALIZED;
    if (async && NULL != this->mObject.mCallback)
        (*this->mObject.mCallback)(self, this->mObject.mContext,
            SL_OBJECT_EVENT_ASYNC_TERMINATION, result, this->mObject.mState,
            NULL);
    return result;
}

static void AudioPlayer_Destroy(SLObjectItf self)
{
    Object_AbortAsyncOperation(self);
    struct AudioPlayer_class *this = (struct AudioPlayer_class *) self;
    // FIXME stop the player in a way that app can't restart it
    if (NULL != this->mBufferQueue.mArray)
        free(this->mBufferQueue.mArray);
#ifdef USE_SNDFILE
    if (NULL != this->mSndFile.mSNDFILE) {
        sf_close(this->mSndFile.mSNDFILE);
        this->mSndFile.mSNDFILE = NULL;
    }
#endif
    this->mObject.mState = SL_OBJECT_STATE_UNREALIZED;
    free(this);
}

static const struct SLObjectItf_ AudioPlayer_ObjectItf = {
    AudioPlayer_Realize,
    Object_Resume,
    Object_GetState,
    Object_GetInterface,
    Object_RegisterCallback,
    Object_AbortAsyncOperation,
    AudioPlayer_Destroy,
    Object_SetPriority,
    Object_GetPriority,
    Object_SetLossOfControlInterfaces,
};

/* DynamicInterfaceManagement implementation */

static SLresult DynamicInterfaceManagement_AddInterface(
    SLDynamicInterfaceManagementItf self,
    const SLInterfaceID iid, SLboolean async)
{
    return SL_RESULT_SUCCESS;
}

static SLresult DynamicInterfaceManagement_RemoveInterface(
    SLDynamicInterfaceManagementItf self,
    const SLInterfaceID iid)
{
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
    return SL_RESULT_SUCCESS;
}

static const struct SLDynamicInterfaceManagementItf_
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
    // FIXME conver sample units to time units
    // SL_TIME_UNKNOWN == this->mPlay.mPosition = SL_TIME_UNKNOWN;
    return SL_RESULT_SUCCESS;
}

static SLresult Play_RegisterCallback(SLPlayItf self, slPlayCallback callback,
    void *pContext)
{
    return SL_RESULT_SUCCESS;
}

static SLresult Play_SetCallbackEventsMask(SLPlayItf self, SLuint32 eventFlags)
{
    return SL_RESULT_SUCCESS;
}

static SLresult Play_GetCallbackEventsMask(SLPlayItf self,
    SLuint32 *pEventFlags)
{
    return SL_RESULT_SUCCESS;
}

static SLresult Play_SetMarkerPosition(SLPlayItf self, SLmillisecond mSec)
{
    return SL_RESULT_SUCCESS;
}

static SLresult Play_ClearMarkerPosition(SLPlayItf self)
{
    return SL_RESULT_SUCCESS;
}

static SLresult Play_GetMarkerPosition(SLPlayItf self, SLmillisecond *pMsec)
{
    return SL_RESULT_SUCCESS;
}

static SLresult Play_SetPositionUpdatePeriod(SLPlayItf self, SLmillisecond mSec)
{
    return SL_RESULT_SUCCESS;
}

static SLresult Play_GetPositionUpdatePeriod(SLPlayItf self,
    SLmillisecond *pMsec)
{
    return SL_RESULT_SUCCESS;
}

static const struct SLPlayItf_ Play_PlayItf = {
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
    // FIXME
    return SL_RESULT_SUCCESS;
}

static SLresult BufferQueue_GetState(SLBufferQueueItf self,
    SLBufferQueueState *pState)
{
    if (NULL == pState)
        return SL_RESULT_PARAMETER_INVALID;
    struct BufferQueue_interface *this = (struct BufferQueue_interface *) self;
    *pState = this->mState;
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

static const struct SLBufferQueueItf_ BufferQueue_BufferQueueItf = {
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

static const struct SLVolumeItf_ Volume_VolumeItf = {
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
    if (NULL == pDevice)
        return SL_RESULT_PARAMETER_INVALID;
    *pDevice = NULL;
    SLresult result = checkInterfaces(LEDDevice_class.interfaces,
        LEDDevice_class.interfaceCount, numInterfaces, pInterfaceIds,
        pInterfaceRequired);
    if (SL_RESULT_SUCCESS != result)
        return result;
    struct LEDDevice_class *this =
        (struct LEDDevice_class *) malloc(sizeof(struct LEDDevice_class));
    if (NULL == this)
        return SL_RESULT_MEMORY_FAILURE;
    this->mObject.itf = &Object_ObjectItf;
    this->mObject.this = this;
    this->mObject.mClass = &LEDDevice_class;
    this->mObject.mState = SL_OBJECT_STATE_UNREALIZED;
    this->mObject.mCallback = NULL;
    this->mObject.mContext = NULL;
    this->mDynamicInterfaceManagement.itf =
        &DynamicInterfaceManagement_DynamicInterfaceManagementItf;
    this->mDynamicInterfaceManagement.this = this;
    this->mLED.itf = &LEDArray_LEDArrayItf;
    this->mLED.this = this;
    // FIXME wrong, and check deviceID vs. DEVICE_ID_LED
    this->mDeviceID = deviceID;
    // return the new LED device object
    *pDevice = &this->mObject.itf;
    return SL_RESULT_SUCCESS;
}

static SLresult Engine_CreateVibraDevice(SLEngineItf self,
    SLObjectItf *pDevice, SLuint32 deviceID, SLuint32 numInterfaces,
    const SLInterfaceID *pInterfaceIds, const SLboolean *pInterfaceRequired)
{
    if (NULL == pDevice)
        return SL_RESULT_PARAMETER_INVALID;
    *pDevice = NULL;
    SLresult result = checkInterfaces(VibraDevice_class.interfaces,
        VibraDevice_class.interfaceCount, numInterfaces, pInterfaceIds,
        pInterfaceRequired);
    if (SL_RESULT_SUCCESS != result)
        return result;
    struct VibraDevice_class *this =
        (struct VibraDevice_class *) malloc(sizeof(struct VibraDevice_class));
    if (NULL == this)
        return SL_RESULT_MEMORY_FAILURE;
    this->mObject.itf = &Object_ObjectItf;
    this->mObject.this = this;
    this->mObject.mClass = &VibraDevice_class;
    this->mObject.mState = SL_OBJECT_STATE_UNREALIZED;
    this->mObject.mCallback = NULL;
    this->mObject.mContext = NULL;
    this->mDynamicInterfaceManagement.itf =
        &DynamicInterfaceManagement_DynamicInterfaceManagementItf;
    this->mDynamicInterfaceManagement.this = this;
    this->mVibra.itf = &Vibra_VibraItf;
    this->mVibra.this = this;
    // FIXME wrong, and check deviceID vs. DEVICE_ID_VIBRA
    this->mDeviceID = deviceID;
    // return the new vibra device object
    *pDevice = &this->mObject.itf;
    return SL_RESULT_SUCCESS;
}

static SLresult Engine_CreateAudioPlayer(SLEngineItf self, SLObjectItf *pPlayer,
    SLDataSource *pAudioSrc, SLDataSink *pAudioSnk, SLuint32 numInterfaces,
    const SLInterfaceID *pInterfaceIds, const SLboolean *pInterfaceRequired)
{
    if (NULL == pPlayer)
        return SL_RESULT_PARAMETER_INVALID;
    *pPlayer = NULL;
    SLresult result = checkInterfaces(AudioPlayer_class.interfaces,
        AudioPlayer_class.interfaceCount, numInterfaces, pInterfaceIds,
        pInterfaceRequired);
    if (SL_RESULT_SUCCESS != result)
        return result;
    if ((NULL == pAudioSrc) || (NULL == (SLuint32 *) pAudioSrc->pLocator) ||
        (NULL == pAudioSrc->pFormat))
        return SL_RESULT_PARAMETER_INVALID;
    SLuint32 locatorType = *(SLuint32 *)pAudioSrc->pLocator;
    SLuint32 formatType = *(SLuint32 *)pAudioSrc->pFormat;
    SLuint32 numBuffers = 0;
    SLDataFormat_PCM *df_pcm = NULL;
    struct Track *track = NULL;
#ifdef USE_SNDFILE
    SLchar *pathname = NULL;
#endif
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
#endif
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
    struct AudioPlayer_class *this =
        (struct AudioPlayer_class *) malloc(sizeof(struct AudioPlayer_class));
    if (NULL == this)
        return SL_RESULT_MEMORY_FAILURE;
    this->mObject.itf = &AudioPlayer_ObjectItf;
    this->mObject.this = this;
    this->mObject.mClass = &AudioPlayer_class;
    this->mObject.mState = SL_OBJECT_STATE_UNREALIZED;
    this->mObject.mCallback = NULL;
    this->mObject.mContext = NULL;
    this->mDynamicInterfaceManagement.itf =
        &DynamicInterfaceManagement_DynamicInterfaceManagementItf;
    this->mDynamicInterfaceManagement.this = this;
    this->mPlay.itf = &Play_PlayItf;
    this->mPlay.this = this;
    this->mPlay.mState = SL_PLAYSTATE_STOPPED;
    this->mPlay.mDuration = SL_TIME_UNKNOWN;
    this->mPlay.mPosition = (SLmillisecond) 0;
    // this->mPlay.mPositionSamples = 0;
    this->mBufferQueue.itf = &BufferQueue_BufferQueueItf;
    this->mBufferQueue.this = this;
    this->mBufferQueue.mState.count = 0;
    this->mBufferQueue.mState.playIndex = 0;
    this->mBufferQueue.mCallback = NULL;
    this->mBufferQueue.mContext = NULL;
    // FIXME numBuffers is unavailable for URL, must make a default !
    this->mBufferQueue.mNumBuffers = numBuffers;
    this->mBufferQueue.mArray =
        malloc((numBuffers + 1) * sizeof(struct BufferHeader));
    // assert(this->mBufferQueue.mArray != NULL);
    this->mBufferQueue.mFront = this->mBufferQueue.mArray;
    this->mBufferQueue.mRear = this->mBufferQueue.mArray;
    this->mVolume.itf = &Volume_VolumeItf;
    this->mVolume.this = this;
    this->mVolume.mLevel = 0; // FIXME correct ?
    this->mVolume.mMute = SL_BOOLEAN_FALSE;
    this->mVolume.mEnableStereoPosition = SL_BOOLEAN_FALSE;
    this->mVolume.mStereoPosition = 0;
    this->mSeek.itf = &Seek_SeekItf;
    this->mSeek.this = this;
    this->mSeek.mPos = (SLmillisecond) -1;
    this->mSeek.mLoopEnabled = SL_BOOLEAN_FALSE;
    this->mSeek.mStartPos = (SLmillisecond) -1;
    this->mSeek.mEndPos = (SLmillisecond) -1;
#ifdef USE_SNDFILE
    this->mSndFile.mPathname = pathname;
    this->mSndFile.mSNDFILE = NULL;
    this->mSndFile.mIs0 = SL_BOOLEAN_TRUE;
    this->mSndFile.mRetryBuffer = NULL;
    this->mSndFile.mRetrySize = 0;
#endif
    // link track to player
    track->mDfPcm = df_pcm;
    track->mBufferQueue = &this->mBufferQueue;
    track->mReader = NULL;
    track->mAvail = 0;
    track->mPlay = &this->mPlay;
    // return the new audio player object
    *pPlayer = &this->mObject.itf;
    return SL_RESULT_SUCCESS;
}

static SLresult Engine_CreateAudioRecorder(SLEngineItf self,
    SLObjectItf *pRecorder, SLDataSource *pAudioSrc, SLDataSink *pAudioSnk,
    SLuint32 numInterfaces, const SLInterfaceID *pInterfaceIds,
    const SLboolean *pInterfaceRequired)
{
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
    SLresult result = checkInterfaces(MidiPlayer_class.interfaces,
        MidiPlayer_class.interfaceCount, numInterfaces, pInterfaceIds,
        pInterfaceRequired);
    if (SL_RESULT_SUCCESS != result)
        return result;
    if (NULL == pMIDISrc || NULL == pAudioOutput)
        return SL_RESULT_PARAMETER_INVALID;
    struct MidiPlayer_class *this =
        (struct MidiPlayer_class *) malloc(sizeof(struct MidiPlayer_class));
    if (NULL == this)
        return SL_RESULT_MEMORY_FAILURE;
    this->mObject.itf = &Object_ObjectItf;
    this->mObject.this = this;
    this->mObject.mClass = &MidiPlayer_class;
    this->mObject.mState = SL_OBJECT_STATE_UNREALIZED;
    this->mObject.mCallback = NULL;
    this->mObject.mContext = NULL;
    this->mDynamicInterfaceManagement.itf =
        &DynamicInterfaceManagement_DynamicInterfaceManagementItf;
    this->mDynamicInterfaceManagement.this = this;
    this->mPlay.itf = &Play_PlayItf;
    this->mPlay.this = this;
    this->mPlay.mState = SL_PLAYSTATE_STOPPED;
    this->mPlay.mDuration = SL_TIME_UNKNOWN;
    this->mPlay.mPosition = 0;
    this->mVolume.itf = &Volume_VolumeItf;
    this->mVolume.this = this;
    this->mVolume.mLevel = 0; // FIXME correct ?
    this->mVolume.mMute = SL_BOOLEAN_FALSE;
    this->mVolume.mEnableStereoPosition = SL_BOOLEAN_FALSE;
    this->mVolume.mStereoPosition = 0;
    this->mSeek.itf = &Seek_SeekItf;
    this->mSeek.this = this;
    this->mSeek.mPos = (SLmillisecond) -1;
    this->mSeek.mLoopEnabled = SL_BOOLEAN_FALSE;
    this->mSeek.mStartPos = (SLmillisecond) -1;
    this->mSeek.mEndPos = (SLmillisecond) -1;
    // return the new MIDI player object
    *pPlayer = &this->mObject.itf;
    return SL_RESULT_SUCCESS;
}

static SLresult Engine_CreateListener(SLEngineItf self, SLObjectItf *pListener,
    SLuint32 numInterfaces, const SLInterfaceID *pInterfaceIds,
    const SLboolean *pInterfaceRequired)
{
    return SL_RESULT_SUCCESS;
}

static SLresult Engine_Create3DGroup(SLEngineItf self, SLObjectItf *pGroup,
    SLuint32 numInterfaces, const SLInterfaceID *pInterfaceIds,
    const SLboolean *pInterfaceRequired)
{
    return SL_RESULT_SUCCESS;
}

static SLresult Engine_CreateOutputMix(SLEngineItf self, SLObjectItf *pMix,
    SLuint32 numInterfaces, const SLInterfaceID *pInterfaceIds,
    const SLboolean *pInterfaceRequired)
{
    if (NULL == pMix)
        return SL_RESULT_PARAMETER_INVALID;
    SLresult result = checkInterfaces(OutputMix_class.interfaces,
        OutputMix_class.interfaceCount, numInterfaces, pInterfaceIds,
        pInterfaceRequired);
    if (SL_RESULT_SUCCESS != result)
        return result;
    struct OutputMix_class *this =
        (struct OutputMix_class *) malloc(sizeof(struct OutputMix_class));
    if (NULL == this) {
        *pMix = NULL;
        return SL_RESULT_MEMORY_FAILURE;
    }
    this->mObject.itf = &Object_ObjectItf;
    this->mObject.this = this;
    this->mObject.mClass = &OutputMix_class;
    this->mObject.mState = SL_OBJECT_STATE_UNREALIZED;
    this->mObject.mCallback = NULL;
    this->mObject.mContext = NULL;
    this->mDynamicInterfaceManagement.itf =
        &DynamicInterfaceManagement_DynamicInterfaceManagementItf;
    this->mDynamicInterfaceManagement.this = this;
    this->mOutputMix.itf = &OutputMix_OutputMixItf;
    this->mOutputMix.this = this;
    this->mOutputMix.mActiveMask = 0;
    struct Track *track = &this->mOutputMix.mTracks[0];
    // FIXME O(n)
    unsigned i;
    for (i = 0; i < 32; ++i, ++track)
        track->mPlay = NULL;
    this->mVolume.itf = &Volume_VolumeItf;
    this->mVolume.this = this;
    *pMix = &this->mObject.itf;
    return SL_RESULT_SUCCESS;
}

static SLresult Engine_CreateMetadataExtractor(SLEngineItf self,
    SLObjectItf *pMetadataExtractor, SLDataSource *pDataSource,
    SLuint32 numInterfaces, const SLInterfaceID *pInterfaceIds,
    const SLboolean *pInterfaceRequired)
{
    return SL_RESULT_SUCCESS;
}

static SLresult Engine_CreateExtensionObject(SLEngineItf self,
    SLObjectItf *pObject, void *pParameters, SLuint32 objectID,
    SLuint32 numInterfaces, const SLInterfaceID *pInterfaceIds,
    const SLboolean *pInterfaceRequired)
{
    return SL_RESULT_SUCCESS;
}

static SLresult Engine_QueryNumSupportedInterfaces(SLEngineItf self,
    SLuint32 objectID, SLuint32 *pNumSupportedInterfaces)
{
    return SL_RESULT_SUCCESS;
}

static  SLresult Engine_QuerySupportedInterfaces(SLEngineItf self,
    SLuint32 objectID, SLuint32 index, SLInterfaceID *pInterfaceId)
{
    // FIXME
    return SL_RESULT_SUCCESS;
}

static SLresult Engine_QueryNumSupportedExtensions(SLEngineItf self,
    SLuint32 *pNumExtensions)
{
    // FIXME
    return SL_RESULT_SUCCESS;
}

static SLresult Engine_QuerySupportedExtension(SLEngineItf self,
    SLuint32 index, SLchar *pExtensionName, SLint16 *pNameLength)
{
    // FIXME
    return SL_RESULT_SUCCESS;
}

static SLresult Engine_IsExtensionSupported(SLEngineItf self,
    const SLchar *pExtensionName, SLboolean *pSupported)
{
    // FIXME
    return SL_RESULT_SUCCESS;
}

static const struct SLEngineItf_ Engine_EngineItf = {
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

static SLresult AudioIODeviceCapabilities_RegisterDefaultDeviceIDMapChangedCallback(
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

static const struct SLAudioIODeviceCapabilitiesItf_
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

static const struct SLOutputMixItf_ OutputMix_OutputMixItf = {
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

static const struct SLSeekItf_ Seek_SeekItf = {
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
    return SL_RESULT_SUCCESS;
}

static const struct SL3DCommitItf_ _3DCommit_3DCommitItf = {
    _3DCommit_Commit,
    _3DCommit_SetDeferred
};

/* 3DDoppler implementation */

static SLresult _3DDopplerSetVelocityCartesian(SL3DDopplerItf self,
    const SLVec3D *pVelocity)
{
    return SL_RESULT_SUCCESS;
}

static SLresult _3DDopplerSetVelocitySpherical(SL3DDopplerItf self,
    SLmillidegree azimuth, SLmillidegree elevation, SLmillimeter speed)
{
    return SL_RESULT_SUCCESS;
}

static SLresult _3DDopplerGetVelocityCartesian(SL3DDopplerItf self,
    SLVec3D *pVelocity)
{
    return SL_RESULT_SUCCESS;
}

static SLresult _3DDopplerSetDopplerFactor(SL3DDopplerItf self,
    SLpermille dopplerFactor)
{
    return SL_RESULT_SUCCESS;
}

static SLresult _3DDopplerGetDopplerFactor(SL3DDopplerItf self,
    SLpermille *pDopplerF)
{
    return SL_RESULT_SUCCESS;
}

static const struct SL3DDopplerItf_ _3DDoppler_3DDopplerItf = {
    _3DDopplerSetVelocityCartesian,
    _3DDopplerSetVelocitySpherical,
    _3DDopplerGetVelocityCartesian,
    _3DDopplerSetDopplerFactor,
    _3DDopplerGetDopplerFactor
};

/* 3DLocation implementation */

static SLresult _3DLocation_SetLocationCartesian(SL3DLocationItf self,
    const SLVec3D *pLocation)
{
    return SL_RESULT_SUCCESS;
}

static SLresult _3DLocation_SetLocationSpherical(SL3DLocationItf self,
    SLmillidegree azimuth, SLmillidegree elevation, SLmillimeter distance)
{
    return SL_RESULT_SUCCESS;
}

static SLresult _3DLocation_Move(SL3DLocationItf self, const SLVec3D *pMovement)
{
    return SL_RESULT_SUCCESS;
}

static SLresult _3DLocation_GetLocationCartesian(SL3DLocationItf self,
    SLVec3D *pLocation)
{
    return SL_RESULT_SUCCESS;
}

static SLresult _3DLocation_SetOrientationVectors(SL3DLocationItf self,
    const SLVec3D *pFront, const SLVec3D *pAbove)
{
    return SL_RESULT_SUCCESS;
}

static SLresult _3DLocation_SetOrientationAngles(SL3DLocationItf self,
    SLmillidegree heading, SLmillidegree pitch, SLmillidegree roll)
{
    return SL_RESULT_SUCCESS;
}

static SLresult _3DLocation_Rotate(SL3DLocationItf self, SLmillidegree theta,
    const SLVec3D *pAxis)
{
    return SL_RESULT_SUCCESS;
}

static SLresult _3DLocation_GetOrientationVectors(SL3DLocationItf self,
    SLVec3D *pFront, SLVec3D *pUp)
{
    return SL_RESULT_SUCCESS;
}

static const struct SL3DLocationItf_ _3DLocation_3DLocationItf = {
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
#define ENTRY(x) {SL_AUDIOCODEC_##x, sizeof(Caps_##x)/sizeof(Caps_##x[0]), Caps_##x}
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

static const struct SLAudioDecoderCapabilitiesItf_
    AudioDecoderCapabilities_AudioDecoderCapabilitiesItf = {
    AudioDecoderCapabilities_GetAudioDecoders,
    AudioDecoderCapabilities_GetAudioDecoderCapabilities
};

/* AudioEncoder implementation */

static SLresult AudioEncoder_SetEncoderSettings(SLAudioEncoderItf self,
    SLAudioEncoderSettings  *pSettings)
{
    return SL_RESULT_SUCCESS;
}

static SLresult AudioEncoder_GetEncoderSettings(SLAudioEncoderItf self,
    SLAudioEncoderSettings *pSettings)
{
    return SL_RESULT_SUCCESS;
}

static const struct SLAudioEncoderItf_ AudioEncoder_AudioEncoderItf = {
    AudioEncoder_SetEncoderSettings,
    AudioEncoder_GetEncoderSettings
};

/* AudioEncoderCapabilities implementation */

static SLresult AudioEncoderCapabilities_GetAudioEncoders(
    SLAudioEncoderCapabilitiesItf self, SLuint32 *pNumEncoders, SLuint32 *pEncoderIds)
{
    return SL_RESULT_SUCCESS;
}

static SLresult AudioEncoderCapabilities_GetAudioEncoderCapabilities(
    SLAudioEncoderCapabilitiesItf self, SLuint32 encoderId, SLuint32 *pIndex,
    SLAudioCodecDescriptor *pDescriptor)
{
    return SL_RESULT_SUCCESS;
}

static const struct SLAudioEncoderCapabilitiesItf_
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
    return SL_RESULT_SUCCESS;
}

static SLresult DeviceVolume_GetVolume(SLDeviceVolumeItf self,
    SLuint32 deviceID, SLint32 *pVolume)
{
    return SL_RESULT_SUCCESS;
}

static const struct SLDeviceVolumeItf_ DeviceVolume_DeviceVolumeItf = {
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

static const struct SLDynamicSourceItf_ DynamicSource_DynamicSourceItf = {
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

static const struct SLEffectSendItf_ EffectSend_EffectSendItf = {
    EffectSend_EnableEffectSend,
    EffectSend_IsEnabled,
    EffectSend_SetDirectLevel,
    EffectSend_GetDirectLevel,
    EffectSend_SetSendLevel,
    EffectSend_GetSendLevel
};

/* EngineCapabilities implementation */

// FIXME for this and all others in this category

static SLresult EngineCapabilities_QuerySupportedProfiles(
    SLEngineCapabilitiesItf self, SLuint16 *pProfilesSupported)
{
    if (NULL == pProfilesSupported)
        return SL_RESULT_PARAMETER_INVALID;
    // FIXME This is pessimistic as it omits the unofficial driver profile
    *pProfilesSupported = 0; // SL_PROFILES_PHONE | SL_PROFILES_MUSIC | SL_PROFILES_GAME
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
    // FIXME
    return SL_RESULT_SUCCESS;
}

static SLresult EngineCapabilities_QueryVibraCapabilities(
    SLEngineCapabilitiesItf self, SLuint32 *pIndex, SLuint32 *pVibraDeviceID,
    SLVibraDescriptor *pDescriptor)
{
    // FIXME
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

static const struct SLEngineCapabilitiesItf_
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

static SLresult LEDArray_ActivateLEDArray(SLLEDArrayItf self, SLuint32 lightMask)
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

static const struct SLLEDArrayItf_ LEDArray_LEDArrayItf = {
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

static const struct SLMetadataExtractionItf_ MetadataExtraction_MetadataExtractionItf = {
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

static SLresult MetadataTraversal_GetChildMIMETypeSize(SLMetadataTraversalItf self,
    SLuint32 index, SLuint32 *pSize)
{
    return SL_RESULT_SUCCESS;
}

static SLresult MetadataTraversal_GetChildInfo(SLMetadataTraversalItf self,
    SLuint32 index, SLint32 *pNodeID, SLuint32 *pType, SLuint32 size, SLchar *pMimeType)
{
    return SL_RESULT_SUCCESS;
}

static SLresult MetadataTraversal_SetActiveNode(SLMetadataTraversalItf self,
    SLuint32 index)
{
    return SL_RESULT_SUCCESS;
}

static const struct SLMetadataTraversalItf_ MetadataTraversal_MetadataTraversalItf = {
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

static const struct SLMuteSoloItf_ MuteSolo_MuteSoloItf = {
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

static const struct SLPitchItf_ Pitch_PitchItf = {
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

static const struct SLPlaybackRateItf_ PlaybackRate_PlaybackRateItf = {
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

static const struct SLPrefetchStatusItf_ PrefetchStatus_PrefetchStatusItf = {
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

static const struct SLRatePitchItf_ RatePitch_RatePitchItf = {
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

static SLresult Record_SetPositionUpdatePeriod(SLRecordItf self, SLmillisecond mSec)
{
    return SL_RESULT_SUCCESS;
}

static SLresult Record_GetPositionUpdatePeriod(SLRecordItf self, SLmillisecond *pMsec)
{
    return SL_RESULT_SUCCESS;
}

static const struct SLRecordItf_ Record_RecordItf = {
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

static const struct SLThreadSyncItf_ ThreadSync_ThreadSyncItf = {
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

static const struct SLVibraItf_ Vibra_VibraItf = {
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
    return SL_RESULT_SUCCESS;
}

static SLresult Visualization_GetMaxRate(SLVisualizationItf self, SLmilliHertz* pRate)
{
    return SL_RESULT_SUCCESS;
}

static const struct SLVisualizationItf_ Visualization_VisualizationItf = {
    Visualization_RegisterVisualizationCallback,
    Visualization_GetMaxRate
};

/* Initial entry points */

SLresult SLAPIENTRY slCreateEngine(SLObjectItf *pEngine, SLuint32 numOptions,
    const SLEngineOption *pEngineOptions, SLuint32 numInterfaces,
    const SLInterfaceID *pInterfaceIds, const SLboolean *pInterfaceRequired)
{
    if (NULL == pEngine)
        return SL_RESULT_PARAMETER_INVALID;
    // FIXME why disallow a non-null pointer if num is 0
    if ((0 < numOptions) != (NULL != pEngineOptions))
        return SL_RESULT_PARAMETER_INVALID;
    SLresult result = checkInterfaces(Engine_class.interfaces,
        Engine_class.interfaceCount, numInterfaces, pInterfaceIds,
        pInterfaceRequired);
    if (SL_RESULT_SUCCESS != result)
        return result;
    struct Engine_class *this =
        (struct Engine_class *) malloc(sizeof(struct Engine_class));
    if (NULL == this) {
        *pEngine = NULL;
        return SL_RESULT_MEMORY_FAILURE;
    }
    this->mObject.itf = &Object_ObjectItf;
    this->mObject.this = this;
    this->mObject.mClass = &Engine_class;
    this->mObject.mState = SL_OBJECT_STATE_UNREALIZED;
    this->mObject.mCallback = NULL;
    this->mObject.mContext = NULL;
    this->mDynamicInterfaceManagement.itf =
        &DynamicInterfaceManagement_DynamicInterfaceManagementItf;
    this->mDynamicInterfaceManagement.this = this;
    this->mEngine.itf = &Engine_EngineItf;
    this->mEngine.this = this;
    this->mAudioIODeviceCapabilities.itf =
        &AudioIODeviceCapabilities_AudioIODeviceCapabilitiesItf;
    this->mAudioIODeviceCapabilities.this = this;
    *pEngine = &this->mObject.itf;
    return SL_RESULT_SUCCESS;
}

SLresult SLAPIENTRY slQueryNumSupportedEngineInterfaces(
    SLuint32 *pNumSupportedInterfaces)
{
    if (NULL != pNumSupportedInterfaces) {
        *pNumSupportedInterfaces = Engine_class.interfaceCount;
        return SL_RESULT_SUCCESS;
    } else
        return SL_RESULT_PARAMETER_INVALID;
}

SLresult SLAPIENTRY slQuerySupportedEngineInterfaces(SLuint32 index,
    SLInterfaceID *pInterfaceId)
{
    if ((index < sizeof(Engine_interfaces)/sizeof(Engine_interfaces[0])) &&
        (NULL != pInterfaceId)) {
        *pInterfaceId = *Engine_interfaces[index].iid;
        return SL_RESULT_SUCCESS;
    } else
        return SL_RESULT_PARAMETER_INVALID;
}

// Used by SDL but not specific to or dependent on SDL

static void OutputMix_FillBuffer(SLOutputMixItf self, void *pBuffer, SLuint32 size)
{
    // Force to be a multiple of a frame, assumes stereo 16-bit PCM
    size &= ~3;
    struct OutputMix_interface *this = (struct OutputMix_interface *) self;
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
                    typedef struct {
                        short left;
                        short right;
                    } stereo;
                    stereo *mixBuffer = dstWriter;
                    const stereo *source = track->mReader;
                    unsigned j;
                    for (j = 0; j < actual; j += sizeof(stereo), ++mixBuffer, ++source) {
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
                        if (++newFront == &bufferQueue->mArray[bufferQueue->mNumBuffers])
                            newFront = bufferQueue->mArray;
                        bufferQueue->mFront = (struct BufferHeader *) newFront;
                        assert(0 < bufferQueue->mState.count);
                        --bufferQueue->mState.count;
                        // FIXME here or in Enqueue?
                        ++bufferQueue->mState.playIndex;
                        // FIXME this might be a good time to do an early warning
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
                // We need data but none available, attempting a desperate callback
                slBufferQueueCallback callback = bufferQueue->mCallback;
                if (NULL != callback) {
                    (*callback)((SLBufferQueueItf) bufferQueue, bufferQueue->mContext);
                    // if we are lucky, maybe the callback enqueued a buffer
                    // we got lucky, now the queue is not empty, the callback worked
                    if (rear != bufferQueue->mRear)
                        goto got_one;
                    // callback was unsuccessful, underflow seems our only recourse
                }
                // here on underflow due to no callback, or failed callback
                // FIXME underflow, send silence (or previous buffer?)
                // we did a callback to try to kick start again but failed
                // should log this
            }
            // here if no buffer queue or underflow, clear out rest of partial buffer
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

#ifdef USE_SDL

/* SDL platform implementation */

static void SDLCALL SDL_callback(void *context, Uint8 *stream, int len)
{
    assert(len > 0);
    OutputMix_FillBuffer((SLOutputMixItf) context, stream, (SLuint32) len);
}

void SDL_start(SLObjectItf self)
{
    assert(self != NULL);
    // FIXME make this an operation on Object: GetClass
    assert(&OutputMix_class == ((struct Object_interface *) self)->mClass);
    struct OutputMix_interface *om = &((struct OutputMix_class *) self)->mOutputMix;

    SDL_AudioSpec fmt;
    fmt.freq = 44100;
    fmt.format = AUDIO_S16;
    fmt.channels = 2;
    fmt.samples = 256;
    fmt.callback = SDL_callback;
    fmt.userdata = om;

    if (SDL_OpenAudio(&fmt, NULL) < 0) {
        fprintf(stderr, "Unable to open audio: %s\n", SDL_GetError());
        exit(1);
    }
    SDL_PauseAudio(0);
}

#endif

/* End */
