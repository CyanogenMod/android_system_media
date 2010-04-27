/* Copyright 2010 The Android Open Source Project */

/* OpenSL ES prototype */

#include "OpenSLES.h"
#include <stddef.h> // offsetof
#include <stdlib.h> // malloc
#include <string.h> // memcmp
#include <stdio.h> // debugging

/* Forward declarations */

static const struct SLObjectItf_ Object_ObjectItf;
static const struct SLDynamicInterfaceManagementItf_
    DynamicInterfaceManagement_DynamicInterfaceManagementItf;
static const struct SLEngineItf_ Engine_EngineItf;
static const struct SLOutputMixItf_ OutputMix_OutputMixItf;
static const struct SLPlayItf_ Play_PlayItf;
static const struct SLVolumeItf_ Volume_VolumeItf;
static const struct SLBufferQueueItf_ BufferQueue_BufferQueueItf;

/* Private types */

struct iid_vtable {
    const SLInterfaceID *iid;
    size_t offset;
};

/* Per-class data shared by all instances of the same class */

struct class {
    // needed by all classes (class class, the superclass of all classes)
    const struct iid_vtable *interfaces;
    const SLuint32 interfaceCount;
    const char * const name;
    const size_t size;
    // non-const here and below
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
};

struct Play_interface {
    const struct SLPlayItf_ *itf;
    void *this;
    // additional fields
    volatile SLuint32 mState;
};

struct BufferQueue_interface {
    const struct SLBufferQueueItf_ *itf;
    void *this;
    // additional fields
    volatile SLBufferQueueState mState;
    slBufferQueueCallback mCallback;
    void *mContext;
    SLuint32 mNumBuffers;
    struct BufferHeader {
        // circular
        // struct Buffer *mNext;
        const void *mBuffer;
        SLuint32 mSize;
    } *mArray, *mFront, *mRear;
};

#if 0 // not yet needed

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

#endif

struct Volume_interface {
    const struct SLVolumeItf_ *itf;
    void *this;
    // additional fields
    SLmillibel mLevel;
};

/* Classes */

struct Engine_class {
    struct Object_interface mObject;
    struct DynamicInterfaceManagement_interface mDynamicInterfaceManagement;
    struct Engine_interface mEngine;
#if 0
// more
#endif
    // additional fields not associated with interfaces goes here
};

struct OutputMix_class {
    struct Object_interface mObject;
    struct DynamicInterfaceManagement_interface mDynamicInterfaceManagement;
    struct OutputMix_interface mOutputMix;
#if 0
    struct EnvironmentalReverb_interface mEnvironmentalReverb;
    struct Equalizer_interface mEqualizer;
    struct PresetReverb_interface mPresetReverb;
    struct Virtualizer_interface mVirtualizer;
#endif
    struct Volume_interface mVolume;
};

struct AudioPlayer_class {
    struct Object_interface mObject;
    struct DynamicInterfaceManagement_interface mDynamicInterfaceManagement;
    struct Play_interface mPlay;
    struct BufferQueue_interface mBufferQueue;
    struct Volume_interface mVolume;
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
                !memcmp(pInterfaceIds[i], *interfaces[j].iid, sizeof(struct SLInterfaceID_)))
                    goto found;
            }
            if (SL_BOOLEAN_FALSE != pInterfaceRequired[i])
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
    {&SL_IID_DYNAMICINTERFACEMANAGEMENT, offsetof(struct Engine_class, mDynamicInterfaceManagement)},
    {&SL_IID_ENGINE, offsetof(struct Engine_class, mEngine)}
#if 0
    {&SL_IID_ENGINECAPABILITIES, NULL},
    {&SL_IID_THREADSYNC, NULL},
    {&SL_IID_AUDIOIODEVICECAPABILITIES, NULL},
    {&SL_IID_AUDIODECODERCAPABILITIES, NULL},
    {&SL_IID_AUDIOENCODERCAPABILITIES, NULL},
    {&SL_IID_3DCOMMIT, NULL}
    // SL_IID_DEVICEVOLUME
#endif
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
    {&SL_IID_DYNAMICINTERFACEMANAGEMENT, offsetof(struct OutputMix_class, mDynamicInterfaceManagement)},
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
    {&SL_IID_DYNAMICINTERFACEMANAGEMENT, offsetof(struct AudioPlayer_class, mDynamicInterfaceManagement)},
    {&SL_IID_PLAY, offsetof(struct AudioPlayer_class, mPlay)},
    {&SL_IID_BUFFERQUEUE, offsetof(struct AudioPlayer_class, mBufferQueue)},
    {&SL_IID_VOLUME, offsetof(struct AudioPlayer_class, mVolume)}
};

static struct class AudioPlayer_class = {
    AudioPlayer_interfaces,
    sizeof(AudioPlayer_interfaces)/sizeof(AudioPlayer_interfaces[0]),
    "AudioPlayer",
    sizeof(struct AudioPlayer_class),
    0
};

/* Object implementation */

static SLresult Object_Realize(SLObjectItf self, SLboolean async)
{
    struct Object_interface *this = (struct Object_interface *) self;
    // FIXME locking needed here in case two threads call Realize at once
    if (this->mState != SL_OBJECT_STATE_UNREALIZED)
        return SL_RESULT_PRECONDITIONS_VIOLATED;
    this->mState = SL_OBJECT_STATE_REALIZED;
#if 0 // FIXME
    void (*callback)(void) = NULL;
    if (SL_BOOLEAN_FALSE != async)
        callback();
#endif
    return SL_RESULT_SUCCESS;
}

static SLresult Object_Resume(SLObjectItf self, SLboolean async)
{
#if 0 // FIXME
    void (*callback)(void) = NULL;
    if (SL_BOOLEAN_FALSE != async)
        callback();
#endif
    return SL_RESULT_SUCCESS;
}

static SLresult Object_GetState(SLObjectItf self, SLuint32 * pState)
{
    if (NULL == pState)
        return SL_RESULT_PARAMETER_INVALID;
    struct Object_interface *this = (struct Object_interface *) self;
    *pState = this->mState;
    return SL_RESULT_SUCCESS;
}

static SLresult Object_GetInterface(SLObjectItf self, const SLInterfaceID iid,
    void * pInterface)
{
    unsigned i;
    if (NULL == pInterface)
        return SL_RESULT_PARAMETER_INVALID;
    struct Object_interface *this = (struct Object_interface *) self;
    struct class *class = this->mClass;
    // FIXME O(n) - could be O(1) by hashing etc.
    for (i = 0; i < class->interfaceCount; ++i) {
        if (iid == *class->interfaces[i].iid ||
            !memcmp(iid, *class->interfaces[i].iid, sizeof(struct SLInterfaceID_))) {
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
    slObjectCallback callback, void * pContext)
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

static SLresult Play_GetDuration(SLPlayItf self,  SLmillisecond *pMsec)
{
    return SL_RESULT_SUCCESS;
}

static SLresult Play_GetPosition(SLPlayItf self,  SLmillisecond *pMsec)
{
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

static SLresult Play_GetCallbackEventsMask(SLPlayItf self, SLuint32 *pEventFlags)
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

static SLresult Play_GetPositionUpdatePeriod(SLPlayItf self, SLmillisecond *pMsec)
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
    // circular
    struct BufferHeader *oldRear = this->mRear;
    struct BufferHeader *newRear = oldRear;
    if (++newRear == &this->mArray[this->mNumBuffers])
        newRear = this->mArray;
    if (newRear == this->mFront)
        return SL_RESULT_BUFFER_INSUFFICIENT;
    oldRear->mBuffer = pBuffer;
    oldRear->mSize = size;
    this->mRear = newRear;
    return SL_RESULT_SUCCESS;
}

static SLresult BufferQueue_Clear(SLBufferQueueItf self)
{
    return SL_RESULT_SUCCESS;
}

static SLresult BufferQueue_GetState(SLBufferQueueItf self, SLBufferQueueState *pState)
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
    if (!(SL_MILLIBEL_MIN <= level && SL_MILLIBEL_MAX >= level))
        return SL_RESULT_PARAMETER_INVALID;
    struct Volume_interface *this = (struct Volume_interface *) self;
    this->mLevel = level;
    return SL_RESULT_SUCCESS;
}

static SLresult Volume_GetVolumeLevel(SLVolumeItf self, SLmillibel *pLevel)
{
    return SL_RESULT_SUCCESS;
}

static SLresult Volume_GetMaxVolumeLevel(SLVolumeItf self, SLmillibel *pMaxLevel)
{
    return SL_RESULT_SUCCESS;
}

static SLresult Volume_SetMute(SLVolumeItf self, SLboolean mute)
{
    return SL_RESULT_SUCCESS;
}

static SLresult Volume_GetMute(SLVolumeItf self, SLboolean *pMute)
{
    return SL_RESULT_SUCCESS;
}

static SLresult Volume_EnableStereoPosition(SLVolumeItf self, SLboolean enable)
{
    return SL_RESULT_SUCCESS;
}

static SLresult Volume_IsEnabledStereoPosition(SLVolumeItf self,
    SLboolean *pEnable)
{
    return SL_RESULT_SUCCESS;
}

static SLresult Volume_SetStereoPosition(SLVolumeItf self,
    SLpermille stereoPosition)
{
    return SL_RESULT_SUCCESS;
}

static SLresult Volume_GetStereoPosition(SLVolumeItf self,
    SLpermille *pStereoPosition)
{
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
    const SLInterfaceID * pInterfaceIds, const SLboolean *pInterfaceRequired)
{
    return SL_RESULT_SUCCESS;
}

static SLresult Engine_CreateVibraDevice(SLEngineItf self,
    SLObjectItf *pDevice, SLuint32 deviceID, SLuint32 numInterfaces,
    const SLInterfaceID *pInterfaceIds, const SLboolean *pInterfaceRequired)
{
    return SL_RESULT_SUCCESS;
}

static SLresult Engine_CreateAudioPlayer(SLEngineItf self, SLObjectItf *pPlayer,
    SLDataSource *pAudioSrc, SLDataSink *pAudioSnk, SLuint32 numInterfaces,
    const SLInterfaceID *pInterfaceIds, const SLboolean *pInterfaceRequired)
{
    if (NULL == pPlayer)
        return SL_RESULT_PARAMETER_INVALID;
    *pPlayer = NULL;
    if (NULL == pAudioSrc || (NULL == (SLuint32 *) pAudioSrc->pLocator))
        return SL_RESULT_PARAMETER_INVALID;
    SLuint32 numBuffers = 0;
    switch (*(SLuint32 *)pAudioSrc->pLocator) {
    case SL_DATALOCATOR_BUFFERQUEUE:
        {
        numBuffers = ((SLDataLocator_BufferQueue *) pAudioSrc->pLocator)->numBuffers;
        if (0 == numBuffers)
            return SL_RESULT_PARAMETER_INVALID;
        if (NULL == pAudioSrc->pFormat) 
            return SL_RESULT_PARAMETER_INVALID;
        switch (*(SLuint32 *)pAudioSrc->pFormat) {
        case SL_DATAFORMAT_PCM:
            {
            SLDataFormat_PCM *pcm = (SLDataFormat_PCM *) pAudioSrc->pFormat;
            switch (pcm->numChannels) {
            case 1:
            case 2:
                break;
            default:
                return SL_RESULT_CONTENT_UNSUPPORTED;
            }
            switch (pcm->samplesPerSec) {
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
            switch (pcm->bitsPerSample) {
            case SL_PCMSAMPLEFORMAT_FIXED_16:
                break;
            // others
            default:
                return SL_RESULT_CONTENT_UNSUPPORTED;
            }
            switch (pcm->containerSize) {
            case 16:
                break;
            // others
            default:
                return SL_RESULT_CONTENT_UNSUPPORTED;
            }
            switch (pcm->channelMask) {
            // needs work
            default:
                break;
            }
            switch (pcm->endianness) {
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
    case SL_DATALOCATOR_URI:
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
        SLObjectItf outputMix = ((SLDataLocator_OutputMix *) pAudioSnk->pLocator)->outputMix;
        // FIXME make this an operation on Object: GetClass
        if (NULL == outputMix || (&OutputMix_class != ((struct Object_interface *) outputMix)->mClass))
            return SL_RESULT_PARAMETER_INVALID;
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
    SLresult result = checkInterfaces(AudioPlayer_class.interfaces,
        AudioPlayer_class.interfaceCount, numInterfaces, pInterfaceIds,
        pInterfaceRequired);
    if (SL_RESULT_SUCCESS != result)
        return result;
    struct AudioPlayer_class *this =
        (struct AudioPlayer_class *) malloc(sizeof(struct AudioPlayer_class));
    if (NULL == this) {
        *pPlayer = NULL;
        return SL_RESULT_MEMORY_FAILURE;
    }
    this->mObject.itf = &Object_ObjectItf;
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
    this->mBufferQueue.itf = &BufferQueue_BufferQueueItf;
    this->mBufferQueue.this = this;
    this->mBufferQueue.mState.count = 0;
    this->mBufferQueue.mState.playIndex = 0;
    this->mBufferQueue.mCallback = NULL;
    this->mBufferQueue.mContext = NULL;
    this->mBufferQueue.mNumBuffers = numBuffers;
    this->mBufferQueue.mArray = malloc((numBuffers + 1) * sizeof(struct BufferHeader));
    // assert(this->mBufferQueue.mArray != NULL);
    this->mBufferQueue.mFront = this->mBufferQueue.mArray;
    this->mBufferQueue.mRear = this->mBufferQueue.mArray;
    this->mVolume.itf = &Volume_VolumeItf;
    this->mVolume.this = this;
    this->mVolume.mLevel = 0; // FIXME correct ?
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
    return SL_RESULT_SUCCESS;
}

static SLresult Engine_QueryNumSupportedExtensions(SLEngineItf self,
    SLuint32 *pNumExtensions)
{
    return SL_RESULT_SUCCESS;
}

static SLresult Engine_QuerySupportedExtension(SLEngineItf self,
    SLuint32 index, SLchar *pExtensionName, SLint16 *pNameLength)
{
    return SL_RESULT_SUCCESS;
}

static SLresult Engine_IsExtensionSupported(SLEngineItf self,
    const SLchar *pExtensionName, SLboolean *pSupported)
{
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

#if 0
/* SDL platform implementation */

#include "/Library/Frameworks/SDL.framework/Versions/A/Headers/SDL_audio.h"

static void SDL_callback(void *context, void *stream, int len)
{
    struct BufferQueue_interface *this = (struct BufferQueue_interface *) self;

    bufferQueue = (BufferQueue *) context;
    BufferHeader *old_front = bufferQueue->mFront;
    if (bufferQueue->mFront == bufferQueue->mRear) {
        // underflow, send silence (or previous buffer?)
        memset(stream, 0, len);
        // should do callback to try to kick start again
    } else {
        // should check len == oldFront->mSize
        memcpy(stream, oldFront->mBuffer, len);
    }
    if (NULL != mBufferQueue)
        ;
}

void SDL_start(SLBufferQueueItf self)
{
    struct BufferQueue_interface *this = (struct BufferQueue_interface *) self;

    SDL_AudioSpec fmt;
    fmt.freq = 44100;
    fmt.format = AUDIO_S16;
    fmt.channels = 2;
    fmt.samples = 2048; // 512;
    fmt.callback = SDL_callback;
    fmt.userdata = this;

    if (SDL_OpenAudio(&fmt, NULL) < 0) {
        fprintf(stderr, "Unable to open audio: %s\n", SDL_GetError());
        exit(1);
    }
    SDL_PauseAudio(0);
}
#endif

/* End */
