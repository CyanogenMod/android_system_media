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

/* OpenSL ES prototype */

#include "sles_allinclusive.h"
#include "sles_check_audioplayer_ext.h"

#ifdef USE_ANDROID
#include "sles_to_android_ext.h"
#endif

/* Forward declarations */

extern const struct SLInterfaceID_ SL_IID_array[MPH_MAX];

#ifdef __cplusplus
#define this this_
#endif

/* Private types */

/* Device table (change this when you port!) */

static const SLAudioInputDescriptor AudioInputDescriptor_mic = {
    (SLchar *) "mic",            // deviceName
    SL_DEVCONNECTION_INTEGRATED, // deviceConnection
    SL_DEVSCOPE_ENVIRONMENT,     // deviceScope
    SL_DEVLOCATION_HANDSET,      // deviceLocation
    SL_BOOLEAN_TRUE,             // isForTelephony
    SL_SAMPLINGRATE_44_1,        // minSampleRate
    SL_SAMPLINGRATE_44_1,        // maxSampleRate
    SL_BOOLEAN_TRUE,             // isFreqRangeContinuous
    NULL,                        // samplingRatesSupported
    0,                           // numOfSamplingRatesSupported
    1                            // maxChannels
};

static const struct AudioInput_id_descriptor {
    SLuint32 id;
    const SLAudioInputDescriptor *descriptor;
} AudioInput_id_descriptors[] = {
    {SL_DEFAULTDEVICEID_AUDIOINPUT, &AudioInputDescriptor_mic}
};

static const SLAudioOutputDescriptor AudioOutputDescriptor_speaker = {
    (SLchar *) "speaker",        // deviceName
    SL_DEVCONNECTION_INTEGRATED, // deviceConnection
    SL_DEVSCOPE_USER,            // deviceScope
    SL_DEVLOCATION_HEADSET,      // deviceLocation
    SL_BOOLEAN_TRUE,             // isForTelephony
    SL_SAMPLINGRATE_44_1,        // minSamplingRate
    SL_SAMPLINGRATE_44_1,        // maxSamplingRate
    SL_BOOLEAN_TRUE,             // isFreqRangeContinuous
    NULL,                        // samplingRatesSupported
    0,                           // numOfSamplingRatesSupported
    2                            // maxChannels
};

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

static const struct AudioOutput_id_descriptor {
    SLuint32 id;
    const SLAudioOutputDescriptor *descriptor;
} AudioOutput_id_descriptors[] = {
    {SL_DEFAULTDEVICEID_AUDIOOUTPUT, &AudioOutputDescriptor_speaker},
    {DEVICE_ID_HEADSET, &AudioOutputDescriptor_headset},
    {DEVICE_ID_HANDSFREE, &AudioOutputDescriptor_handsfree}
};

static const SLLEDDescriptor SLLEDDescriptor_default = {
    32, // ledCount
    0,  // primaryLED
    ~0  // colorMask
};

static const struct LED_id_descriptor {
    SLuint32 id;
    const SLLEDDescriptor *descriptor;
} LED_id_descriptors[] = {
    {SL_DEFAULTDEVICEID_LED, &SLLEDDescriptor_default}
};

static const SLVibraDescriptor SLVibraDescriptor_default = {
    SL_BOOLEAN_TRUE, // supportsFrequency
    SL_BOOLEAN_TRUE, // supportsIntensity
    20000,           // minFrequency
    100000           // maxFrequency
};

static const struct Vibra_id_descriptor {
    SLuint32 id;
    const SLVibraDescriptor *descriptor;
} Vibra_id_descriptors[] = {
    {SL_DEFAULTDEVICEID_VIBRA, &SLVibraDescriptor_default}
};


/* Private functions */

static void object_lock_exclusive(struct Object_interface *this)
{
    int ok;
    ok = pthread_mutex_lock(&this->mMutex);
    assert(0 == ok);
}

static void object_unlock_exclusive(struct Object_interface *this)
{
    int ok;
    ok = pthread_mutex_unlock(&this->mMutex);
    assert(0 == ok);
}

static void object_cond_wait(struct Object_interface *this)
{
    int ok;
    ok = pthread_cond_wait(&this->mCond, &this->mMutex);
    assert(0 == ok);
}

static void object_cond_signal(struct Object_interface *this)
{
    int ok;
    ok = pthread_cond_signal(&this->mCond);
    assert(0 == ok);
}

// Currently shared locks are implemented as exclusive, but don't count on it

#define object_lock_shared(this)   object_lock_exclusive(this)
#define object_unlock_shared(this) object_unlock_exclusive(this)

// Currently interface locks are actually on whole object, but don't count on it

#define interface_lock_exclusive(this)   object_lock_exclusive((this)->mThis)
#define interface_unlock_exclusive(this) object_unlock_exclusive((this)->mThis)
#define interface_lock_shared(this)      object_lock_shared((this)->mThis)
#define interface_unlock_shared(this)    object_unlock_shared((this)->mThis)
#define interface_cond_wait(this)        object_cond_wait((this)->mThis)
#define interface_cond_signal(this)      object_cond_signal((this)->mThis)

// Peek and poke are an optimization for small atomic fields that don't "matter"

#define interface_lock_poke(this)   /* interface_lock_exclusive(this) */
#define interface_unlock_poke(this) /* interface_unlock_exclusive(this) */
#define interface_lock_peek(this)   /* interface_lock_shared(this) */
#define interface_unlock_peek(this) /* interface_unlock_shared(this) */

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

// Check the interface IDs passed into a Create operation

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
            int MPH, index;
            if ((0 > (MPH = IID_to_MPH(iid))) ||
                (0 > (index = class__->mMPH_to_index[MPH]))) {
                if (pInterfaceRequired[i])
                    return SL_RESULT_FEATURE_UNSUPPORTED;
                continue;
            }
            // FIXME this seems a bit strong? what is correct logic?
            // we are requesting a duplicate explicit interface,
            // or we are requesting one which is already implicit ?
            // if (exposedMask & (1 << index))
            //    return SL_RESULT_PARAMETER_INVALID;
            exposedMask |= (1 << index);
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
    this->mGeneration = 0;
#endif
}

extern const struct SL3DDopplerItf_ _3DDoppler_3DDopplerItf;

static void _3DDoppler_init(void *self)
{
    struct _3DDoppler_interface *this = (struct _3DDoppler_interface *) self;
    this->mItf = &_3DDoppler_3DDopplerItf;
#ifndef NDEBUG
    this->mVelocityCartesian.x = 0;
    this->mVelocityCartesian.y = 0;
    this->mVelocityCartesian.z = 0;
    memset(&this->mVelocitySpherical, 0x55, sizeof(this->mVelocitySpherical));
#endif
    this->mDopplerFactor = 1000;
    this->mVelocityActive = CARTESIAN_SET_SPHERICAL_UNKNOWN;
}

extern const struct SL3DGroupingItf_ _3DGrouping_3DGroupingItf;

static void _3DGrouping_init(void *self)
{
    struct _3DGrouping_interface *this = (struct _3DGrouping_interface *) self;
    this->mItf = &_3DGrouping_3DGroupingItf;
#ifndef NDEBUG
    this->mGroup = NULL;
#endif
    // FIXME initialize the bag here
}

extern const struct SL3DLocationItf_ _3DLocation_3DLocationItf;

static void _3DLocation_init(void *self)
{
    struct _3DLocation_interface *this = (struct _3DLocation_interface *) self;
    this->mItf = &_3DLocation_3DLocationItf;
#ifndef NDEBUG
    this->mLocationCartesian.x = 0;
    this->mLocationCartesian.y = 0;
    this->mLocationCartesian.z = 0;
    memset(&this->mLocationSpherical, 0x55, sizeof(this->mLocationSpherical));
    this->mOrientationAngles.mHeading = 0;
    this->mOrientationAngles.mPitch = 0;
    this->mOrientationAngles.mRoll = 0;
    this->mOrientationVectors.mFront.x = 0;
    this->mOrientationVectors.mFront.y = 0;
    this->mOrientationVectors.mAbove.x = 0;
    this->mOrientationVectors.mAbove.y = 0;
    this->mOrientationVectors.mUp.x = 0;
    this->mOrientationVectors.mUp.z = 0;
    this->mTheta = 0x55555555;
    this->mAxis.x = 0x55555555;
    this->mAxis.y = 0x55555555;
    this->mAxis.z = 0x55555555;
    this->mRotatePending = SL_BOOLEAN_FALSE;
#endif
    this->mOrientationVectors.mFront.z = -1000;
    this->mOrientationVectors.mUp.y = 1000;
    this->mOrientationVectors.mAbove.z = -1000;
    this->mLocationActive = CARTESIAN_SET_SPHERICAL_UNKNOWN;
    this->mOrientationActive = ANGLES_SET_VECTORS_UNKNOWN;
}

extern const struct SL3DMacroscopicItf_ _3DMacroscopic_3DMacroscopicItf;

static void _3DMacroscopic_init(void *self)
{
    struct _3DMacroscopic_interface *this = (struct _3DMacroscopic_interface *) self;
    this->mItf = &_3DMacroscopic_3DMacroscopicItf;
#ifndef NDEBUG
    this->mSize.mWidth = 0;
    this->mSize.mHeight = 0;
    this->mSize.mDepth = 0;
    this->mOrientationAngles.mHeading = 0;
    this->mOrientationAngles.mPitch = 0;
    this->mOrientationAngles.mRoll = 0;
    this->mOrientationVectors.mFront.x = 0;
    this->mOrientationVectors.mFront.y = 0;
    this->mOrientationVectors.mUp.x = 0;
    this->mOrientationVectors.mUp.z = 0;
    // this->mGeneration = 0;
    this->mTheta = 0x55555555;
    this->mAxis.x = 0x55555555;
    this->mAxis.y = 0x55555555;
    this->mAxis.z = 0x55555555;
    this->mRotatePending = SL_BOOLEAN_FALSE;
#endif
    this->mOrientationVectors.mFront.z = -1000;
    this->mOrientationVectors.mUp.y = 1000;
    this->mOrientationActive = ANGLES_SET_VECTORS_UNKNOWN;
}

extern const struct SL3DSourceItf_ _3DSource_3DSourceItf;

static void _3DSource_init(void *self)
{
    struct _3DSource_interface *this = (struct _3DSource_interface *) self;
    this->mItf = &_3DSource_3DSourceItf;
#ifndef NDEBUG
    this->mHeadRelative = SL_BOOLEAN_FALSE;
    this->mRolloffMaxDistanceMute = SL_BOOLEAN_FALSE;
    this->mConeOuterLevel = 0;
    this->mRoomRolloffFactor = 0;
#endif
    this->mMaxDistance = SL_MILLIMETER_MAX;
    this->mMinDistance = 1000;
    this->mConeInnerAngle = 360000;
    this->mConeOuterAngle = 360000;
    this->mRolloffFactor = 1000;
    this->mDistanceModel = SL_ROLLOFFMODEL_EXPONENTIAL;
}

extern const struct SLAudioDecoderCapabilitiesItf_
    AudioDecoderCapabilities_AudioDecoderCapabilitiesItf;

static void AudioDecoderCapabilities_init(void *self)
{
    struct AudioDecoderCapabilities_interface *this =
        (struct AudioDecoderCapabilities_interface *) self;
    this->mItf = &AudioDecoderCapabilities_AudioDecoderCapabilitiesItf;
}

extern const struct SLAudioEncoderItf_ AudioEncoder_AudioEncoderItf;

static void AudioEncoder_init(void *self)
{
    struct AudioEncoder_interface *this = (struct AudioEncoder_interface *) self;
    this->mItf = &AudioEncoder_AudioEncoderItf;
#ifndef NDEBUG
    memset(&this->mSettings, 0, sizeof(SLAudioEncoderSettings));
#endif
}

extern const struct SLAudioEncoderCapabilitiesItf_
    AudioEncoderCapabilities_AudioEncoderCapabilitiesItf;

static void AudioEncoderCapabilities_init(void *self)
{
    struct AudioEncoderCapabilities_interface *this =
        (struct AudioEncoderCapabilities_interface *) self;
    this->mItf = &AudioEncoderCapabilities_AudioEncoderCapabilitiesItf;
}

extern const struct SLAudioIODeviceCapabilitiesItf_
    AudioIODeviceCapabilities_AudioIODeviceCapabilitiesItf;

static void AudioIODeviceCapabilities_init(void *self)
{
    struct AudioIODeviceCapabilities_interface *this =
        (struct AudioIODeviceCapabilities_interface *) self;
    this->mItf = &AudioIODeviceCapabilities_AudioIODeviceCapabilitiesItf;
#ifndef NDEBUG
    this->mAvailableAudioInputsChangedCallback = NULL;
    this->mAvailableAudioInputsChangedContext = NULL;
    this->mAvailableAudioOutputsChangedCallback = NULL;
    this->mAvailableAudioOutputsChangedContext = NULL;
    this->mDefaultDeviceIDMapChangedCallback = NULL;
    this->mDefaultDeviceIDMapChangedContext = NULL;
#endif
}

extern const struct SLBassBoostItf_ BassBoost_BassBoostItf;

static void BassBoost_init(void *self)
{
    struct BassBoost_interface *this = (struct BassBoost_interface *) self;
    this->mItf = &BassBoost_BassBoostItf;
#ifndef NDEBUG
    this->mEnabled = SL_BOOLEAN_FALSE;
#endif
    this->mStrength = 1000;
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
    struct BufferHeader *bufferHeader = this->mTypical;
    unsigned i;
    for (i = 0; i < BUFFER_HEADER_TYPICAL+1; ++i, ++bufferHeader) {
        bufferHeader->mBuffer = NULL;
        bufferHeader->mSize = 0;
    }
#endif
}

extern const struct SLDeviceVolumeItf_ DeviceVolume_DeviceVolumeItf;

static void DeviceVolume_init(void *self)
{
    struct DeviceVolume_interface *this =
        (struct DeviceVolume_interface *) self;
    this->mItf = &DeviceVolume_DeviceVolumeItf;
    this->mVolume[0] = 10;
    this->mVolume[1] = 10;
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

extern const struct SLDynamicSourceItf_ DynamicSource_DynamicSourceItf;

static void DynamicSource_init(void *self)
{
    struct DynamicSource_interface *this = (struct DynamicSource_interface *) self;
    this->mItf = &DynamicSource_DynamicSourceItf;
    // mDataSource will be initialized later in CreateAudioPlayer etc.
}

extern const struct SLEffectSendItf_ EffectSend_EffectSendItf;

static void EffectSend_init(void *self)
{
    struct EffectSend_interface *this = (struct EffectSend_interface *) self;
    this->mItf = &EffectSend_EffectSendItf;
#ifndef NDEBUG
    this->mOutputMix = NULL; // FIXME wrong
    this->mDirectLevel = 0;
    this->mEnableLevels[AUX_ENVIRONMENTALREVERB].mEnable = SL_BOOLEAN_FALSE;
    this->mEnableLevels[AUX_ENVIRONMENTALREVERB].mSendLevel = 0;
    this->mEnableLevels[AUX_PRESETREVERB].mEnable = SL_BOOLEAN_FALSE;
    this->mEnableLevels[AUX_PRESETREVERB].mSendLevel = 0;
#endif
}

extern const struct SLEngineItf_ Engine_EngineItf;

static void Engine_init(void *self)
{
    struct Engine_interface *this = (struct Engine_interface *) self;
    this->mItf = &Engine_EngineItf;
    // mLossOfControlGlobal is initialized in CreateEngine
}

extern const struct SLEngineCapabilitiesItf_
    EngineCapabilities_EngineCapabilitiesItf;

static void EngineCapabilities_init(void *self)
{
    struct EngineCapabilities_interface *this =
        (struct EngineCapabilities_interface *) self;
    this->mItf = &EngineCapabilities_EngineCapabilitiesItf;
}

// FIXME move
static const SLEnvironmentalReverbSettings EnvironmentalReverb_default = {
    SL_MILLIBEL_MIN, // roomLevel
    0,               // roomHFLevel
    1000,            // decayTime
    500,             // decayHFRatio
    SL_MILLIBEL_MIN, // reflectionsLevel
    20,              // reflectionsDelay
    SL_MILLIBEL_MIN, // reverbLevel
    40,              // reverbDelay
    1000,            // diffusion
    1000             // density
};

extern const struct SLEnvironmentalReverbItf_ EnvironmentalReverb_EnvironmentalReverbItf;

static void EnvironmentalReverb_init(void *self)
{
    struct EnvironmentalReverb_interface *this = (struct EnvironmentalReverb_interface *) self;
    this->mItf = &EnvironmentalReverb_EnvironmentalReverbItf;
    this->mProperties = EnvironmentalReverb_default;
}

extern const struct SLEqualizerItf_ Equalizer_EqualizerItf;

static void Equalizer_init(void *self)
{
    struct Equalizer_interface *this = (struct Equalizer_interface *) self;
    this->mItf = &Equalizer_EqualizerItf;
    this->mEnabled = SL_BOOLEAN_FALSE;
    this->mNumBands = 4;
    this->mBandLevelRangeMin = 0;
    this->mBandLevelRangeMax = 1000;
    struct EqualizerBand *band = this->mBands;
    unsigned i;
    for (i = 0; i < this->mNumBands; ++i, ++band) {
        this->mBands[i].mLevel = 0;
        this->mBands[i].mCenter = 0;
        this->mBands[i].mMin = 0;
        this->mBands[i].mMax = 0;
    }
    this->mPreset = 0;
}

extern const struct SLLEDArrayItf_ LEDArray_LEDArrayItf;

static void LEDArray_init(void *self)
{
    struct LEDArray_interface *this = (struct LEDArray_interface *) self;
    this->mItf = &LEDArray_LEDArrayItf;
#ifndef NDEBUG
    this->mLightMask = 0;
    this->mColor = NULL;
#endif
    this->mCount = 8;   // FIXME wrong place
}

extern const struct SLMetadataExtractionItf_ MetadataExtraction_MetadataExtractionItf;

static void MetadataExtraction_init(void *self)
{
    struct MetadataExtraction_interface *this = (struct MetadataExtraction_interface *) self;
    this->mItf = &MetadataExtraction_MetadataExtractionItf;
    this->mKeySize = 0;
    this->mKey = NULL;
    this->mKeyEncoding = 0 /*TBD*/;
    this->mValueLangCountry = 0 /*TBD*/;
    this->mValueEncoding = 0 /*TBD*/;
    this->mFilterMask = 0 /*TBD*/;
}

extern const struct SLMetadataTraversalItf_ MetadataTraversal_MetadataTraversalItf;

static void MetadataTraversal_init(void *self)
{
    struct MetadataTraversal_interface *this = (struct MetadataTraversal_interface *) self;
    this->mItf = &MetadataTraversal_MetadataTraversalItf;
#ifndef NDEBUG
    this->mIndex = 0;
#endif
}

extern const struct SLMIDIMessageItf_ MIDIMessage_MIDIMessageItf;

static void MIDIMessage_init(void *self)
{
    struct MIDIMessage_interface *this = (struct MIDIMessage_interface *) self;
    this->mItf = &MIDIMessage_MIDIMessageItf;
#ifndef NDEBUG
    this->mMetaEventCallback = NULL;
    this->mMetaEventContext = NULL;
    this->mMessageCallback = NULL;
    this->mMessageContext = NULL;
    this->mMessageTypes = 0 /*TBD*/;
#endif
}

extern const struct SLMIDIMuteSoloItf_ MIDIMuteSolo_MIDIMuteSoloItf;

static void MIDIMuteSolo_init(void *self)
{
    struct MIDIMuteSolo_interface *this = (struct MIDIMuteSolo_interface *) self;
    this->mItf = &MIDIMuteSolo_MIDIMuteSoloItf;
}

extern const struct SLMIDITempoItf_ MIDITempo_MIDITempoItf;

static void MIDITempo_init(void *self)
{
    struct MIDITempo_interface *this = (struct MIDITempo_interface *) self;
    this->mItf = &MIDITempo_MIDITempoItf;
}

extern const struct SLMIDITimeItf_ MIDITime_MIDITimeItf;

static void MIDITime_init(void *self)
{
    struct MIDITime_interface *this = (struct MIDITime_interface *) self;
    this->mItf = &MIDITime_MIDITimeItf;
}

extern const struct SLMuteSoloItf_ MuteSolo_MuteSoloItf;

static void MuteSolo_init(void *self)
{
    struct MuteSolo_interface *this = (struct MuteSolo_interface *) self;
    this->mItf = &MuteSolo_MuteSoloItf;
}

extern const struct SLPrefetchStatusItf_ PrefetchStatus_PrefetchStatusItf;

static void PrefetchStatus_init(void *self)
{
    struct PrefetchStatus_interface *this = (struct PrefetchStatus_interface *) self;
    this->mItf = &PrefetchStatus_PrefetchStatusItf;
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
    int ok;
    ok = pthread_mutex_init(&this->mMutex, (const pthread_mutexattr_t *) NULL);
    assert(0 == ok);
    ok = pthread_cond_init(&this->mCond, (const pthread_condattr_t *) NULL);
    assert(0 == ok);
}

extern const struct SLOutputMixItf_ OutputMix_OutputMixItf;

static void OutputMix_init(void *self)
{
    struct OutputMix_interface *this = (struct OutputMix_interface *) self;
    this->mItf = &OutputMix_OutputMixItf;
#ifndef NDEBUG
    this->mCallback = NULL;
    this->mContext = NULL;
#endif
#ifdef USE_OUTPUTMIXEXT
#ifndef NDEBUG
    this->mActiveMask = 0;
    struct Track *track = &this->mTracks[0];
    // FIXME O(n)
    // FIXME magic number
    unsigned i;
    for (i = 0; i < 32; ++i, ++track)
        track->mPlay = NULL;
#endif
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

extern const struct SLPitchItf_ Pitch_PitchItf;

static void Pitch_init(void *self)
{
    struct Pitch_interface *this = (struct Pitch_interface *) self;
    this->mItf = &Pitch_PitchItf;
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

extern const struct SLPlaybackRateItf_ PlaybackRate_PlaybackRateItf;

static void PlaybackRate_init(void *self)
{
    struct PlaybackRate_interface *this = (struct PlaybackRate_interface *) self;
    this->mItf = &PlaybackRate_PlaybackRateItf;
}

extern const struct SLPresetReverbItf_ PresetReverb_PresetReverbItf;

static void PresetReverb_init(void *self)
{
    struct PresetReverb_interface *this = (struct PresetReverb_interface *) self;
    this->mItf = &PresetReverb_PresetReverbItf;
}

extern const struct SLRatePitchItf_ RatePitch_RatePitchItf;

static void RatePitch_init(void *self)
{
    struct RatePitch_interface *this = (struct RatePitch_interface *) self;
    this->mItf = &RatePitch_RatePitchItf;
}

extern const struct SLRecordItf_ Record_RecordItf;

static void Record_init(void *self)
{
    struct Record_interface *this = (struct Record_interface *) self;
    this->mItf = &Record_RecordItf;
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
#ifndef NDEBUG
    this->mInCriticalSection = SL_BOOLEAN_FALSE;
    this->mWaiting = SL_BOOLEAN_FALSE;
    this->mOwner = (pthread_t) NULL;
#endif
}

extern const struct SLVirtualizerItf_ Virtualizer_VirtualizerItf;

static void Virtualizer_init(void *self)
{
    struct Virtualizer_interface *this = (struct Virtualizer_interface *) self;
    this->mItf = &Virtualizer_VirtualizerItf;
#ifndef NDEBUG
    this->mEnabled = SL_BOOLEAN_FALSE;
    this->mStrength = 0;
#endif
}

extern const struct SLVibraItf_ Vibra_VibraItf;

static void Vibra_init(void *self)
{
    struct Vibra_interface *this = (struct Vibra_interface *) self;
    this->mItf = &Vibra_VibraItf;
#ifndef NDEBUG
    this->mVibrate = SL_BOOLEAN_FALSE;
#endif
    this->mFrequency = SLVibraDescriptor_default.minFrequency;
    this->mIntensity = 1000;
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
#endif
    this->mRate = 20000;
}

extern const struct SLVolumeItf_ Volume_VolumeItf;

static void Volume_init(void *self)
{
    struct Volume_interface *this = (struct Volume_interface *) self;
    this->mItf = &Volume_VolumeItf;
#ifndef NDEBUG
    this->mLevel = 0;
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
    { /* MPH_METADATAEXTRACTION, */ MetadataExtraction_init, NULL },
    { /* MPH_METADATATRAVERSAL, */ MetadataTraversal_init, NULL },
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
#endif

static SLresult AudioPlayer_Realize(void *self)
{
    struct AudioPlayer_class *this = (struct AudioPlayer_class *) self;
    SLresult result = SL_RESULT_SUCCESS;

#ifdef USE_ANDROID
    // FIXME move this to android specific files
    result = sles_to_android_realizeAudioPlayer(this);
    int ok;
    ok = pthread_create(&this->mThread, (const pthread_attr_t *) NULL, thread_body, this);
    assert(ok == 0);
#endif

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
            // FIXME how do we know this interface is exposed?
            SLBufferQueueItf bufferQueue = &this->mBufferQueue.mItf;
            // FIXME should use a private internal API, and disallow
            // application to have access to our buffer queue
            // FIXME if we had an internal API, could call this directly
            // FIXME can't call this directly as we get a double lock
            // result = (*bufferQueue)->RegisterCallback(bufferQueue,
            //    SndFile_Callback, &this->mSndFile);
            // FIXME so let's inline the code, but this is maintenance risk
            // we know we are called by Object_Realize, which holds a lock,
            // but if interface lock != object lock, need to rewrite this
            struct BufferQueue_interface *thisBQ =
                (struct BufferQueue_interface *) bufferQueue;
            thisBQ->mCallback = SndFile_Callback;
            thisBQ->mContext = &this->mSndFile;
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
        offsetof(struct AudioPlayer_class, mMetadataExtraction)},
    {MPH_METADATATRAVERSAL, INTERFACE_MUSIC_GAME,
        offsetof(struct AudioPlayer_class, mMetadataTraversal)},
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
    NULL /*AudioPlayer_Resume*/,
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
        offsetof(struct LEDDevice_class, mLEDArray)}
};

static const struct class_ LEDDevice_class = {
    LEDDevice_interfaces,
    sizeof(LEDDevice_interfaces)/sizeof(LEDDevice_interfaces[0]),
    MPH_to_LEDDevice,
    //"LEDDevice",
    sizeof(struct LEDDevice_class),
    SL_OBJECTID_LEDDEVICE,
    NULL,
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
        offsetof(struct MetadataExtractor_class, mMetadataExtraction)},
    {MPH_METADATATRAVERSAL, INTERFACE_IMPLICIT,
        offsetof(struct MetadataExtractor_class, mMetadataTraversal)}
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
        offsetof(struct MidiPlayer_class, mMetadataExtraction)},
    {MPH_METADATATRAVERSAL, INTERFACE_GAME,
        offsetof(struct MidiPlayer_class, mMetadataTraversal)},
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

static struct Object_interface *construct(const struct class_ *class__,
    unsigned exposedMask, SLEngineItf engine)
{
    struct Object_interface *this;
#ifndef NDEBUG
    this = (struct Object_interface *) malloc(class__->mSize);
#else
    this = (struct Object_interface *) calloc(1, class__->mSize);
#endif
    if (NULL != this) {
#ifndef NDEBUG
        // for debugging, to detect uninitialized fields
        memset(this, 0x55, class__->mSize);
#endif
        this->mClass = class__;
        this->mExposedMask = exposedMask;
        struct Engine_interface *thisEngine =
            (struct Engine_interface *) engine;
        this->mLossOfControlMask = (NULL == thisEngine) ? 0 :
            (thisEngine->mLossOfControlGlobal ? ~0 : 0);
        const struct iid_vtable *x = class__->mInterfaces;
        unsigned i;
        for (i = 0; exposedMask; ++i, ++x, exposedMask >>= 1) {
            if (exposedMask & 1) {
                void *self = (char *) this + x->mOffset;
                ((struct Object_interface **) self)[1] = this;
                VoidHook init = MPH_init_table[x->mMPH].mInit;
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
    const struct class_ *class__ = this->mClass;
    StatusHook realize = class__->mRealize;
    SLresult result = SL_RESULT_SUCCESS;
    object_lock_exclusive(this);
    // FIXME The realize hook and callback should be asynchronous if requested
    if (SL_OBJECT_STATE_UNREALIZED != this->mState) {
        result = SL_RESULT_PRECONDITIONS_VIOLATED;
    } else {
        if (NULL != realize)
            result = (*realize)(this);
        if (SL_RESULT_SUCCESS == result)
            this->mState = SL_OBJECT_STATE_REALIZED;
        // FIXME callback should not run with mutex lock
        if (async && (NULL != this->mCallback))
            (*this->mCallback)(self, this->mContext,
            SL_OBJECT_EVENT_ASYNC_TERMINATION, result, this->mState, NULL);
    }
    object_unlock_exclusive(this);
    return result;
}

static SLresult Object_Resume(SLObjectItf self, SLboolean async)
{
    struct Object_interface *this = (struct Object_interface *) self;
    const struct class_ *class__ = this->mClass;
    StatusHook resume = class__->mResume;
    SLresult result = SL_RESULT_SUCCESS;
    object_lock_exclusive(this);
    // FIXME The resume hook and callback should be asynchronous if requested
    if (SL_OBJECT_STATE_SUSPENDED != this->mState) {
        result = SL_RESULT_PRECONDITIONS_VIOLATED;
    } else {
        if (NULL != resume)
            result = (*resume)(this);
        if (SL_RESULT_SUCCESS == result)
            this->mState = SL_OBJECT_STATE_REALIZED;
        // FIXME callback should not run with mutex lock
        if (async && (NULL != this->mCallback))
            (*this->mCallback)(self, this->mContext,
            SL_OBJECT_EVENT_ASYNC_TERMINATION, result, this->mState, NULL);
    }
    object_unlock_exclusive(this);
    return result;
}

static SLresult Object_GetState(SLObjectItf self, SLuint32 *pState)
{
    if (NULL == pState)
        return SL_RESULT_PARAMETER_INVALID;
    struct Object_interface *this = (struct Object_interface *) self;
    // FIXME Investigate what it would take to change to a peek lock
    object_lock_shared(this);
    SLuint32 state = this->mState;
    object_unlock_shared(this);
    *pState = state;
    return SL_RESULT_SUCCESS;
}

static SLresult Object_GetInterface(SLObjectItf self, const SLInterfaceID iid,
    void *pInterface)
{
    if (NULL == pInterface)
        return SL_RESULT_PARAMETER_INVALID;
    SLresult result;
    void *interface = NULL;
    if (NULL == iid)
        result = SL_RESULT_PARAMETER_INVALID;
    else {
        struct Object_interface *this = (struct Object_interface *) self;
        const struct class_ *class__ = this->mClass;
        int MPH, index;
        if ((0 > (MPH = IID_to_MPH(iid))) ||
            (0 > (index = class__->mMPH_to_index[MPH])))
            result = SL_RESULT_FEATURE_UNSUPPORTED;
        else {
            unsigned mask = 1 << index;
            object_lock_shared(this);
            if (SL_OBJECT_STATE_REALIZED != this->mState)
                result = SL_RESULT_PRECONDITIONS_VIOLATED;
            else if (!(this->mExposedMask & mask))
                result = SL_RESULT_FEATURE_UNSUPPORTED;
            else {
                // FIXME Should note that interface has been gotten,
                // so as to detect use of ill-gotten interfaces; be sure
                // to change the lock to exclusive if that is done
                interface = (char *) this + class__->mInterfaces[index].mOffset;
                result = SL_RESULT_SUCCESS;
            }
            object_unlock_shared(this);
        }
    }
    *(void **)pInterface = interface;
    return SL_RESULT_SUCCESS;
}

static SLresult Object_RegisterCallback(SLObjectItf self,
    slObjectCallback callback, void *pContext)
{
    struct Object_interface *this = (struct Object_interface *) self;
    // FIXME This could be a poke lock, if we had atomic double-word load/store
    object_lock_exclusive(this);
    this->mCallback = callback;
    this->mContext = pContext;
    object_unlock_exclusive(this);
    return SL_RESULT_SUCCESS;
}

static void Object_AbortAsyncOperation(SLObjectItf self)
{
    // FIXME Asynchronous operations are not yet implemented
}

static void Object_Destroy(SLObjectItf self)
{
    Object_AbortAsyncOperation(self);
    struct Object_interface *this = (struct Object_interface *) self;
    const struct class_ *class__ = this->mClass;
    VoidHook destroy = class__->mDestroy;
    const struct iid_vtable *x = class__->mInterfaces;
    object_lock_exclusive(this);
    // Call the deinitializer for each currently exposed interface,
    // whether it is implicit, explicit, optional, or dynamically added.
    unsigned exposedMask = this->mExposedMask;
    for ( ; exposedMask; exposedMask >>= 1, ++x) {
        if (exposedMask & 1) {
            VoidHook deinit = MPH_init_table[x->mMPH].mDeinit;
            if (NULL != deinit)
                (*deinit)((char *) this + x->mOffset);
        }
    }
    if (NULL != destroy)
        (*destroy)(this);
    // redundant: this->mState = SL_OBJECT_STATE_UNREALIZED;
    object_unlock_exclusive(this);
#ifndef NDEBUG
    memset(this, 0x55, class__->mSize);
#endif
    free(this);
}

static SLresult Object_SetPriority(SLObjectItf self, SLint32 priority,
    SLboolean preemptable)
{
    struct Object_interface *this = (struct Object_interface *) self;
    object_lock_exclusive(this);
    this->mPriority = priority;
    this->mPreemptable = preemptable;
    object_unlock_exclusive(this);
    return SL_RESULT_SUCCESS;
}

static SLresult Object_GetPriority(SLObjectItf self, SLint32 *pPriority,
    SLboolean *pPreemptable)
{
    if (NULL == pPriority || NULL == pPreemptable)
        return SL_RESULT_PARAMETER_INVALID;
    struct Object_interface *this = (struct Object_interface *) self;
    object_lock_shared(this);
    SLint32 priority = this->mPriority;
    SLboolean preemptable = this->mPreemptable;
    object_unlock_shared(this);
    *pPriority = priority;
    *pPreemptable = preemptable;
    return SL_RESULT_SUCCESS;
}

static SLresult Object_SetLossOfControlInterfaces(SLObjectItf self,
    SLint16 numInterfaces, SLInterfaceID *pInterfaceIDs, SLboolean enabled)
{
    if (0 < numInterfaces) {
        SLuint32 i;
        if (NULL == pInterfaceIDs)
            return SL_RESULT_PARAMETER_INVALID;
        struct Object_interface *this = (struct Object_interface *) self;
        const struct class_ *class__ = this->mClass;
        unsigned lossOfControlMask = 0;
        // FIXME The cast is due to a typo in the spec
        for (i = 0; i < (SLuint32) numInterfaces; ++i) {
            SLInterfaceID iid = pInterfaceIDs[i];
            if (NULL == iid)
                return SL_RESULT_PARAMETER_INVALID;
            int MPH, index;
            if (0 <= (MPH = IID_to_MPH(iid)) &&
                (0 <= (index = class__->mMPH_to_index[MPH])))
                lossOfControlMask |= (1 << index);
        }
        object_lock_exclusive(this);
        if (enabled)
            this->mLossOfControlMask |= lossOfControlMask;
        else
            this->mLossOfControlMask &= ~lossOfControlMask;
        object_unlock_exclusive(this);
    }
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
    struct Object_interface *thisObject = this->mThis;
    const struct class_ *class__ = thisObject->mClass;
    int MPH, index;
    if ((0 > (MPH = IID_to_MPH(iid))) ||
        (0 > (index = class__->mMPH_to_index[MPH])))
        return SL_RESULT_FEATURE_UNSUPPORTED;
    SLresult result;
    VoidHook init = MPH_init_table[MPH].mInit;
    const struct iid_vtable *x = &class__->mInterfaces[index];
    size_t offset = x->mOffset;
    void *thisItf = (char *) thisObject + offset;
    size_t size = ((SLuint32) (index + 1) == class__->mInterfaceCount ?
        class__->mSize : x[1].mOffset) - offset;
    unsigned mask = 1 << index;
    // Lock the object rather than the DIM interface, because
    // we modify both the object (exposed) and interface (added)
    object_lock_exclusive(thisObject);
    if (thisObject->mExposedMask & mask) {
        result = SL_RESULT_PRECONDITIONS_VIOLATED;
    } else {
        // FIXME Currently do initialization here, but might be asynchronous
#ifndef NDEBUG
// for debugging, to detect uninitialized fields
#define FILLER 0x55
#else
#define FILLER 0
#endif
        memset(thisItf, FILLER, size);
        ((void **) thisItf)[1] = thisObject;
        if (NULL != init)
            (*init)(thisItf);
        thisObject->mExposedMask |= mask;
        this->mAddedMask |= mask;
        result = SL_RESULT_SUCCESS;
        if (async && (NULL != this->mCallback)) {
            // FIXME Callback runs with mutex locked
            (*this->mCallback)(self, this->mContext,
                SL_DYNAMIC_ITF_EVENT_RESOURCES_AVAILABLE, result, iid);
        }
    }
    object_unlock_exclusive(thisObject);
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
        (struct Object_interface *) this->mThis;
    const struct class_ *class__ = thisObject->mClass;
    int MPH = IID_to_MPH(iid);
    if (0 > MPH)
        return SL_RESULT_PRECONDITIONS_VIOLATED;
    int index = class__->mMPH_to_index[MPH];
    if (0 > index)
        return SL_RESULT_PRECONDITIONS_VIOLATED;
    SLresult result = SL_RESULT_SUCCESS;
    VoidHook deinit = MPH_init_table[MPH].mDeinit;
    const struct iid_vtable *x = &class__->mInterfaces[index];
    size_t offset = x->mOffset;
    void *thisItf = (char *) thisObject + offset;
    size_t size = ((SLuint32) (index + 1) == class__->mInterfaceCount ?
        class__->mSize : x[1].mOffset) - offset;
    unsigned mask = 1 << index;
    // Lock the object rather than the DIM interface, because
    // we modify both the object (exposed) and interface (added)
    object_lock_exclusive(thisObject);
    // disallow removal of non-dynamic interfaces
    if (!(this->mAddedMask & mask)) {
        result = SL_RESULT_PRECONDITIONS_VIOLATED;
    } else {
        if (NULL != deinit)
            (*deinit)(thisItf);
#ifndef NDEBUG
        memset(thisItf, 0x55, size);
#endif
        thisObject->mExposedMask &= ~mask;
        this->mAddedMask &= ~mask;
    }
    object_unlock_exclusive(thisObject);
    return result;
}

static SLresult DynamicInterfaceManagement_ResumeInterface(
    SLDynamicInterfaceManagementItf self,
    const SLInterfaceID iid, SLboolean async)
{
    if (NULL == iid)
        return SL_RESULT_PARAMETER_INVALID;
    struct DynamicInterfaceManagement_interface *this =
        (struct DynamicInterfaceManagement_interface *) self;
    struct Object_interface *thisObject =
        (struct Object_interface *) this->mThis;
    const struct class_ *class__ = thisObject->mClass;
    int MPH = IID_to_MPH(iid);
    if (0 > MPH)
        return SL_RESULT_PRECONDITIONS_VIOLATED;
    int index = class__->mMPH_to_index[MPH];
    if (0 > index)
        return SL_RESULT_PRECONDITIONS_VIOLATED;
    SLresult result = SL_RESULT_SUCCESS;
    unsigned mask = 1 << index;
    // FIXME Change to exclusive when resume hook implemented
    object_lock_shared(thisObject);
    if (!(this->mAddedMask & mask))
        result = SL_RESULT_PRECONDITIONS_VIOLATED;
    // FIXME Call a resume hook on the interface, if suspended
    object_unlock_shared(thisObject);
    return result;
}

static SLresult DynamicInterfaceManagement_RegisterCallback(
    SLDynamicInterfaceManagementItf self,
    slDynamicInterfaceManagementCallback callback, void *pContext)
{
    struct DynamicInterfaceManagement_interface *this =
        (struct DynamicInterfaceManagement_interface *) self;
    struct Object_interface *thisObject = this->mThis;
    // FIXME This could be a poke lock, if we had atomic double-word load/store
    object_lock_exclusive(thisObject);
    this->mCallback = callback;
    this->mContext = pContext;
    object_unlock_exclusive(thisObject);
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
    interface_lock_exclusive(this);
    this->mState = state;
    if (SL_PLAYSTATE_STOPPED == state) {
        this->mPosition = (SLmillisecond) 0;
        // this->mPositionSamples = 0;
    }
    interface_unlock_exclusive(this);
#ifdef USE_ANDROID
    return sles_to_android_audioPlayerSetPlayState(this, state);
#else
    return SL_RESULT_SUCCESS;
#endif
}

static SLresult Play_GetPlayState(SLPlayItf self, SLuint32 *pState)
{
    if (NULL == pState)
        return SL_RESULT_PARAMETER_INVALID;
    struct Play_interface *this = (struct Play_interface *) self;
    interface_lock_peek(this);
    SLuint32 state = this->mState;
    interface_unlock_peek(this);
    *pState = state;
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
    interface_lock_peek(this);
    SLmillisecond duration = this->mDuration;
    interface_unlock_peek(this);
    *pMsec = duration;
    return SL_RESULT_SUCCESS;
}

static SLresult Play_GetPosition(SLPlayItf self, SLmillisecond *pMsec)
{
    if (NULL == pMsec)
        return SL_RESULT_PARAMETER_INVALID;
    struct Play_interface *this = (struct Play_interface *) self;
    interface_lock_peek(this);
    SLmillisecond position = this->mPosition;
    interface_unlock_peek(this);
    *pMsec = position;
    // FIXME convert sample units to time units
    // FIXME handle SL_TIME_UNKNOWN
    return SL_RESULT_SUCCESS;
}

static SLresult Play_RegisterCallback(SLPlayItf self, slPlayCallback callback,
    void *pContext)
{
    struct Play_interface *this = (struct Play_interface *) self;
    // FIXME This could be a poke lock, if we had atomic double-word load/store
    interface_lock_exclusive(this);
    this->mCallback = callback;
    this->mContext = pContext;
    interface_unlock_exclusive(this);
    return SL_RESULT_SUCCESS;
}

static SLresult Play_SetCallbackEventsMask(SLPlayItf self, SLuint32 eventFlags)
{
    struct Play_interface *this = (struct Play_interface *) self;
    interface_lock_poke(this);
    this->mEventFlags = eventFlags;
    interface_unlock_poke(this);
    return SL_RESULT_SUCCESS;
}

static SLresult Play_GetCallbackEventsMask(SLPlayItf self,
    SLuint32 *pEventFlags)
{
    if (NULL == pEventFlags)
        return SL_RESULT_PARAMETER_INVALID;
    struct Play_interface *this = (struct Play_interface *) self;
    interface_lock_peek(this);
    SLuint32 eventFlags = this->mEventFlags;
    interface_unlock_peek(this);
    *pEventFlags = eventFlags;
    return SL_RESULT_SUCCESS;
}

static SLresult Play_SetMarkerPosition(SLPlayItf self, SLmillisecond mSec)
{
    struct Play_interface *this = (struct Play_interface *) self;
    interface_lock_poke(this);
    this->mMarkerPosition = mSec;
    interface_unlock_poke(this);
    return SL_RESULT_SUCCESS;
}

static SLresult Play_ClearMarkerPosition(SLPlayItf self)
{
    struct Play_interface *this = (struct Play_interface *) self;
    interface_lock_poke(this);
    this->mMarkerPosition = 0;
    interface_unlock_poke(this);
    return SL_RESULT_SUCCESS;
}

static SLresult Play_GetMarkerPosition(SLPlayItf self, SLmillisecond *pMsec)
{
    if (NULL == pMsec)
        return SL_RESULT_PARAMETER_INVALID;
    struct Play_interface *this = (struct Play_interface *) self;
    interface_lock_peek(this);
    SLmillisecond markerPosition = this->mMarkerPosition;
    interface_unlock_peek(this);
    *pMsec = markerPosition;
    return SL_RESULT_SUCCESS;
}

static SLresult Play_SetPositionUpdatePeriod(SLPlayItf self, SLmillisecond mSec)
{
    struct Play_interface *this = (struct Play_interface *) self;
    interface_lock_poke(this);
    this->mPositionUpdatePeriod = mSec;
    interface_unlock_poke(this);
    return SL_RESULT_SUCCESS;
}

static SLresult Play_GetPositionUpdatePeriod(SLPlayItf self,
    SLmillisecond *pMsec)
{
    if (NULL == pMsec)
        return SL_RESULT_PARAMETER_INVALID;
    struct Play_interface *this = (struct Play_interface *) self;
    interface_lock_peek(this);
    SLmillisecond positionUpdatePeriod = this->mPositionUpdatePeriod;
    interface_unlock_peek(this);
    *pMsec = positionUpdatePeriod;
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
    //FIXME if queue is empty and associated player is not in SL_PLAYSTATE_PLAYING state,
    // set it to SL_PLAYSTATE_PLAYING (and start playing)
    if (NULL == pBuffer)
        return SL_RESULT_PARAMETER_INVALID;
    struct BufferQueue_interface *this = (struct BufferQueue_interface *) self;
    SLresult result;
    interface_lock_exclusive(this);
    struct BufferHeader *oldRear = this->mRear, *newRear;
    if ((newRear = oldRear + 1) == &this->mArray[this->mNumBuffers])
        newRear = this->mArray;
    if (newRear == this->mFront) {
        result = SL_RESULT_BUFFER_INSUFFICIENT;
    } else {
        oldRear->mBuffer = pBuffer;
        oldRear->mSize = size;
        this->mRear = newRear;
        ++this->mState.count;
        result = SL_RESULT_SUCCESS;
    }
    interface_unlock_exclusive(this);
    return result;
}

static SLresult BufferQueue_Clear(SLBufferQueueItf self)
{
    struct BufferQueue_interface *this = (struct BufferQueue_interface *) self;
    interface_lock_exclusive(this);
    this->mFront = &this->mArray[0];
    this->mRear = &this->mArray[0];
    this->mState.count = 0;
    interface_unlock_exclusive(this);
    return SL_RESULT_SUCCESS;
}

static SLresult BufferQueue_GetState(SLBufferQueueItf self,
    SLBufferQueueState *pState)
{
    if (NULL == pState)
        return SL_RESULT_PARAMETER_INVALID;
    struct BufferQueue_interface *this = (struct BufferQueue_interface *) self;
    SLBufferQueueState state;
    interface_lock_shared(this);
#ifdef __cplusplus // FIXME Is this a compiler bug?
    state.count = this->mState.count;
    state.playIndex = this->mState.playIndex;
#else
    state = this->mState;
#endif
    interface_unlock_shared(this);
#ifdef __cplusplus // FIXME Is this a compiler bug?
    pState->count = state.count;
    pState->playIndex = state.playIndex;
#else
    *pState = state;
#endif
    return SL_RESULT_SUCCESS;
}

static SLresult BufferQueue_RegisterCallback(SLBufferQueueItf self,
    slBufferQueueCallback callback, void *pContext)
{
    struct BufferQueue_interface *this = (struct BufferQueue_interface *) self;
    // FIXME This could be a poke lock, if we had atomic double-word load/store
    interface_lock_exclusive(this);
    this->mCallback = callback;
    this->mContext = pContext;
    interface_unlock_exclusive(this);
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
#if 0
    // some compilers allow a wider int type to be passed as a parameter,
    // but that will be truncated during the field assignment
    if (!((SL_MILLIBEL_MIN <= level) && (SL_MILLIBEL_MAX >= level)))
        return SL_RESULT_PARAMETER_INVALID;
#endif
    struct Volume_interface *this = (struct Volume_interface *) self;
    interface_lock_poke(this);
    this->mLevel = level;
    interface_unlock_poke(this);
    return SL_RESULT_SUCCESS;
}

static SLresult Volume_GetVolumeLevel(SLVolumeItf self, SLmillibel *pLevel)
{
    if (NULL == pLevel)
        return SL_RESULT_PARAMETER_INVALID;
    struct Volume_interface *this = (struct Volume_interface *) self;
    interface_lock_peek(this);
    SLmillibel level = this->mLevel;
    interface_unlock_peek(this);
    *pLevel = level;
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
    interface_lock_poke(this);
    this->mMute = mute;
    interface_unlock_poke(this);
    return SL_RESULT_SUCCESS;
}

static SLresult Volume_GetMute(SLVolumeItf self, SLboolean *pMute)
{
    if (NULL == pMute)
        return SL_RESULT_PARAMETER_INVALID;
    struct Volume_interface *this = (struct Volume_interface *) self;
    interface_lock_peek(this);
    SLboolean mute = this->mMute;
    interface_unlock_peek(this);
    *pMute = mute;
    return SL_RESULT_SUCCESS;
}

static SLresult Volume_EnableStereoPosition(SLVolumeItf self, SLboolean enable)
{
    struct Volume_interface *this = (struct Volume_interface *) self;
    interface_lock_poke(this);
    this->mEnableStereoPosition = enable;
    interface_unlock_poke(this);
    return SL_RESULT_SUCCESS;
}

static SLresult Volume_IsEnabledStereoPosition(SLVolumeItf self,
    SLboolean *pEnable)
{
    if (NULL == pEnable)
        return SL_RESULT_PARAMETER_INVALID;
    struct Volume_interface *this = (struct Volume_interface *) self;
    interface_lock_peek(this);
    SLboolean enable = this->mEnableStereoPosition;
    interface_unlock_peek(this);
    *pEnable = enable;
    return SL_RESULT_SUCCESS;
}

static SLresult Volume_SetStereoPosition(SLVolumeItf self,
    SLpermille stereoPosition)
{
    if (!((-1000 <= stereoPosition) && (1000 >= stereoPosition)))
        return SL_RESULT_PARAMETER_INVALID;
    struct Volume_interface *this = (struct Volume_interface *) self;
    interface_lock_poke(this);
    this->mStereoPosition = stereoPosition;
    interface_unlock_poke(this);
    return SL_RESULT_SUCCESS;
}

static SLresult Volume_GetStereoPosition(SLVolumeItf self,
    SLpermille *pStereoPosition)
{
    if (NULL == pStereoPosition)
        return SL_RESULT_PARAMETER_INVALID;
    struct Volume_interface *this = (struct Volume_interface *) self;
    interface_lock_peek(this);
    SLpermille stereoPosition = this->mStereoPosition;
    interface_unlock_peek(this);
    *pStereoPosition = stereoPosition;
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
    struct LEDDevice_class *this = (struct LEDDevice_class *)
        construct(&LEDDevice_class, exposedMask, self);
    if (NULL == this)
        return SL_RESULT_MEMORY_FAILURE;
    SLHSL *color = (SLHSL *) malloc(sizeof(SLHSL) * this->mLEDArray.mCount);
    // FIXME
    assert(NULL != this->mLEDArray.mColor);
    this->mLEDArray.mColor = color;
    unsigned i;
    for (i = 0; i < this->mLEDArray.mCount; ++i) {
        // per specification 1.0.1 pg. 259: "Default color is undefined."
        color->hue = 0;
        color->saturation = 1000;
        color->lightness = 1000;
    }
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
    struct VibraDevice_class *this = (struct VibraDevice_class *)
        construct(&VibraDevice_class, exposedMask, self);
    if (NULL == this)
        return SL_RESULT_MEMORY_FAILURE;
    this->mDeviceID = deviceID;
    *pDevice = &this->mObject.mItf;
    return SL_RESULT_SUCCESS;
}


static SLresult Engine_CreateAudioPlayer(SLEngineItf self, SLObjectItf *pPlayer,
    SLDataSource *pAudioSrc, SLDataSink *pAudioSnk, SLuint32 numInterfaces,
    const SLInterfaceID *pInterfaceIds, const SLboolean *pInterfaceRequired)
{
    fprintf(stderr, "entering Engine_CreateAudioPlayer()\n");

    if (NULL == pPlayer)
        return SL_RESULT_PARAMETER_INVALID;
    *pPlayer = NULL;
    unsigned exposedMask;
    SLresult result = checkInterfaces(&AudioPlayer_class, numInterfaces,
        pInterfaceIds, pInterfaceRequired, &exposedMask);
    if (SL_RESULT_SUCCESS != result)
        return result;
    // check the audio source and sinks
    result = sles_checkAudioPlayerSourceSink(pAudioSrc, pAudioSnk);
    if (result != SL_RESULT_SUCCESS) {
        return result;
    }
    fprintf(stderr, "\t after sles_checkSourceSink()\n");
    // check the audio source and sink parameters against platform support
#ifdef USE_ANDROID
    result = sles_to_android_checkAudioPlayerSourceSink(pAudioSrc, pAudioSnk);
    if (result != SL_RESULT_SUCCESS) {
            return result;
    }
    fprintf(stderr, "\t after sles_to_android_checkAudioPlayerSourceSink()\n");
#endif

#ifdef USE_SNDFILE
    SLuint32 locatorType = *(SLuint32 *)pAudioSrc->pLocator;
    SLuint32 formatType = *(SLuint32 *)pAudioSrc->pFormat;
    SLuint32 numBuffers = 0;
    SLDataFormat_PCM *df_pcm = NULL;
#endif
#ifdef USE_OUTPUTMIXEXT
    struct Track *track = NULL;
#endif
#ifdef USE_SNDFILE
    SLchar *pathname = NULL;
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
#endif // USE_SNDFILE

#ifdef USE_OUTPUTMIXEXT
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
        // FIXME replace the above for Android - do not use our own mixer!
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
#endif // USE_OUTPUTMIXEXT

    // Construct our new AudioPlayer instance
    struct AudioPlayer_class *this = (struct AudioPlayer_class *)
        construct(&AudioPlayer_class, exposedMask, self);
    if (NULL == this)
        return SL_RESULT_MEMORY_FAILURE;

    // DataSource specific initializations
    switch(*(SLuint32 *)pAudioSrc->pLocator) {
    case SL_DATALOCATOR_BUFFERQUEUE:
        {
        SLDataLocator_BufferQueue *dl_bq = (SLDataLocator_BufferQueue *) pAudioSrc->pLocator;
        SLuint32 numBuffs = dl_bq->numBuffers;
        if (0 == numBuffs) {
            return SL_RESULT_PARAMETER_INVALID;
        }
        this->mBufferQueue.mNumBuffers = numBuffs;
        // inline allocation of circular mArray, up to a typical max
        if (BUFFER_HEADER_TYPICAL >= numBuffs) {
            this->mBufferQueue.mArray = this->mBufferQueue.mTypical;
        } else {
            // FIXME integer overflow possible during multiplication
            this->mBufferQueue.mArray = (struct BufferHeader *)
                    malloc((numBuffs + 1) * sizeof(struct BufferHeader));
            if (NULL == this->mBufferQueue.mArray) {
                free(this);
                return SL_RESULT_MEMORY_FAILURE;
            }
        }
        this->mBufferQueue.mFront = this->mBufferQueue.mArray;
        this->mBufferQueue.mRear = this->mBufferQueue.mArray;
        }
        break;
    case SL_DATALOCATOR_URI:
        break;
    default:
        // no other init required
        break;
    }

    // used to store the data source of our audio player
    this->mDynamicSource.mDataSource = pAudioSrc;
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
    sles_to_android_createAudioPlayer(pAudioSrc, pAudioSnk, this);
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
    struct MidiPlayer_class *this = (struct MidiPlayer_class *)
        construct(&MidiPlayer_class, exposedMask, self);
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
    struct OutputMix_class *this = (struct OutputMix_class *)
        construct(&OutputMix_class, exposedMask, self);
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
        construct(&MetadataExtractor_class, exposedMask, self);
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
    if (NULL == pInterfaceId)
        return SL_RESULT_PARAMETER_INVALID;
    const struct class_ *class__ = objectIDtoClass(objectID);
    if (NULL == class__)
        return SL_RESULT_FEATURE_UNSUPPORTED;
    if (index >= class__->mInterfaceCount)
        return SL_RESULT_PARAMETER_INVALID;
    *pInterfaceId = &SL_IID_array[class__->mInterfaces[index].mMPH];
    return SL_RESULT_SUCCESS;
};

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
    if (NULL == pNumInputs)
        return SL_RESULT_PARAMETER_INVALID;
    if (NULL != pInputDeviceIDs) {
        // FIXME should be OEM-configurable
        if (1 > *pNumInputs)
            return SL_RESULT_BUFFER_INSUFFICIENT;
        pInputDeviceIDs[0] = SL_DEFAULTDEVICEID_AUDIOINPUT;
    }
    *pNumInputs = 1;
    return SL_RESULT_SUCCESS;
}

static SLresult AudioIODeviceCapabilities_QueryAudioInputCapabilities(
    SLAudioIODeviceCapabilitiesItf self, SLuint32 deviceID,
    SLAudioInputDescriptor *pDescriptor)
{
    if (NULL == pDescriptor)
        return SL_RESULT_PARAMETER_INVALID;
    switch (deviceID) {
    // FIXME should be OEM-configurable
    case SL_DEFAULTDEVICEID_AUDIOINPUT:
        *pDescriptor = AudioInputDescriptor_mic;
        break;
    default:
        return SL_RESULT_IO_ERROR;
    }
    return SL_RESULT_SUCCESS;
}

static SLresult
    AudioIODeviceCapabilities_RegisterAvailableAudioInputsChangedCallback(
    SLAudioIODeviceCapabilitiesItf self,
    slAvailableAudioInputsChangedCallback callback, void *pContext)
{
    struct AudioIODeviceCapabilities_interface * this =
        (struct AudioIODeviceCapabilities_interface *) self;
    interface_lock_exclusive(this);
    this->mAvailableAudioInputsChangedCallback = callback;
    this->mAvailableAudioInputsChangedContext = pContext;
    interface_unlock_exclusive(this);
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
    struct AudioIODeviceCapabilities_interface * this =
        (struct AudioIODeviceCapabilities_interface *) self;
    interface_lock_exclusive(this);
    this->mAvailableAudioOutputsChangedCallback = callback;
    this->mAvailableAudioOutputsChangedContext = pContext;
    interface_unlock_exclusive(this);
    return SL_RESULT_SUCCESS;
}

static SLresult
    AudioIODeviceCapabilities_RegisterDefaultDeviceIDMapChangedCallback(
    SLAudioIODeviceCapabilitiesItf self,
    slDefaultDeviceIDMapChangedCallback callback, void *pContext)
{
    struct AudioIODeviceCapabilities_interface * this =
        (struct AudioIODeviceCapabilities_interface *) self;
    interface_lock_exclusive(this);
    this->mDefaultDeviceIDMapChangedCallback = callback;
    this->mDefaultDeviceIDMapChangedContext = pContext;
    interface_unlock_exclusive(this);
    return SL_RESULT_SUCCESS;
}

static SLresult AudioIODeviceCapabilities_GetAssociatedAudioInputs(
    SLAudioIODeviceCapabilitiesItf self, SLuint32 deviceID,
    SLint32 *pNumAudioInputs, SLuint32 *pAudioInputDeviceIDs)
{
    if (NULL == pNumAudioInputs)
        return SL_RESULT_PARAMETER_INVALID;
    *pNumAudioInputs = 0;
    return SL_RESULT_SUCCESS;
}

static SLresult AudioIODeviceCapabilities_GetAssociatedAudioOutputs(
    SLAudioIODeviceCapabilitiesItf self, SLuint32 deviceID,
    SLint32 *pNumAudioOutputs, SLuint32 *pAudioOutputDeviceIDs)
{
    if (NULL == pNumAudioOutputs)
        return SL_RESULT_PARAMETER_INVALID;
    *pNumAudioOutputs = 0;
    return SL_RESULT_SUCCESS;
}

static SLresult AudioIODeviceCapabilities_GetDefaultAudioDevices(
    SLAudioIODeviceCapabilitiesItf self, SLuint32 defaultDeviceID,
    SLint32 *pNumAudioDevices, SLuint32 *pAudioDeviceIDs)
{
    if (NULL == pNumAudioDevices)
        return SL_RESULT_PARAMETER_INVALID;
    switch (defaultDeviceID) {
    case SL_DEFAULTDEVICEID_AUDIOINPUT:
    case SL_DEFAULTDEVICEID_AUDIOOUTPUT:
        break;
    default:
        return SL_RESULT_IO_ERROR;
    }
    if (NULL != pAudioDeviceIDs) {
        // FIXME should be OEM-configurable
        if (2 > *pNumAudioDevices)
            return SL_RESULT_BUFFER_INSUFFICIENT;
        pAudioDeviceIDs[0] = DEVICE_ID_HEADSET;
        pAudioDeviceIDs[1] = DEVICE_ID_HANDSFREE;
    }
    *pNumAudioDevices = 1;
    return SL_RESULT_SUCCESS;
}

static SLresult AudioIODeviceCapabilities_QuerySampleFormatsSupported(
    SLAudioIODeviceCapabilitiesItf self, SLuint32 deviceID,
    SLmilliHertz samplingRate, SLint32 *pSampleFormats,
    SLint32 *pNumOfSampleFormats)
{
    if (NULL == pNumOfSampleFormats)
        return SL_RESULT_PARAMETER_INVALID;
    switch (deviceID) {
    case SL_DEFAULTDEVICEID_AUDIOINPUT:
    case SL_DEFAULTDEVICEID_AUDIOOUTPUT:
        break;
    default:
        return SL_RESULT_IO_ERROR;
    }
    switch (samplingRate) {
    case SL_SAMPLINGRATE_44_1:
        break;
    default:
        return SL_RESULT_IO_ERROR;
    }
    if (NULL != pSampleFormats) {
        if (1 > *pNumOfSampleFormats)
            return SL_RESULT_BUFFER_INSUFFICIENT;
        pSampleFormats[0] = SL_PCMSAMPLEFORMAT_FIXED_16;
    }
    *pNumOfSampleFormats = 1;
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
    if (NULL == pNumDevices)
        return SL_RESULT_PARAMETER_INVALID;
    if (NULL != pDeviceIDs) {
        if (1 > *pNumDevices)
            return SL_RESULT_BUFFER_INSUFFICIENT;
        pDeviceIDs[0] = SL_DEFAULTDEVICEID_AUDIOOUTPUT;
    }
    *pNumDevices = 1;
    return SL_RESULT_SUCCESS;
}

static SLresult OutputMix_RegisterDeviceChangeCallback(SLOutputMixItf self,
    slMixDeviceChangeCallback callback, void *pContext)
{
    struct OutputMix_interface *this = (struct OutputMix_interface *) self;
    interface_lock_exclusive(this);
    this->mCallback = callback;
    this->mContext = pContext;
    interface_unlock_exclusive(this);
    return SL_RESULT_SUCCESS;
}

static SLresult OutputMix_ReRoute(SLOutputMixItf self, SLint32 numOutputDevices,
    SLuint32 *pOutputDeviceIDs)
{
    if (1 > numOutputDevices)
        return SL_RESULT_PARAMETER_INVALID;
    if (NULL == pOutputDeviceIDs)
        return SL_RESULT_PARAMETER_INVALID;
    switch (pOutputDeviceIDs[0]) {
    case SL_DEFAULTDEVICEID_AUDIOOUTPUT:
    case DEVICE_ID_HEADSET:
    case DEVICE_ID_HANDSFREE:
        break;
    default:
        return SL_RESULT_PARAMETER_INVALID;
    }
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
    interface_lock_poke(this);
    this->mPos = pos;
    interface_unlock_poke(this);
    return SL_RESULT_SUCCESS;
}

static SLresult Seek_SetLoop(SLSeekItf self, SLboolean loopEnable,
    SLmillisecond startPos, SLmillisecond endPos)
{
    struct Seek_interface *this = (struct Seek_interface *) self;
    interface_lock_exclusive(this);
    this->mLoopEnabled = loopEnable;
    this->mStartPos = startPos;
    this->mEndPos = endPos;
    interface_unlock_exclusive(this);
    return SL_RESULT_SUCCESS;
}

static SLresult Seek_GetLoop(SLSeekItf self, SLboolean *pLoopEnabled,
    SLmillisecond *pStartPos, SLmillisecond *pEndPos)
{
    if (NULL == pLoopEnabled || NULL == pStartPos || NULL == pEndPos)
        return SL_RESULT_PARAMETER_INVALID;
    struct Seek_interface *this = (struct Seek_interface *) self;
    interface_lock_shared(this);
    SLboolean loopEnabled = this->mLoopEnabled;
    SLmillisecond startPos = this->mStartPos;
    SLmillisecond endPos = this->mEndPos;
    interface_unlock_shared(this);
    *pLoopEnabled = loopEnabled;
    *pStartPos = startPos;
    *pEndPos = endPos;
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
    struct _3DCommit_interface *this = (struct _3DCommit_interface *) self;
    struct Object_interface *thisObject = this->mThis;
    object_lock_exclusive(thisObject);
    if (this->mDeferred) {
        SLuint32 myGeneration = this->mGeneration;
        do
            object_cond_wait(thisObject);
        while (this->mGeneration == myGeneration);
    }
    object_unlock_exclusive(thisObject);
    return SL_RESULT_SUCCESS;
}

static SLresult _3DCommit_SetDeferred(SL3DCommitItf self, SLboolean deferred)
{
    struct _3DCommit_interface *this = (struct _3DCommit_interface *) self;
    struct Object_interface *thisObject = this->mThis;
    object_lock_exclusive(thisObject);
    this->mDeferred = deferred;
    object_unlock_exclusive(thisObject);
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
    SLVec3D velocityCartesian = *pVelocity;
    interface_lock_exclusive(this);
    this->mVelocityCartesian = velocityCartesian;
    this->mVelocityActive = CARTESIAN_SET_SPHERICAL_UNKNOWN;
    interface_unlock_exclusive(this);
    return SL_RESULT_SUCCESS;
}

static SLresult _3DDoppler_SetVelocitySpherical(SL3DDopplerItf self,
    SLmillidegree azimuth, SLmillidegree elevation, SLmillimeter speed)
{
    struct _3DDoppler_interface *this = (struct _3DDoppler_interface *) self;
    interface_lock_exclusive(this);
    this->mVelocitySpherical.mAzimuth = azimuth;
    this->mVelocitySpherical.mElevation = elevation;
    this->mVelocitySpherical.mSpeed = speed;
    this->mVelocityActive = CARTESIAN_UNKNOWN_SPHERICAL_SET;
    interface_unlock_exclusive(this);
    return SL_RESULT_SUCCESS;
}

static SLresult _3DDoppler_GetVelocityCartesian(SL3DDopplerItf self,
    SLVec3D *pVelocity)
{
    if (NULL == pVelocity)
        return SL_RESULT_PARAMETER_INVALID;
    struct _3DDoppler_interface *this = (struct _3DDoppler_interface *) self;
    interface_lock_exclusive(this);
    for (;;) {
        enum CartesianSphericalActive velocityActive = this->mVelocityActive;
        switch (velocityActive) {
        case CARTESIAN_COMPUTED_SPHERICAL_SET:
        case CARTESIAN_SET_SPHERICAL_COMPUTED:  // not in 1.0.1
        case CARTESIAN_SET_SPHERICAL_REQUESTED: // not in 1.0.1
        case CARTESIAN_SET_SPHERICAL_UNKNOWN:
            {
            SLVec3D velocityCartesian = this->mVelocityCartesian;
            interface_unlock_exclusive(this);
            *pVelocity = velocityCartesian;
            }
            break;
        case CARTESIAN_UNKNOWN_SPHERICAL_SET:
            this->mVelocityActive = CARTESIAN_REQUESTED_SPHERICAL_SET;
            // fall through
        case CARTESIAN_REQUESTED_SPHERICAL_SET:
            // matched by cond_broadcast in case multiple requesters
            interface_cond_wait(this);
            continue;
        default:
            assert(SL_BOOLEAN_FALSE);
            interface_unlock_exclusive(this);
            pVelocity->x = 0;
            pVelocity->y = 0;
            pVelocity->z = 0;
            break;
        }
    }
    return SL_RESULT_SUCCESS;
}

static SLresult _3DDoppler_SetDopplerFactor(SL3DDopplerItf self,
    SLpermille dopplerFactor)
{
    struct _3DDoppler_interface *this = (struct _3DDoppler_interface *) self;
    interface_lock_poke(this);
    this->mDopplerFactor = dopplerFactor;
    interface_unlock_poke(this);
    return SL_RESULT_SUCCESS;
}

static SLresult _3DDoppler_GetDopplerFactor(SL3DDopplerItf self,
    SLpermille *pDopplerFactor)
{
    if (NULL == pDopplerFactor)
        return SL_RESULT_PARAMETER_INVALID;
    struct _3DDoppler_interface *this = (struct _3DDoppler_interface *) self;
    interface_lock_peek(this);
    SLpermille dopplerFactor = this->mDopplerFactor;
    interface_unlock_peek(this);
    *pDopplerFactor = dopplerFactor;
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
    SLVec3D locationCartesian = *pLocation;
    interface_lock_exclusive(this);
    this->mLocationCartesian = locationCartesian;
    this->mLocationActive = CARTESIAN_SET_SPHERICAL_UNKNOWN;
    interface_unlock_exclusive(this);
    return SL_RESULT_SUCCESS;
}

static SLresult _3DLocation_SetLocationSpherical(SL3DLocationItf self,
    SLmillidegree azimuth, SLmillidegree elevation, SLmillimeter distance)
{
    struct _3DLocation_interface *this = (struct _3DLocation_interface *) self;
    interface_lock_exclusive(this);
    this->mLocationSpherical.mAzimuth = azimuth;
    this->mLocationSpherical.mElevation = elevation;
    this->mLocationSpherical.mDistance = distance;
    this->mLocationActive = CARTESIAN_UNKNOWN_SPHERICAL_SET;
    interface_unlock_exclusive(this);
    return SL_RESULT_SUCCESS;
}

static SLresult _3DLocation_Move(SL3DLocationItf self, const SLVec3D *pMovement)
{
    if (NULL == pMovement)
        return SL_RESULT_PARAMETER_INVALID;
    struct _3DLocation_interface *this = (struct _3DLocation_interface *) self;
    SLVec3D movementCartesian = *pMovement;
    interface_lock_exclusive(this);
    for (;;) {
        enum CartesianSphericalActive locationActive = this->mLocationActive;
        switch (locationActive) {
        case CARTESIAN_COMPUTED_SPHERICAL_SET:
        case CARTESIAN_SET_SPHERICAL_COMPUTED:  // not in 1.0.1
        case CARTESIAN_SET_SPHERICAL_REQUESTED: // not in 1.0.1
        case CARTESIAN_SET_SPHERICAL_UNKNOWN:
            this->mLocationCartesian.x += movementCartesian.x;
            this->mLocationCartesian.y += movementCartesian.y;
            this->mLocationCartesian.z += movementCartesian.z;
            this->mLocationActive = CARTESIAN_SET_SPHERICAL_UNKNOWN;
            break;
        case CARTESIAN_UNKNOWN_SPHERICAL_SET:
            this->mLocationActive = CARTESIAN_REQUESTED_SPHERICAL_SET;
            // fall through
        case CARTESIAN_REQUESTED_SPHERICAL_SET:
            // matched by cond_broadcast in case multiple requesters
            interface_cond_wait(this);
            continue;
        default:
            assert(SL_BOOLEAN_FALSE);
            break;
        }
        break;
    }
    interface_unlock_exclusive(this);
    return SL_RESULT_SUCCESS;
}

static SLresult _3DLocation_GetLocationCartesian(SL3DLocationItf self,
    SLVec3D *pLocation)
{
    if (NULL == pLocation)
        return SL_RESULT_PARAMETER_INVALID;
    struct _3DLocation_interface *this = (struct _3DLocation_interface *) self;
    interface_lock_exclusive(this);
    for (;;) {
        enum CartesianSphericalActive locationActive = this->mLocationActive;
        switch (locationActive) {
        case CARTESIAN_COMPUTED_SPHERICAL_SET:
        case CARTESIAN_SET_SPHERICAL_COMPUTED:  // not in 1.0.1
        case CARTESIAN_SET_SPHERICAL_REQUESTED: // not in 1.0.1
        case CARTESIAN_SET_SPHERICAL_UNKNOWN:
            {
            SLVec3D locationCartesian = this->mLocationCartesian;
            interface_unlock_exclusive(this);
            *pLocation = locationCartesian;
            }
            break;
        case CARTESIAN_UNKNOWN_SPHERICAL_SET:
            this->mLocationActive = CARTESIAN_REQUESTED_SPHERICAL_SET;
            // fall through
        case CARTESIAN_REQUESTED_SPHERICAL_SET:
            // matched by cond_broadcast in case multiple requesters
            interface_cond_wait(this);
            continue;
        default:
            assert(SL_BOOLEAN_FALSE);
            interface_unlock_exclusive(this);
            pLocation->x = 0;
            pLocation->y = 0;
            pLocation->z = 0;
            break;
        }
    }
    return SL_RESULT_SUCCESS;
}

static SLresult _3DLocation_SetOrientationVectors(SL3DLocationItf self,
    const SLVec3D *pFront, const SLVec3D *pAbove)
{
    if (NULL == pFront || NULL == pAbove)
        return SL_RESULT_PARAMETER_INVALID;
    SLVec3D front = *pFront;
    SLVec3D above = *pAbove;
    struct _3DLocation_interface *this = (struct _3DLocation_interface *) self;
    interface_lock_exclusive(this);
    this->mOrientationVectors.mFront = front;
    this->mOrientationVectors.mAbove = above;
    this->mOrientationActive = ANGLES_UNKNOWN_VECTORS_SET;
    this->mRotatePending = SL_BOOLEAN_FALSE;
    interface_unlock_exclusive(this);
    return SL_RESULT_SUCCESS;
}

static SLresult _3DLocation_SetOrientationAngles(SL3DLocationItf self,
    SLmillidegree heading, SLmillidegree pitch, SLmillidegree roll)
{
    struct _3DLocation_interface *this = (struct _3DLocation_interface *) self;
    interface_lock_exclusive(this);
    this->mOrientationAngles.mHeading = heading;
    this->mOrientationAngles.mPitch = pitch;
    this->mOrientationAngles.mRoll = roll;
    this->mOrientationActive = ANGLES_SET_VECTORS_UNKNOWN;
    this->mRotatePending = SL_BOOLEAN_FALSE;
    interface_unlock_exclusive(this);
    return SL_RESULT_SUCCESS;
}

static SLresult _3DLocation_Rotate(SL3DLocationItf self, SLmillidegree theta,
    const SLVec3D *pAxis)
{
    if (NULL == pAxis)
        return SL_RESULT_PARAMETER_INVALID;
    SLVec3D axis = *pAxis;
    struct _3DLocation_interface *this = (struct _3DLocation_interface *) self;
    interface_lock_exclusive(this);
    while (this->mRotatePending)
        interface_cond_wait(this);
    this->mTheta = theta;
    this->mAxis = axis;
    this->mRotatePending = SL_BOOLEAN_TRUE;
    interface_unlock_exclusive(this);
    return SL_RESULT_SUCCESS;
}

static SLresult _3DLocation_GetOrientationVectors(SL3DLocationItf self,
    SLVec3D *pFront, SLVec3D *pUp)
{
    if (NULL == pFront || NULL == pUp)
        return SL_RESULT_PARAMETER_INVALID;
    struct _3DLocation_interface *this = (struct _3DLocation_interface *) self;
    interface_lock_shared(this);
    SLVec3D front = this->mOrientationVectors.mFront;
    SLVec3D up = this->mOrientationVectors.mUp;
    interface_unlock_shared(this);
    *pFront = front;
    *pUp = up;
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

// These are not in 1.0.1 header file
#define SL_AUDIOCODEC_NULL   0
#define SL_AUDIOCODEC_VORBIS 9

// FIXME should build this table from Caps table below
static const SLuint32 Decoder_IDs[] = {
    SL_AUDIOCODEC_PCM,
    SL_AUDIOCODEC_MP3,
    SL_AUDIOCODEC_AMR,
    SL_AUDIOCODEC_AMRWB,
    SL_AUDIOCODEC_AMRWBPLUS,
    SL_AUDIOCODEC_AAC,
    SL_AUDIOCODEC_WMA,
    SL_AUDIOCODEC_REAL,
    SL_AUDIOCODEC_VORBIS
};
#define MAX_DECODERS (sizeof(Decoder_IDs) / sizeof(Decoder_IDs[0]))

// For now, but encoders might be different than decoders later
#define Encoder_IDs Decoder_IDs
#define MAX_ENCODERS (sizeof(Encoder_IDs) / sizeof(Encoder_IDs[0]))

static const SLmilliHertz SamplingRates_A[] = {
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

static const SLAudioCodecDescriptor CodecDescriptor_A = {
    2,                   // maxChannels
    8,                   // minBitsPerSample
    16,                  // maxBitsPerSample
    SL_SAMPLINGRATE_8,   // minSampleRate
    SL_SAMPLINGRATE_48,  // maxSampleRate
    SL_BOOLEAN_FALSE,    // isFreqRangeContinuous
    (SLmilliHertz *) SamplingRates_A,
                         // pSampleRatesSupported;
    sizeof(SamplingRates_A) / sizeof(SamplingRates_A[0]),
                         // numSampleRatesSupported
    1,                   // minBitRate
    ~0,                  // maxBitRate
    SL_BOOLEAN_TRUE,     // isBitrateRangeContinuous
    NULL,                // pBitratesSupported
    0,                   // numBitratesSupported
    SL_AUDIOPROFILE_PCM, // profileSetting
    0                    // modeSetting
};

static const struct CodecDescriptor {
    SLuint32 mCodecID;
    const SLAudioCodecDescriptor *mDescriptor;
} DecoderDescriptors[] = {
    {SL_AUDIOCODEC_PCM, &CodecDescriptor_A},
    {SL_AUDIOCODEC_NULL, NULL}
}, EncoderDescriptors[] = {
    {SL_AUDIOCODEC_PCM, &CodecDescriptor_A},
    {SL_AUDIOCODEC_NULL, NULL}
};

static SLresult AudioDecoderCapabilities_GetAudioDecoders(
    SLAudioDecoderCapabilitiesItf self, SLuint32 *pNumDecoders,
    SLuint32 *pDecoderIds)
{
    if (NULL == pNumDecoders)
        return SL_RESULT_PARAMETER_INVALID;
    if (NULL == pDecoderIds) {
        *pNumDecoders = MAX_DECODERS;
    } else {
        SLuint32 numDecoders = *pNumDecoders;
        if (MAX_DECODERS <= numDecoders)
            *pNumDecoders = numDecoders = MAX_DECODERS;
        memcpy(pDecoderIds, Decoder_IDs, numDecoders * sizeof(SLuint32));
    }
    return SL_RESULT_SUCCESS;
}

// private helper shared by decoder and encoder
static SLresult GetCodecCapabilities(SLuint32 decoderId, SLuint32 *pIndex,
    SLAudioCodecDescriptor *pDescriptor,
    const struct CodecDescriptor *codecDescriptors)
{
    if (NULL == pIndex)
        return SL_RESULT_PARAMETER_INVALID;
    const struct CodecDescriptor *cd = codecDescriptors;
    SLuint32 index;
    if (NULL == pDescriptor) {
        for (index = 0 ; NULL != cd->mDescriptor; ++cd)
            if (cd->mCodecID == decoderId)
                ++index;
        *pIndex = index;
        return SL_RESULT_SUCCESS;
    }
    index = *pIndex;
    for ( ; NULL != cd->mDescriptor; ++cd) {
        if (cd->mCodecID == decoderId) {
            if (0 == index) {
                *pDescriptor = *cd->mDescriptor;
                return SL_RESULT_SUCCESS;
            }
            --index;
        }
    }
    return SL_RESULT_PARAMETER_INVALID;
}

static SLresult AudioDecoderCapabilities_GetAudioDecoderCapabilities(
    SLAudioDecoderCapabilitiesItf self, SLuint32 decoderId, SLuint32 *pIndex,
    SLAudioCodecDescriptor *pDescriptor)
{
    return GetCodecCapabilities(decoderId, pIndex, pDescriptor,
        DecoderDescriptors);
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
    SLAudioEncoderSettings settings = *pSettings;
    interface_lock_exclusive(this);
    this->mSettings = settings;
    interface_unlock_exclusive(this);
    return SL_RESULT_SUCCESS;
}

static SLresult AudioEncoder_GetEncoderSettings(SLAudioEncoderItf self,
    SLAudioEncoderSettings *pSettings)
{
    if (NULL == pSettings)
        return SL_RESULT_PARAMETER_INVALID;
    struct AudioEncoder_interface *this =
        (struct AudioEncoder_interface *) self;
    interface_lock_shared(this);
    SLAudioEncoderSettings settings = this->mSettings;
    interface_unlock_shared(this);
    *pSettings = settings;
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
    if (NULL == pNumEncoders)
        return SL_RESULT_PARAMETER_INVALID;
    if (NULL == pEncoderIds) {
        *pNumEncoders = MAX_ENCODERS;
    } else {
        SLuint32 numEncoders = *pNumEncoders;
        if (MAX_ENCODERS <= numEncoders)
            *pNumEncoders = numEncoders = MAX_ENCODERS;
        memcpy(pEncoderIds, Encoder_IDs, numEncoders * sizeof(SLuint32));
    }
    return SL_RESULT_SUCCESS;
}

static SLresult AudioEncoderCapabilities_GetAudioEncoderCapabilities(
    SLAudioEncoderCapabilitiesItf self, SLuint32 encoderId, SLuint32 *pIndex,
    SLAudioCodecDescriptor *pDescriptor)
{
    return GetCodecCapabilities(encoderId, pIndex, pDescriptor,
        EncoderDescriptors);
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
    switch (deviceID) {
    case SL_DEFAULTDEVICEID_AUDIOINPUT:
    case SL_DEFAULTDEVICEID_AUDIOOUTPUT:
        break;
    default:
        return SL_RESULT_PARAMETER_INVALID;
    }
    struct DeviceVolume_interface *this =
        (struct DeviceVolume_interface *) self;
    interface_lock_poke(this);
    this->mVolume[~deviceID] = volume;
    interface_unlock_poke(this);
    return SL_RESULT_SUCCESS;
}

static SLresult DeviceVolume_GetVolume(SLDeviceVolumeItf self,
    SLuint32 deviceID, SLint32 *pVolume)
{
    if (NULL == pVolume)
        return SL_RESULT_PARAMETER_INVALID;
    switch (deviceID) {
    case SL_DEFAULTDEVICEID_AUDIOINPUT:
    case SL_DEFAULTDEVICEID_AUDIOOUTPUT:
        break;
    default:
        return SL_RESULT_PARAMETER_INVALID;
    }
    struct DeviceVolume_interface *this =
        (struct DeviceVolume_interface *) self;
    interface_lock_peek(this);
    SLint32 volume = this->mVolume[~deviceID];
    interface_unlock_peek(this);
    *pVolume = volume;
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
    if (NULL == pDataSource)
        return SL_RESULT_PARAMETER_INVALID;
    struct DynamicSource_interface *this = (struct DynamicSource_interface *) self;
    // need to lock the object, as a change to source can impact most of object
    struct Object_interface *thisObject = this->mThis;
    object_lock_exclusive(thisObject);
    // FIXME a bit of a simplification to say the least!
    this->mDataSource = pDataSource;
    object_unlock_exclusive(thisObject);
    return SL_RESULT_SUCCESS;
}

/*static*/ const struct SLDynamicSourceItf_ DynamicSource_DynamicSourceItf = {
    DynamicSource_SetSource
};

/* EffectSend implementation */

static struct EnableLevel *getEnableLevel(struct EffectSend_interface *this,
    const void *pAuxEffect)
{
    struct OutputMix_class *outputMix = this->mOutputMix;
    // Make sure the sink for this player is an output mix
#if 0 // not necessary
    if (NULL == outputMix)
        return NULL;
#endif
    if (pAuxEffect == &outputMix->mEnvironmentalReverb)
        return &this->mEnableLevels[AUX_ENVIRONMENTALREVERB];
    if (pAuxEffect == &outputMix->mPresetReverb)
        return &this->mEnableLevels[AUX_PRESETREVERB];
    return NULL;
#if 0 // App couldn't have an interface for effect without exposure
    unsigned interfaceMask = 1 << MPH_to_OutputMix[AUX_to_MPH[aux]];
    if (outputMix->mExposedMask & interfaceMask)
        result = SL_RESULT_PARAMETER_INVALID;
#endif
}

static SLresult EffectSend_EnableEffectSend(SLEffectSendItf self,
    const void *pAuxEffect, SLboolean enable, SLmillibel initialLevel)
{
    struct EffectSend_interface *this = (struct EffectSend_interface *) self;
    struct EnableLevel *enableLevel = getEnableLevel(this, pAuxEffect);
    if (NULL == enableLevel)
        return SL_RESULT_PARAMETER_INVALID;
    interface_lock_exclusive(this);
    enableLevel->mEnable = enable;
    enableLevel->mSendLevel = initialLevel;
    interface_unlock_exclusive(this);
    return SL_RESULT_SUCCESS;
}

static SLresult EffectSend_IsEnabled(SLEffectSendItf self,
    const void *pAuxEffect, SLboolean *pEnable)
{
    if (NULL == pEnable)
        return SL_RESULT_PARAMETER_INVALID;
    struct EffectSend_interface *this = (struct EffectSend_interface *) self;
    struct EnableLevel *enableLevel = getEnableLevel(this, pAuxEffect);
    if (NULL == enableLevel)
        return SL_RESULT_PARAMETER_INVALID;
    interface_lock_peek(this);
    SLboolean enable = enableLevel->mEnable;
    interface_unlock_peek(this);
    *pEnable = enable;
    return SL_RESULT_SUCCESS;
}

static SLresult EffectSend_SetDirectLevel(SLEffectSendItf self,
    SLmillibel directLevel)
{
    struct EffectSend_interface *this = (struct EffectSend_interface *) self;
    interface_lock_poke(this);
    this->mDirectLevel = directLevel;
    interface_unlock_poke(this);
    return SL_RESULT_SUCCESS;
}

static SLresult EffectSend_GetDirectLevel(SLEffectSendItf self,
    SLmillibel *pDirectLevel)
{
    if (NULL == pDirectLevel)
        return SL_RESULT_PARAMETER_INVALID;
    struct EffectSend_interface *this = (struct EffectSend_interface *) self;
    interface_lock_peek(this);
    SLmillibel directLevel = this->mDirectLevel;
    interface_unlock_peek(this);
    *pDirectLevel = directLevel;
    return SL_RESULT_SUCCESS;
}

static SLresult EffectSend_SetSendLevel(SLEffectSendItf self,
    const void *pAuxEffect, SLmillibel sendLevel)
{
    struct EffectSend_interface *this = (struct EffectSend_interface *) self;
    struct EnableLevel *enableLevel = getEnableLevel(this, pAuxEffect);
    if (NULL == enableLevel)
        return SL_RESULT_PARAMETER_INVALID;
    // EnableEffectSend is exclusive, so this has to be also
    interface_lock_exclusive(this);
    enableLevel->mSendLevel = sendLevel;
    interface_unlock_exclusive(this);
    return SL_RESULT_SUCCESS;
}

static SLresult EffectSend_GetSendLevel(SLEffectSendItf self,
    const void *pAuxEffect, SLmillibel *pSendLevel)
{
    if (NULL == pSendLevel)
        return SL_RESULT_PARAMETER_INVALID;
    struct EffectSend_interface *this = (struct EffectSend_interface *) self;
    struct EnableLevel *enableLevel = getEnableLevel(this, pAuxEffect);
    if (NULL == enableLevel)
        return SL_RESULT_PARAMETER_INVALID;
    interface_lock_peek(this);
    SLmillibel sendLevel = enableLevel->mSendLevel;
    interface_unlock_peek(this);
    *pSendLevel = sendLevel;
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
    // This omits the unofficial driver profile
    *pProfilesSupported =
        SL_PROFILES_PHONE | SL_PROFILES_MUSIC | SL_PROFILES_GAME;
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
        *pNumMaxVoices = 32;
    if (NULL != pIsAbsoluteMax)
        *pIsAbsoluteMax = SL_BOOLEAN_TRUE;
    if (NULL != pNumFreeVoices)
        *pNumFreeVoices = 32;
    return SL_RESULT_SUCCESS;
}

static SLresult EngineCapabilities_QueryNumberOfMIDISynthesizers(
    SLEngineCapabilitiesItf self, SLint16 *pNum)
{
    if (NULL == pNum)
        return SL_RESULT_PARAMETER_INVALID;
    *pNum = 1;
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
    SLuint32 maxIndex = sizeof(LED_id_descriptors) /
        sizeof(LED_id_descriptors[0]);
    const struct LED_id_descriptor *id_descriptor;
    SLuint32 index;
    if (NULL != pIndex) {
        if (NULL != pLEDDeviceID || NULL != pDescriptor) {
            index = *pIndex;
            if (index >= maxIndex)
                return SL_RESULT_PARAMETER_INVALID;
            id_descriptor = &LED_id_descriptors[index];
            if (NULL != pLEDDeviceID)
                *pLEDDeviceID = id_descriptor->id;
            if (NULL != pDescriptor)
                *pDescriptor = *id_descriptor->descriptor;
        }
        /* FIXME else? */
        *pIndex = maxIndex;
        return SL_RESULT_SUCCESS;
    } else {
        if (NULL != pLEDDeviceID && NULL != pDescriptor) {
            SLuint32 id = *pLEDDeviceID;
            for (index = 0; index < maxIndex; ++index) {
                id_descriptor = &LED_id_descriptors[index];
                if (id == id_descriptor->id) {
                    *pDescriptor = *id_descriptor->descriptor;
                    return SL_RESULT_SUCCESS;
                }
            }
        }
        return SL_RESULT_PARAMETER_INVALID;
    }
}

static SLresult EngineCapabilities_QueryVibraCapabilities(
    SLEngineCapabilitiesItf self, SLuint32 *pIndex, SLuint32 *pVibraDeviceID,
    SLVibraDescriptor *pDescriptor)
{
    SLuint32 maxIndex = sizeof(Vibra_id_descriptors) /
        sizeof(Vibra_id_descriptors[0]);
    const struct Vibra_id_descriptor *id_descriptor;
    SLuint32 index;
    if (NULL != pIndex) {
        if (NULL != pVibraDeviceID || NULL != pDescriptor) {
            index = *pIndex;
            if (index >= maxIndex)
                return SL_RESULT_PARAMETER_INVALID;
            id_descriptor = &Vibra_id_descriptors[index];
            if (NULL != pVibraDeviceID)
                *pVibraDeviceID = id_descriptor->id;
            if (NULL != pDescriptor)
                *pDescriptor = *id_descriptor->descriptor;
        }
        /* FIXME else? */
        *pIndex = maxIndex;
        return SL_RESULT_SUCCESS;
    } else {
        if (NULL != pVibraDeviceID && NULL != pDescriptor) {
            SLuint32 id = *pVibraDeviceID;
            for (index = 0; index < maxIndex; ++index) {
                id_descriptor = &Vibra_id_descriptors[index];
                if (id == id_descriptor->id) {
                    *pDescriptor = *id_descriptor->descriptor;
                    return SL_RESULT_SUCCESS;
                }
            }
        }
        return SL_RESULT_PARAMETER_INVALID;
    }
    return SL_RESULT_SUCCESS;
}

static SLresult EngineCapabilities_IsThreadSafe(SLEngineCapabilitiesItf self,
    SLboolean *pIsThreadSafe)
{
    if (NULL == pIsThreadSafe)
        return SL_RESULT_PARAMETER_INVALID;
    struct EngineCapabilities_interface *this =
        (struct EngineCapabilities_interface *) self;
    *pIsThreadSafe = this->mThreadSafe;
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
    struct LEDArray_interface *this = (struct LEDArray_interface *) self;
    interface_lock_poke(this);
    this->mLightMask = lightMask;
    interface_unlock_poke(this);
    return SL_RESULT_SUCCESS;
}

static SLresult LEDArray_IsLEDArrayActivated(SLLEDArrayItf self,
    SLuint32 *pLightMask)
{
    if (NULL == pLightMask)
        return SL_RESULT_PARAMETER_INVALID;
    struct LEDArray_interface *this = (struct LEDArray_interface *) self;
    interface_lock_peek(this);
    SLuint32 lightMask = this->mLightMask;
    interface_unlock_peek(this);
    *pLightMask = lightMask;
    return SL_RESULT_SUCCESS;
}

static SLresult LEDArray_SetColor(SLLEDArrayItf self, SLuint8 index,
    const SLHSL *pColor)
{
    if (NULL == pColor)
        return SL_RESULT_PARAMETER_INVALID;
    SLHSL color = *pColor;
    struct LEDArray_interface *this = (struct LEDArray_interface *) self;
    interface_lock_poke(this);
    this->mColor[index] = color;
    interface_unlock_poke(this);
    return SL_RESULT_SUCCESS;
}

static SLresult LEDArray_GetColor(SLLEDArrayItf self, SLuint8 index,
    SLHSL *pColor)
{
    if (NULL == pColor)
        return SL_RESULT_PARAMETER_INVALID;
    struct LEDArray_interface *this = (struct LEDArray_interface *) self;
    interface_lock_peek(this);
    SLHSL color = this->mColor[index];
    interface_unlock_peek(this);
    *pColor = color;
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
    //struct MetadataExtraction_interface *this = (struct MetadataExtraction_interface *) self;
    if (NULL == pItemCount)
        return SL_RESULT_PARAMETER_INVALID;
    *pItemCount = 0;
    return SL_RESULT_SUCCESS;
}

static SLresult MetadataExtraction_GetKeySize(SLMetadataExtractionItf self,
    SLuint32 index, SLuint32 *pKeySize)
{
    //struct MetadataExtraction_interface *this = (struct MetadataExtraction_interface *) self;
    if (NULL == pKeySize)
        return SL_RESULT_PARAMETER_INVALID;
    *pKeySize = 0;
    return SL_RESULT_SUCCESS;
}

static SLresult MetadataExtraction_GetKey(SLMetadataExtractionItf self,
    SLuint32 index, SLuint32 keySize, SLMetadataInfo *pKey)
{
    //struct MetadataExtraction_interface *this = (struct MetadataExtraction_interface *) self;
    if (NULL == pKey)
        return SL_RESULT_PARAMETER_INVALID;
    SLMetadataInfo metadataInfo;
    *pKey = metadataInfo;
    return SL_RESULT_SUCCESS;
}

static SLresult MetadataExtraction_GetValueSize(SLMetadataExtractionItf self,
    SLuint32 index, SLuint32 *pValueSize)
{
    //struct MetadataExtraction_interface *this = (struct MetadataExtraction_interface *) self;
    if (NULL == pValueSize)
        return SL_RESULT_PARAMETER_INVALID;
    *pValueSize = 0;
    return SL_RESULT_SUCCESS;
}

static SLresult MetadataExtraction_GetValue(SLMetadataExtractionItf self,
    SLuint32 index, SLuint32 valueSize, SLMetadataInfo *pValue)
{
    //struct MetadataExtraction_interface *this = (struct MetadataExtraction_interface *) self;
    if (NULL == pValue)
        return SL_RESULT_PARAMETER_INVALID;
    SLMetadataInfo value;
    *pValue = value;;
    return SL_RESULT_SUCCESS;
}

static SLresult MetadataExtraction_AddKeyFilter(SLMetadataExtractionItf self,
    SLuint32 keySize, const void *pKey, SLuint32 keyEncoding,
    const SLchar *pValueLangCountry, SLuint32 valueEncoding, SLuint8 filterMask)
{
    if (NULL == pKey || NULL == pValueLangCountry)
        return SL_RESULT_PARAMETER_INVALID;
    struct MetadataExtraction_interface *this = (struct MetadataExtraction_interface *) self;
    interface_lock_exclusive(this);
    this->mKeySize = keySize;
    this->mKey = pKey;
    this->mKeyEncoding = keyEncoding;
    this->mValueLangCountry = pValueLangCountry; // FIXME local copy?
    this->mValueEncoding = valueEncoding;
    this->mFilterMask = filterMask;
    interface_unlock_exclusive(this);
    return SL_RESULT_SUCCESS;
}

static SLresult MetadataExtraction_ClearKeyFilter(SLMetadataExtractionItf self)
{
    struct MetadataExtraction_interface *this = (struct MetadataExtraction_interface *) self;
    this->mKeyFilter = 0;
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
    struct MetadataTraversal_interface *this = (struct MetadataTraversal_interface *) self;
    this->mMode = mode;
    return SL_RESULT_SUCCESS;
}

static SLresult MetadataTraversal_GetChildCount(SLMetadataTraversalItf self,
    SLuint32 *pCount)
{
    if (NULL == pCount)
        return SL_RESULT_PARAMETER_INVALID;
    struct MetadataTraversal_interface *this = (struct MetadataTraversal_interface *) self;
    *pCount = this->mCount;
    return SL_RESULT_SUCCESS;
}

static SLresult MetadataTraversal_GetChildMIMETypeSize(
    SLMetadataTraversalItf self, SLuint32 index, SLuint32 *pSize)
{
    if (NULL == pSize)
        return SL_RESULT_PARAMETER_INVALID;
    struct MetadataTraversal_interface *this = (struct MetadataTraversal_interface *) self;
    *pSize = this->mSize;
    return SL_RESULT_SUCCESS;
}

static SLresult MetadataTraversal_GetChildInfo(SLMetadataTraversalItf self,
    SLuint32 index, SLint32 *pNodeID, SLuint32 *pType, SLuint32 size,
    SLchar *pMimeType)
{
    //struct MetadataTraversal_interface *this = (struct MetadataTraversal_interface *) self;
    return SL_RESULT_SUCCESS;
}

static SLresult MetadataTraversal_SetActiveNode(SLMetadataTraversalItf self,
    SLuint32 index)
{
    struct MetadataTraversal_interface *this = (struct MetadataTraversal_interface *) self;
    this->mIndex = index;
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
    struct MuteSolo_interface *this = (struct MuteSolo_interface *) self;
    SLuint32 mask = 1 << chan;
    interface_lock_exclusive(this);
    if (mute)
        this->mMuteMask |= mask;
    else
        this->mMuteMask &= ~mask;
    interface_unlock_exclusive(this);
    return SL_RESULT_SUCCESS;
}

static SLresult MuteSolo_GetChannelMute(SLMuteSoloItf self, SLuint8 chan,
    SLboolean *pMute)
{
    if (NULL == pMute)
        return SL_RESULT_PARAMETER_INVALID;
    struct MuteSolo_interface *this = (struct MuteSolo_interface *) self;
    interface_lock_peek(this);
    SLuint32 mask = this->mMuteMask;
    interface_unlock_peek(this);
    *pMute = (mask >> chan) & 1;
    return SL_RESULT_SUCCESS;
}

static SLresult MuteSolo_SetChannelSolo(SLMuteSoloItf self, SLuint8 chan,
    SLboolean solo)
{
    struct MuteSolo_interface *this = (struct MuteSolo_interface *) self;
    SLuint32 mask = 1 << chan;
    interface_lock_exclusive(this);
    if (solo)
        this->mSoloMask |= mask;
    else
        this->mSoloMask &= ~mask;
    interface_unlock_exclusive(this);
    return SL_RESULT_SUCCESS;
}

static SLresult MuteSolo_GetChannelSolo(SLMuteSoloItf self, SLuint8 chan,
    SLboolean *pSolo)
{
    if (NULL == pSolo)
        return SL_RESULT_PARAMETER_INVALID;
    struct MuteSolo_interface *this = (struct MuteSolo_interface *) self;
    interface_lock_peek(this);
    SLuint32 mask = this->mSoloMask;
    interface_unlock_peek(this);
    *pSolo = (mask >> chan) & 1;
    return SL_RESULT_SUCCESS;
}

static SLresult MuteSolo_GetNumChannels(SLMuteSoloItf self,
    SLuint8 *pNumChannels)
{
    if (NULL == pNumChannels)
        return SL_RESULT_PARAMETER_INVALID;
    struct MuteSolo_interface *this = (struct MuteSolo_interface *) self;
    // FIXME const
    SLuint32 numChannels = this->mNumChannels;
    *pNumChannels = numChannels;
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
    struct Pitch_interface *this = (struct Pitch_interface *) self;
    interface_lock_poke(this);
    this->mPitch = pitch;
    interface_unlock_poke(this);
    return SL_RESULT_SUCCESS;
}

static SLresult Pitch_GetPitch(SLPitchItf self, SLpermille *pPitch)
{
    if (NULL == pPitch)
        return SL_RESULT_PARAMETER_INVALID;
    struct Pitch_interface *this = (struct Pitch_interface *) self;
    interface_lock_peek(this);
    SLpermille pitch = this->mPitch;
    interface_unlock_peek(this);
    *pPitch = pitch;
    return SL_RESULT_SUCCESS;
}

static SLresult Pitch_GetPitchCapabilities(SLPitchItf self,
    SLpermille *pMinPitch, SLpermille *pMaxPitch)
{
    if (NULL == pMinPitch || NULL == pMaxPitch)
        return SL_RESULT_PARAMETER_INVALID;
    struct Pitch_interface *this = (struct Pitch_interface *) self;
    SLpermille minPitch = this->mMinPitch;
    SLpermille maxPitch = this->mMaxPitch;
    *pMinPitch = minPitch;
    *pMaxPitch = maxPitch;
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
    struct PlaybackRate_interface *this = (struct PlaybackRate_interface *) self;
    interface_lock_poke(this);
    this->mRate = rate;
    interface_unlock_poke(this);
    return SL_RESULT_SUCCESS;
}

static SLresult PlaybackRate_GetRate(SLPlaybackRateItf self, SLpermille *pRate)
{
    if (NULL == pRate)
        return SL_RESULT_PARAMETER_INVALID;
    struct PlaybackRate_interface *this = (struct PlaybackRate_interface *) self;
    interface_lock_peek(this);
    SLpermille rate = this->mRate;
    interface_unlock_peek(this);
    *pRate = rate;
    return SL_RESULT_SUCCESS;
}

static SLresult PlaybackRate_SetPropertyConstraints(SLPlaybackRateItf self,
    SLuint32 constraints)
{
    struct PlaybackRate_interface *this = (struct PlaybackRate_interface *) self;
    // FIXME are properties and property constraints the same thing?
    this->mProperties = constraints;
    return SL_RESULT_SUCCESS;
}

static SLresult PlaybackRate_GetProperties(SLPlaybackRateItf self,
    SLuint32 *pProperties)
{
    if (NULL == pProperties)
        return SL_RESULT_PARAMETER_INVALID;
    struct PlaybackRate_interface *this = (struct PlaybackRate_interface *) self;
    SLuint32 properties = this->mProperties;
    *pProperties = properties;
    return SL_RESULT_SUCCESS;
}

static SLresult PlaybackRate_GetCapabilitiesOfRate(SLPlaybackRateItf self,
    SLpermille rate, SLuint32 *pCapabilities)
{
    if (NULL == pCapabilities)
        return SL_RESULT_PARAMETER_INVALID;
    // struct PlaybackRate_interface *this = (struct PlaybackRate_interface *) self;
    SLuint32 capabilities = 0; // FIXME
    *pCapabilities = capabilities;
    return SL_RESULT_SUCCESS;
}

static SLresult PlaybackRate_GetRateRange(SLPlaybackRateItf self, SLuint8 index,
    SLpermille *pMinRate, SLpermille *pMaxRate, SLpermille *pStepSize,
    SLuint32 *pCapabilities)
{
    if (NULL == pMinRate || NULL == pMaxRate || NULL == pStepSize || NULL == pCapabilities)
        return SL_RESULT_PARAMETER_INVALID;
    struct PlaybackRate_interface *this = (struct PlaybackRate_interface *) self;
    interface_lock_shared(this);
    SLpermille minRate = this->mMinRate;
    SLpermille maxRate = this->mMaxRate;
    SLpermille stepSize = this->mStepSize;
    SLuint32 capabilities = this->mCapabilities;
    interface_unlock_shared(this);
    *pMinRate = minRate;
    *pMaxRate = maxRate;
    *pStepSize = stepSize;
    *pCapabilities = capabilities;
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
    if (NULL == pStatus)
        return SL_RESULT_PARAMETER_INVALID;
    struct PrefetchStatus_interface *this = (struct PrefetchStatus_interface *) self;
    interface_lock_peek(this);
    SLuint32 status = this->mStatus;
    interface_unlock_peek(this);
    *pStatus = status;
    return SL_RESULT_SUCCESS;
}

static SLresult PrefetchStatus_GetFillLevel(SLPrefetchStatusItf self,
    SLpermille *pLevel)
{
    if (NULL == pLevel)
        return SL_RESULT_PARAMETER_INVALID;
    struct PrefetchStatus_interface *this = (struct PrefetchStatus_interface *) self;
    interface_lock_peek(this);
    SLpermille level = this->mLevel;
    interface_unlock_peek(this);
    *pLevel = level;
    return SL_RESULT_SUCCESS;
}

static SLresult PrefetchStatus_RegisterCallback(SLPrefetchStatusItf self,
    slPrefetchCallback callback, void *pContext)
{
    struct PrefetchStatus_interface *this = (struct PrefetchStatus_interface *) self;
    interface_lock_exclusive(this);
    this->mCallback = callback;
    this->mContext = pContext;
    interface_unlock_exclusive(this);
    return SL_RESULT_SUCCESS;
}

static SLresult PrefetchStatus_SetCallbackEventsMask(SLPrefetchStatusItf self,
    SLuint32 eventFlags)
{
    struct PrefetchStatus_interface *this = (struct PrefetchStatus_interface *) self;
    interface_lock_poke(this);
    this->mCallbackEventsMask = eventFlags;
    interface_unlock_poke(this);
    return SL_RESULT_SUCCESS;
}

static SLresult PrefetchStatus_GetCallbackEventsMask(SLPrefetchStatusItf self,
    SLuint32 *pEventFlags)
{
    if (NULL == pEventFlags)
        return SL_RESULT_PARAMETER_INVALID;
    struct PrefetchStatus_interface *this = (struct PrefetchStatus_interface *) self;
    interface_lock_peek(this);
    SLuint32 callbackEventsMask = this->mCallbackEventsMask;
    interface_unlock_peek(this);
    *pEventFlags = callbackEventsMask;
    return SL_RESULT_SUCCESS;
}

static SLresult PrefetchStatus_SetFillUpdatePeriod(SLPrefetchStatusItf self,
    SLpermille period)
{
    struct PrefetchStatus_interface *this = (struct PrefetchStatus_interface *) self;
    interface_lock_poke(this);
    this->mFillUpdatePeriod = period;
    interface_unlock_poke(this);
    return SL_RESULT_SUCCESS;
}

static SLresult PrefetchStatus_GetFillUpdatePeriod(SLPrefetchStatusItf self,
    SLpermille *pPeriod)
{
    if (NULL == pPeriod)
        return SL_RESULT_PARAMETER_INVALID;
    struct PrefetchStatus_interface *this = (struct PrefetchStatus_interface *) self;
    interface_lock_peek(this);
    SLpermille fillUpdatePeriod = this->mFillUpdatePeriod;
    interface_unlock_peek(this);
    *pPeriod = fillUpdatePeriod;
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
    struct RatePitch_interface *this = (struct RatePitch_interface *) self;
    interface_lock_poke(this);
    this->mRate = rate;
    interface_unlock_poke(this);
    return SL_RESULT_SUCCESS;
}

static SLresult RatePitch_GetRate(SLRatePitchItf self, SLpermille *pRate)
{
    if (NULL == pRate)
        return SL_RESULT_PARAMETER_INVALID;
    struct RatePitch_interface *this = (struct RatePitch_interface *) self;
    interface_lock_peek(this);
    SLpermille rate = this->mRate;
    interface_unlock_peek(this);
    *pRate = rate;
    return SL_RESULT_SUCCESS;
}

static SLresult RatePitch_GetRatePitchCapabilities(SLRatePitchItf self,
    SLpermille *pMinRate, SLpermille *pMaxRate)
{
    if (NULL == pMinRate || NULL == pMaxRate)
        return SL_RESULT_PARAMETER_INVALID;
    struct RatePitch_interface *this = (struct RatePitch_interface *) self;
    // FIXME const, direct access?
    SLpermille minRate = this->mMinRate;
    SLpermille maxRate = this->mMaxRate;
    *pMinRate = minRate;
    *pMaxRate = maxRate;
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
    struct Record_interface *this = (struct Record_interface *) self;
    interface_lock_poke(this);
    this->mState = state;
    interface_unlock_poke(this);
    return SL_RESULT_SUCCESS;
}

static SLresult Record_GetRecordState(SLRecordItf self, SLuint32 *pState)
{
    struct Record_interface *this = (struct Record_interface *) self;
    if (NULL == pState)
        return SL_RESULT_PARAMETER_INVALID;
    interface_lock_peek(this);
    SLuint32 state = this->mState;
    interface_unlock_peek(this);
    *pState = state;
    return SL_RESULT_SUCCESS;
}

static SLresult Record_SetDurationLimit(SLRecordItf self, SLmillisecond msec)
{
    struct Record_interface *this = (struct Record_interface *) self;
    interface_lock_poke(this);
    this->mDurationLimit = msec;
    interface_unlock_poke(this);
    return SL_RESULT_SUCCESS;
}

static SLresult Record_GetPosition(SLRecordItf self, SLmillisecond *pMsec)
{
    if (NULL == pMsec)
        return SL_RESULT_PARAMETER_INVALID;
    struct Record_interface *this = (struct Record_interface *) self;
    interface_lock_peek(this);
    SLmillisecond position = this->mPosition;
    interface_unlock_peek(this);
    *pMsec = position;
    return SL_RESULT_SUCCESS;
}

static SLresult Record_RegisterCallback(SLRecordItf self,
    slRecordCallback callback, void *pContext)
{
    struct Record_interface *this = (struct Record_interface *) self;
    interface_lock_exclusive(this);
    this->mCallback = callback;
    this->mContext = pContext;
    interface_unlock_exclusive(this);
    return SL_RESULT_SUCCESS;
}

static SLresult Record_SetCallbackEventsMask(SLRecordItf self,
    SLuint32 eventFlags)
{
    struct Record_interface *this = (struct Record_interface *) self;
    interface_lock_poke(this);
    this->mCallbackEventsMask = eventFlags;
    interface_unlock_poke(this);
    return SL_RESULT_SUCCESS;
}

static SLresult Record_GetCallbackEventsMask(SLRecordItf self,
    SLuint32 *pEventFlags)
{
    if (NULL == pEventFlags)
        return SL_RESULT_PARAMETER_INVALID;
    struct Record_interface *this = (struct Record_interface *) self;
    interface_lock_peek(this);
    SLuint32 callbackEventsMask = this->mCallbackEventsMask;
    interface_unlock_peek(this);
    *pEventFlags = callbackEventsMask;
    return SL_RESULT_SUCCESS;
}

static SLresult Record_SetMarkerPosition(SLRecordItf self, SLmillisecond mSec)
{
    struct Record_interface *this = (struct Record_interface *) self;
    interface_lock_poke(this);
    this->mMarkerPosition = mSec;
    interface_unlock_poke(this);
    return SL_RESULT_SUCCESS;
}

static SLresult Record_ClearMarkerPosition(SLRecordItf self)
{
    struct Record_interface *this = (struct Record_interface *) self;
    interface_lock_poke(this);
    this->mMarkerPosition = 0;
    interface_unlock_poke(this);
    return SL_RESULT_SUCCESS;
}

static SLresult Record_GetMarkerPosition(SLRecordItf self, SLmillisecond *pMsec)
{
    if (NULL == pMsec)
        return SL_RESULT_PARAMETER_INVALID;
    struct Record_interface *this = (struct Record_interface *) self;
    interface_lock_peek(this);
    SLmillisecond markerPosition = this->mMarkerPosition;
    interface_unlock_peek(this);
    *pMsec = markerPosition;
    return SL_RESULT_SUCCESS;
}

static SLresult Record_SetPositionUpdatePeriod(SLRecordItf self,
    SLmillisecond mSec)
{
    struct Record_interface *this = (struct Record_interface *) self;
    interface_lock_poke(this);
    this->mPositionUpdatePeriod = mSec;
    interface_unlock_poke(this);
    return SL_RESULT_SUCCESS;
}

static SLresult Record_GetPositionUpdatePeriod(SLRecordItf self,
    SLmillisecond *pMsec)
{
    if (NULL == pMsec)
        return SL_RESULT_PARAMETER_INVALID;
    struct Record_interface *this = (struct Record_interface *) self;
    interface_lock_peek(this);
    SLmillisecond positionUpdatePeriod = this->mPositionUpdatePeriod;
    interface_unlock_peek(this);
    *pMsec = positionUpdatePeriod;
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
    struct ThreadSync_interface *this = (struct ThreadSync_interface *) self;
    SLresult result;
    interface_lock_exclusive(this);
    for (;;) {
        if (this->mInCriticalSection) {
            if (this->mOwner != pthread_self()) {
                this->mWaiting = SL_BOOLEAN_TRUE;
                interface_cond_wait(this);
                continue;
            }
            result = SL_RESULT_PRECONDITIONS_VIOLATED;
            break;
        }
        this->mInCriticalSection = SL_BOOLEAN_TRUE;
        this->mOwner = pthread_self();
        result = SL_RESULT_SUCCESS;
        break;
    }
    interface_unlock_exclusive(this);
    return result;
}

static SLresult ThreadSync_ExitCriticalSection(SLThreadSyncItf self)
{
    struct ThreadSync_interface *this = (struct ThreadSync_interface *) self;
    SLresult result;
    interface_lock_exclusive(this);
    if (!this->mInCriticalSection || this->mOwner != pthread_self()) {
        result = SL_RESULT_PRECONDITIONS_VIOLATED;
    } else {
        this->mInCriticalSection = SL_BOOLEAN_FALSE;
        this->mOwner = NULL;
        result = SL_RESULT_SUCCESS;
        if (this->mWaiting) {
            this->mWaiting = SL_BOOLEAN_FALSE;
            interface_cond_signal(this);
        }
    }
    interface_unlock_exclusive(this);
    return result;
}

/*static*/ const struct SLThreadSyncItf_ ThreadSync_ThreadSyncItf = {
    ThreadSync_EnterCriticalSection,
    ThreadSync_ExitCriticalSection
};

/* Vibra implementation */

static SLresult Vibra_Vibrate(SLVibraItf self, SLboolean vibrate)
{
    struct Vibra_interface *this = (struct Vibra_interface *) self;
    interface_lock_poke(this);
    this->mVibrate = vibrate;
    interface_unlock_poke(this);
    return SL_RESULT_SUCCESS;
}

static SLresult Vibra_IsVibrating(SLVibraItf self, SLboolean *pVibrating)
{
    if (NULL == pVibrating)
        return SL_RESULT_PARAMETER_INVALID;
    struct Vibra_interface *this = (struct Vibra_interface *) self;
    interface_lock_peek(this);
    SLboolean vibrate = this->mVibrate;
    interface_unlock_peek(this);
    *pVibrating = vibrate;
    return SL_RESULT_SUCCESS;
}

static SLresult Vibra_SetFrequency(SLVibraItf self, SLmilliHertz frequency)
{
    struct Vibra_interface *this = (struct Vibra_interface *) self;
    interface_lock_poke(this);
    this->mFrequency = frequency;
    interface_unlock_exclusive(this);
    return SL_RESULT_SUCCESS;
}

static SLresult Vibra_GetFrequency(SLVibraItf self, SLmilliHertz *pFrequency)
{
    if (NULL == pFrequency)
        return SL_RESULT_PARAMETER_INVALID;
    struct Vibra_interface *this = (struct Vibra_interface *) self;
    interface_lock_peek(this);
    SLmilliHertz frequency = this->mFrequency;
    interface_unlock_peek(this);
    *pFrequency = frequency;
    return SL_RESULT_SUCCESS;
}

static SLresult Vibra_SetIntensity(SLVibraItf self, SLpermille intensity)
{
    struct Vibra_interface *this = (struct Vibra_interface *) self;
    interface_lock_poke(this);
    this->mIntensity = intensity;
    interface_unlock_poke(this);
    return SL_RESULT_SUCCESS;
}

static SLresult Vibra_GetIntensity(SLVibraItf self, SLpermille *pIntensity)
{
    if (NULL == pIntensity)
        return SL_RESULT_PARAMETER_INVALID;
    struct Vibra_interface *this = (struct Vibra_interface *) self;
    interface_lock_peek(this);
    SLpermille intensity = this->mIntensity;
    interface_unlock_peek(this);
    *pIntensity = intensity;
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
    interface_lock_exclusive(this);
    this->mCallback = callback;
    this->mContext = pContext;
    this->mRate = rate;
    interface_unlock_exclusive(this);
    return SL_RESULT_SUCCESS;
}

static SLresult Visualization_GetMaxRate(SLVisualizationItf self,
    SLmilliHertz *pRate)
{
    if (NULL == pRate)
        return SL_RESULT_PARAMETER_INVALID;
    *pRate = 20000;
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
    interface_lock_poke(this);
    this->mEnabled = enabled;
    interface_unlock_poke(this);
    return SL_RESULT_SUCCESS;
}

static SLresult BassBoost_IsEnabled(SLBassBoostItf self, SLboolean *pEnabled)
{
    if (NULL == pEnabled)
        return SL_RESULT_PARAMETER_INVALID;
    struct BassBoost_interface *this = (struct BassBoost_interface *) self;
    interface_lock_peek(this);
    SLboolean enabled = this->mEnabled;
    interface_unlock_peek(this);
    *pEnabled = enabled;
    return SL_RESULT_SUCCESS;
}

static SLresult BassBoost_SetStrength(SLBassBoostItf self, SLpermille strength)
{
    struct BassBoost_interface *this = (struct BassBoost_interface *) self;
    interface_lock_poke(this);
    this->mStrength = strength;
    interface_unlock_poke(this);
    return SL_RESULT_SUCCESS;
}

static SLresult BassBoost_GetRoundedStrength(SLBassBoostItf self,
    SLpermille *pStrength)
{
    if (NULL == pStrength)
        return SL_RESULT_PARAMETER_INVALID;
    struct BassBoost_interface *this = (struct BassBoost_interface *) self;
    interface_lock_peek(this);
    SLpermille strength = this->mStrength;
    interface_unlock_peek(this);
    *pStrength = strength;
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
    if (NULL == group)
        return SL_RESULT_PARAMETER_INVALID;
    struct Object_interface *thisGroup = (struct Object_interface *) group;
    if (&_3DGroup_class != thisGroup->mClass)
        return SL_RESULT_PARAMETER_INVALID;
    // FIXME race possible if group unrealized immediately after, should lock
    if (SL_OBJECT_STATE_REALIZED != thisGroup->mState)
        return SL_RESULT_PRECONDITIONS_VIOLATED;
    interface_lock_exclusive(this);
    this->mGroup = group;
    // FIXME add this object to the group's bag of objects
    interface_unlock_exclusive(this);
    return SL_RESULT_SUCCESS;
}

static SLresult _3DGrouping_Get3DGroup(SL3DGroupingItf self,
    SLObjectItf *pGroup)
{
    if (NULL == pGroup)
        return SL_RESULT_PARAMETER_INVALID;
    struct _3DGrouping_interface *this = (struct _3DGrouping_interface *) self;
    interface_lock_peek(this);
    *pGroup = this->mGroup;
    interface_unlock_peek(this);
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
    struct _3DMacroscopic_interface *this = (struct _3DMacroscopic_interface *) self;
    interface_lock_exclusive(this);
    this->mSize.mWidth = width;
    this->mSize.mHeight = height;
    this->mSize.mDepth = depth;
    interface_unlock_exclusive(this);
    return SL_RESULT_SUCCESS;
}

static SLresult _3DMacroscopic_GetSize(SL3DMacroscopicItf self,
    SLmillimeter *pWidth, SLmillimeter *pHeight, SLmillimeter *pDepth)
{
    if (NULL == pWidth || NULL == pHeight || NULL == pDepth)
        return SL_RESULT_PARAMETER_INVALID;
    struct _3DMacroscopic_interface *this = (struct _3DMacroscopic_interface *) self;
    interface_lock_shared(this);
    SLmillimeter width = this->mSize.mWidth;
    SLmillimeter height = this->mSize.mHeight;
    SLmillimeter depth = this->mSize.mDepth;
    interface_unlock_shared(this);
    *pWidth = width;
    *pHeight = height;
    *pDepth = depth;
    return SL_RESULT_SUCCESS;
}

static SLresult _3DMacroscopic_SetOrientationAngles(SL3DMacroscopicItf self,
    SLmillidegree heading, SLmillidegree pitch, SLmillidegree roll)
{
    struct _3DMacroscopic_interface *this = (struct _3DMacroscopic_interface *) self;
    interface_lock_exclusive(this);
    this->mOrientationAngles.mHeading = heading;
    this->mOrientationAngles.mPitch = pitch;
    this->mOrientationAngles.mRoll = roll;
    this->mOrientationActive = ANGLES_SET_VECTORS_UNKNOWN;
    this->mRotatePending = SL_BOOLEAN_FALSE;
    // ++this->mGeneration;
    interface_unlock_exclusive(this);
    return SL_RESULT_SUCCESS;
}

static SLresult _3DMacroscopic_SetOrientationVectors(SL3DMacroscopicItf self,
    const SLVec3D *pFront, const SLVec3D *pAbove)
{
    if (NULL == pFront || NULL == pAbove)
        return SL_RESULT_PARAMETER_INVALID;
    struct _3DMacroscopic_interface *this = (struct _3DMacroscopic_interface *) self;
    SLVec3D front = *pFront;
    SLVec3D above = *pAbove;
    interface_lock_exclusive(this);
    this->mOrientationVectors.mFront = front;
    this->mOrientationVectors.mUp = above;
    this->mOrientationActive = ANGLES_UNKNOWN_VECTORS_SET;
    this->mRotatePending = SL_BOOLEAN_FALSE;
    interface_unlock_exclusive(this);
    return SL_RESULT_SUCCESS;
}

static SLresult _3DMacroscopic_Rotate(SL3DMacroscopicItf self,
    SLmillidegree theta, const SLVec3D *pAxis)
{
    if (NULL == pAxis)
        return SL_RESULT_PARAMETER_INVALID;
    SLVec3D axis = *pAxis;
    struct _3DMacroscopic_interface *this = (struct _3DMacroscopic_interface *) self;
    // FIXME Do the rotate here:
    // interface_lock_shared(this);
    // read old values and generation
    // interface_unlock_shared(this);
    // compute new position
    interface_lock_exclusive(this);
    while (this->mRotatePending)
        interface_cond_wait(this);
    this->mTheta = theta;
    this->mAxis = axis;
    this->mRotatePending = SL_BOOLEAN_TRUE;
    // compare generation with saved value
    // if equal, store new position and increment generation
    // if unequal, discard new position
    interface_unlock_exclusive(this);
    return SL_RESULT_SUCCESS;
}

static SLresult _3DMacroscopic_GetOrientationVectors(SL3DMacroscopicItf self,
    SLVec3D *pFront, SLVec3D *pUp)
{
    if (NULL == pFront || NULL == pUp)
        return SL_RESULT_PARAMETER_INVALID;
    struct _3DMacroscopic_interface *this = (struct _3DMacroscopic_interface *) self;
    interface_lock_exclusive(this);
    for (;;) {
        enum AnglesVectorsActive orientationActive = this->mOrientationActive;
        switch (orientationActive) {
        case ANGLES_COMPUTED_VECTORS_SET:    // not in 1.0.1
        case ANGLES_REQUESTED_VECTORS_SET:   // not in 1.0.1
        case ANGLES_UNKNOWN_VECTORS_SET:
        case ANGLES_SET_VECTORS_COMPUTED:
            {
            SLVec3D front = this->mOrientationVectors.mFront;
            SLVec3D up = this->mOrientationVectors.mUp;
            interface_unlock_exclusive(this);
            *pFront = front;
            *pUp = up;
            }
            break;
        case ANGLES_SET_VECTORS_UNKNOWN:
            this->mOrientationActive = ANGLES_SET_VECTORS_REQUESTED;
            // fall through
        case ANGLES_SET_VECTORS_REQUESTED:
            // matched by cond_broadcast in case multiple requesters
            interface_cond_wait(this);
            continue;
        default:
            interface_unlock_exclusive(this);
            assert(SL_BOOLEAN_FALSE);
            pFront->x = 0;
            pFront->y = 0;
            pFront->z = 0;
            pUp->x = 0;
            pUp->y = 0;
            pUp->z = 0;
            break;
        }
        break;
    }
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
    struct _3DSource_interface *this = (struct _3DSource_interface *) self;
    interface_lock_poke(this);
    this->mHeadRelative = headRelative;
    interface_unlock_poke(this);
    return SL_RESULT_SUCCESS;
}

static SLresult _3DSource_GetHeadRelative(SL3DSourceItf self,
    SLboolean *pHeadRelative)
{
    if (NULL == pHeadRelative)
        return SL_RESULT_PARAMETER_INVALID;
    struct _3DSource_interface *this = (struct _3DSource_interface *) self;
    interface_lock_peek(this);
    SLboolean headRelative = this->mHeadRelative;
    interface_unlock_peek(this);
    *pHeadRelative = headRelative;
    return SL_RESULT_SUCCESS;
}

static SLresult _3DSource_SetRolloffDistances(SL3DSourceItf self,
    SLmillimeter minDistance, SLmillimeter maxDistance)
{
    struct _3DSource_interface *this = (struct _3DSource_interface *) self;
    interface_lock_exclusive(this);
    this->mMinDistance = minDistance;
    this->mMaxDistance = maxDistance;
    interface_unlock_exclusive(this);
    return SL_RESULT_SUCCESS;
}

static SLresult _3DSource_GetRolloffDistances(SL3DSourceItf self,
    SLmillimeter *pMinDistance, SLmillimeter *pMaxDistance)
{
    if (NULL == pMinDistance || NULL == pMaxDistance)
        return SL_RESULT_PARAMETER_INVALID;
    struct _3DSource_interface *this = (struct _3DSource_interface *) self;
    interface_lock_shared(this);
    SLmillimeter minDistance = this->mMinDistance;
    SLmillimeter maxDistance = this->mMaxDistance;
    interface_unlock_shared(this);
    *pMinDistance = minDistance;
    *pMaxDistance = maxDistance;
    return SL_RESULT_SUCCESS;
}

static SLresult _3DSource_SetRolloffMaxDistanceMute(SL3DSourceItf self,
    SLboolean mute)
{
    struct _3DSource_interface *this = (struct _3DSource_interface *) self;
    interface_lock_poke(this);
    this->mRolloffMaxDistanceMute = mute;
    interface_unlock_poke(this);
    return SL_RESULT_SUCCESS;
}

static SLresult _3DSource_GetRolloffMaxDistanceMute(SL3DSourceItf self,
    SLboolean *pMute)
{
    if (NULL == pMute)
        return SL_RESULT_PARAMETER_INVALID;
    struct _3DSource_interface *this = (struct _3DSource_interface *) self;
    interface_lock_peek(this);
    SLboolean mute = this->mRolloffMaxDistanceMute;
    interface_unlock_peek(this);
    *pMute = mute;
    return SL_RESULT_SUCCESS;
}

static SLresult _3DSource_SetRolloffFactor(SL3DSourceItf self,
    SLpermille rolloffFactor)
{
    struct _3DSource_interface *this = (struct _3DSource_interface *) self;
    interface_lock_poke(this);
    this->mRolloffFactor = rolloffFactor;
    interface_unlock_poke(this);
    return SL_RESULT_SUCCESS;
}

static SLresult _3DSource_GetRolloffFactor(SL3DSourceItf self,
    SLpermille *pRolloffFactor)
{
    struct _3DSource_interface *this = (struct _3DSource_interface *) self;
    interface_lock_peek(this);
    SLpermille rolloffFactor = this->mRolloffFactor;
    interface_unlock_peek(this);
    *pRolloffFactor = rolloffFactor;
    return SL_RESULT_SUCCESS;
}

static SLresult _3DSource_SetRoomRolloffFactor(SL3DSourceItf self,
    SLpermille roomRolloffFactor)
{
    struct _3DSource_interface *this = (struct _3DSource_interface *) self;
    interface_lock_poke(this);
    this->mRoomRolloffFactor = roomRolloffFactor;
    interface_unlock_poke(this);
    return SL_RESULT_SUCCESS;
}

static SLresult _3DSource_GetRoomRolloffFactor(SL3DSourceItf self,
    SLpermille *pRoomRolloffFactor)
{
    struct _3DSource_interface *this = (struct _3DSource_interface *) self;
    interface_lock_peek(this);
    SLpermille roomRolloffFactor = this->mRoomRolloffFactor;
    interface_unlock_peek(this);
    *pRoomRolloffFactor = roomRolloffFactor;
    return SL_RESULT_SUCCESS;
}

static SLresult _3DSource_SetRolloffModel(SL3DSourceItf self, SLuint8 model)
{
    struct _3DSource_interface *this = (struct _3DSource_interface *) self;
    interface_lock_poke(this);
    this->mDistanceModel = model;
    interface_unlock_poke(this);
    return SL_RESULT_SUCCESS;
}

static SLresult _3DSource_GetRolloffModel(SL3DSourceItf self, SLuint8 *pModel)
{
    struct _3DSource_interface *this = (struct _3DSource_interface *) self;
    interface_lock_peek(this);
    SLuint8 model = this->mDistanceModel;
    interface_unlock_peek(this);
    *pModel = model;
    return SL_RESULT_SUCCESS;
}

static SLresult _3DSource_SetCone(SL3DSourceItf self, SLmillidegree innerAngle,
    SLmillidegree outerAngle, SLmillibel outerLevel)
{
    struct _3DSource_interface *this = (struct _3DSource_interface *) self;
    interface_lock_exclusive(this);
    this->mConeInnerAngle = innerAngle;
    this->mConeOuterAngle = outerAngle;
    this->mConeOuterLevel = outerLevel;
    interface_unlock_exclusive(this);
    return SL_RESULT_SUCCESS;
}

static SLresult _3DSource_GetCone(SL3DSourceItf self,
    SLmillidegree *pInnerAngle, SLmillidegree *pOuterAngle,
    SLmillibel *pOuterLevel)
{
    if (NULL == pInnerAngle || NULL == pOuterAngle || NULL == pOuterLevel)
        return SL_RESULT_PARAMETER_INVALID;
    struct _3DSource_interface *this = (struct _3DSource_interface *) self;
    interface_lock_shared(this);
    SLmillidegree innerAngle = this->mConeInnerAngle;
    SLmillidegree outerAngle = this->mConeOuterAngle;
    SLmillibel outerLevel = this->mConeOuterLevel;
    interface_unlock_shared(this);
    *pInnerAngle = innerAngle;
    *pOuterAngle = outerAngle;
    *pOuterLevel = outerLevel;
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
    struct Equalizer_interface *this = (struct Equalizer_interface *) self;
    interface_lock_poke(this);
    this->mEnabled = enabled;
    interface_unlock_poke(this);
    return SL_RESULT_SUCCESS;
}

static SLresult Equalizer_IsEnabled(SLEqualizerItf self, SLboolean *pEnabled)
{
    if (NULL == pEnabled)
        return SL_RESULT_PARAMETER_INVALID;
    struct Equalizer_interface *this = (struct Equalizer_interface *) self;
    interface_lock_peek(this);
    SLboolean enabled = this->mEnabled;
    interface_unlock_peek(this);
    *pEnabled = enabled;
    return SL_RESULT_SUCCESS;
}

static SLresult Equalizer_GetNumberOfBands(SLEqualizerItf self,
    SLuint16 *pNumBands)
{
    if (NULL == pNumBands)
        return SL_RESULT_PARAMETER_INVALID;
    struct Equalizer_interface *this = (struct Equalizer_interface *) self;
    // Note: no lock, but OK because it is const
    *pNumBands = this->mNumBands;
    return SL_RESULT_SUCCESS;
}

static SLresult Equalizer_GetBandLevelRange(SLEqualizerItf self,
    SLmillibel *pMin, SLmillibel *pMax)
{
    if (NULL == pMin && NULL == pMax)
        return SL_RESULT_PARAMETER_INVALID;
    struct Equalizer_interface *this = (struct Equalizer_interface *) self;
    // Note: no lock, but OK because it is const
    if (NULL != pMin)
        *pMin = this->mMin;
    if (NULL != pMax)
        *pMax = this->mMax;
    return SL_RESULT_SUCCESS;
}

static SLresult Equalizer_SetBandLevel(SLEqualizerItf self, SLuint16 band,
    SLmillibel level)
{
    struct Equalizer_interface *this = (struct Equalizer_interface *) self;
    if (band >= this->mNumBands)
        return SL_RESULT_PARAMETER_INVALID;
    interface_lock_poke(this);
    this->mLevels[band] = level;
    interface_unlock_poke(this);
    return SL_RESULT_SUCCESS;
}

static SLresult Equalizer_GetBandLevel(SLEqualizerItf self, SLuint16 band,
    SLmillibel *pLevel)
{
    if (NULL == pLevel)
        return SL_RESULT_PARAMETER_INVALID;
    struct Equalizer_interface *this = (struct Equalizer_interface *) self;
    if (band >= this->mNumBands)
        return SL_RESULT_PARAMETER_INVALID;
    interface_lock_peek(this);
    SLmillibel level = this->mLevels[band];
    interface_unlock_peek(this);
    *pLevel = level;
    return SL_RESULT_SUCCESS;
}

static SLresult Equalizer_GetCenterFreq(SLEqualizerItf self, SLuint16 band,
    SLmilliHertz *pCenter)
{
    if (NULL == pCenter)
        return SL_RESULT_PARAMETER_INVALID;
    struct Equalizer_interface *this = (struct Equalizer_interface *) self;
    if (band >= this->mNumBands)
        return SL_RESULT_PARAMETER_INVALID;
    // Note: no lock, but OK because it is const
    *pCenter = this->mBands[band].mCenter;
    return SL_RESULT_SUCCESS;
}

static SLresult Equalizer_GetBandFreqRange(SLEqualizerItf self, SLuint16 band,
    SLmilliHertz *pMin, SLmilliHertz *pMax)
{
    if (NULL == pMin && NULL == pMax)
        return SL_RESULT_PARAMETER_INVALID;
    struct Equalizer_interface *this = (struct Equalizer_interface *) self;
    if (band >= this->mNumBands)
        return SL_RESULT_PARAMETER_INVALID;
    // Note: no lock, but OK because it is const
    if (NULL != pMin)
        *pMin = this->mBands[band].mMin;
    if (NULL != pMax)
        *pMax = this->mBands[band].mMax;
    return SL_RESULT_SUCCESS;
}

static SLresult Equalizer_GetBand(SLEqualizerItf self, SLmilliHertz frequency,
    SLuint16 *pBand)
{
    if (NULL == pBand)
        return SL_RESULT_PARAMETER_INVALID;
    struct Equalizer_interface *this = (struct Equalizer_interface *) self;
    // search for band whose center frequency has the closest ratio to 1.0
    // assumes bands are unsorted (a pessimistic assumption)
    // assumes bands can overlap (a pessimistic assumption)
    // assumes a small number of bands, so no need for a fancier algorithm
    const struct EqualizerBand *band;
    float floatFreq = (float) frequency;
    float bestRatio = 0.0;
    SLuint16 bestBand = SL_EQUALIZER_UNDEFINED;
    for (band = this->mBands; band < &this->mBands[this->mNumBands]; ++band) {
        if (!(band->mMin <= frequency && frequency <= band->mMax))
            continue;
        assert(band->mMin <= band->mCenter && band->mCenter <= band->mMax);
        assert(band->mCenter != 0);
        float ratio = frequency <= band->mCenter ?
            floatFreq / band->mCenter : band->mCenter / floatFreq;
        if (ratio > bestRatio) {
            bestRatio = ratio;
            bestBand = band - this->mBands;
        }
    }
    *pBand = band - this->mBands;
    return SL_RESULT_SUCCESS;
}

static SLresult Equalizer_GetCurrentPreset(SLEqualizerItf self,
    SLuint16 *pPreset)
{
    if (NULL == pPreset)
        return SL_RESULT_PARAMETER_INVALID;
    struct Equalizer_interface *this = (struct Equalizer_interface *) self;
    interface_lock_peek(this);
    *pPreset = this->mPreset;
    interface_unlock_peek(this);
    return SL_RESULT_SUCCESS;
}

static SLresult Equalizer_UsePreset(SLEqualizerItf self, SLuint16 index)
{
    struct Equalizer_interface *this = (struct Equalizer_interface *) self;
    if (index >= this->mNumPresets)
        return SL_RESULT_PARAMETER_INVALID;
    interface_lock_poke(this);
    this->mPreset = index;
    interface_unlock_poke(this);
    return SL_RESULT_SUCCESS;
}

static SLresult Equalizer_GetNumberOfPresets(SLEqualizerItf self,
    SLuint16 *pNumPresets)
{
    if (NULL == pNumPresets)
        return SL_RESULT_PARAMETER_INVALID;
    struct Equalizer_interface *this = (struct Equalizer_interface *) self;
    // Note: no lock, but OK because it is const
    *pNumPresets = this->mNumPresets;
    return SL_RESULT_SUCCESS;
}

static SLresult Equalizer_GetPresetName(SLEqualizerItf self, SLuint16 index,
    const SLchar ** ppName)
{
    if (NULL == ppName)
        return SL_RESULT_PARAMETER_INVALID;
    struct Equalizer_interface *this = (struct Equalizer_interface *) self;
    if (index >= this->mNumPresets)
        return SL_RESULT_PARAMETER_INVALID;
    *ppName = this->mPresetNames[index];
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
    interface_lock_poke(this);
    this->mPreset = preset;
    interface_unlock_poke(this);
    return SL_RESULT_SUCCESS;
}

static SLresult PresetReverb_GetPreset(SLPresetReverbItf self,
    SLuint16 *pPreset)
{
    if (NULL == pPreset)
        return SL_RESULT_PARAMETER_INVALID;
    struct PresetReverb_interface *this =
        (struct PresetReverb_interface *) self;
    interface_lock_peek(this);
    SLuint16 preset = this->mPreset;
    interface_unlock_peek(this);
    *pPreset = preset;
    return SL_RESULT_SUCCESS;
}

/*static*/ const struct SLPresetReverbItf_ PresetReverb_PresetReverbItf = {
    PresetReverb_SetPreset,
    PresetReverb_GetPreset
};

// EnvironmentalReverb implementation

// Note: all Set operations use exclusive not poke,
// because SetEnvironmentalReverbProperties is exclusive.
// It is safe for the Get operations to use peek,
// on the assumption that the block copy will atomically
// replace each word of the block.

static SLresult EnvironmentalReverb_SetRoomLevel(SLEnvironmentalReverbItf self,
    SLmillibel room)
{
    struct EnvironmentalReverb_interface *this = (struct EnvironmentalReverb_interface *) self;
    interface_lock_exclusive(this);
    this->mProperties.roomLevel = room;
    interface_unlock_exclusive(this);
    return SL_RESULT_SUCCESS;
}

static SLresult EnvironmentalReverb_GetRoomLevel(SLEnvironmentalReverbItf self,
    SLmillibel *pRoom)
{
    if (NULL == pRoom)
        return SL_RESULT_PARAMETER_INVALID;
    struct EnvironmentalReverb_interface *this = (struct EnvironmentalReverb_interface *) self;
    interface_lock_peek(this);
    SLmillibel roomLevel = this->mProperties.roomLevel;
    interface_unlock_peek(this);
    *pRoom = roomLevel;
    return SL_RESULT_SUCCESS;
}

static SLresult EnvironmentalReverb_SetRoomHFLevel(
    SLEnvironmentalReverbItf self, SLmillibel roomHF)
{
    struct EnvironmentalReverb_interface *this = (struct EnvironmentalReverb_interface *) self;
    interface_lock_exclusive(this);
    this->mProperties.roomHFLevel = roomHF;
    interface_unlock_exclusive(this);
    return SL_RESULT_SUCCESS;
}

static SLresult EnvironmentalReverb_GetRoomHFLevel(
    SLEnvironmentalReverbItf self, SLmillibel *pRoomHF)
{
    if (NULL == pRoomHF)
        return SL_RESULT_PARAMETER_INVALID;
    struct EnvironmentalReverb_interface *this = (struct EnvironmentalReverb_interface *) self;
    interface_lock_peek(this);
    SLmillibel roomHFLevel = this->mProperties.roomHFLevel;
    interface_unlock_peek(this);
    *pRoomHF = roomHFLevel;
    return SL_RESULT_SUCCESS;
}

static SLresult EnvironmentalReverb_SetDecayTime(
    SLEnvironmentalReverbItf self, SLmillisecond decayTime)
{
    struct EnvironmentalReverb_interface *this = (struct EnvironmentalReverb_interface *) self;
    interface_lock_exclusive(this);
    this->mProperties.decayTime = decayTime;
    interface_unlock_exclusive(this);
    return SL_RESULT_SUCCESS;
}

static SLresult EnvironmentalReverb_GetDecayTime(
    SLEnvironmentalReverbItf self, SLmillisecond *pDecayTime)
{
    if (NULL == pDecayTime)
        return SL_RESULT_PARAMETER_INVALID;
    struct EnvironmentalReverb_interface *this = (struct EnvironmentalReverb_interface *) self;
    interface_lock_peek(this);
    SLmillisecond decayTime = this->mProperties.decayTime;
    interface_unlock_peek(this);
    *pDecayTime = decayTime;
    return SL_RESULT_SUCCESS;
}

static SLresult EnvironmentalReverb_SetDecayHFRatio(
    SLEnvironmentalReverbItf self, SLpermille decayHFRatio)
{
    struct EnvironmentalReverb_interface *this = (struct EnvironmentalReverb_interface *) self;
    interface_lock_exclusive(this);
    this->mProperties.decayHFRatio = decayHFRatio;
    interface_unlock_exclusive(this);
    return SL_RESULT_SUCCESS;
}

static SLresult EnvironmentalReverb_GetDecayHFRatio(
    SLEnvironmentalReverbItf self, SLpermille *pDecayHFRatio)
{
    if (NULL == pDecayHFRatio)
        return SL_RESULT_PARAMETER_INVALID;
    struct EnvironmentalReverb_interface *this = (struct EnvironmentalReverb_interface *) self;
    interface_lock_peek(this);
    SLpermille decayHFRatio = this->mProperties.decayHFRatio;
    interface_unlock_peek(this);
    *pDecayHFRatio = decayHFRatio;
    return SL_RESULT_SUCCESS;
}

static SLresult EnvironmentalReverb_SetReflectionsLevel(
    SLEnvironmentalReverbItf self, SLmillibel reflectionsLevel)
{
    struct EnvironmentalReverb_interface *this = (struct EnvironmentalReverb_interface *) self;
    interface_lock_exclusive(this);
    this->mProperties.reflectionsLevel = reflectionsLevel;
    interface_unlock_exclusive(this);
    return SL_RESULT_SUCCESS;
}

static SLresult EnvironmentalReverb_GetReflectionsLevel(
    SLEnvironmentalReverbItf self, SLmillibel *pReflectionsLevel)
{
    if (NULL == pReflectionsLevel)
        return SL_RESULT_PARAMETER_INVALID;
    struct EnvironmentalReverb_interface *this = (struct EnvironmentalReverb_interface *) self;
    interface_lock_peek(this);
    SLmillibel reflectionsLevel = this->mProperties.reflectionsLevel;
    interface_unlock_peek(this);
    *pReflectionsLevel = reflectionsLevel;
    return SL_RESULT_SUCCESS;
}

static SLresult EnvironmentalReverb_SetReflectionsDelay(
    SLEnvironmentalReverbItf self, SLmillisecond reflectionsDelay)
{
    struct EnvironmentalReverb_interface *this = (struct EnvironmentalReverb_interface *) self;
    interface_lock_exclusive(this);
    this->mProperties.reflectionsDelay = reflectionsDelay;
    interface_unlock_exclusive(this);
    return SL_RESULT_SUCCESS;
}

static SLresult EnvironmentalReverb_GetReflectionsDelay(
    SLEnvironmentalReverbItf self, SLmillisecond *pReflectionsDelay)
{
    if (NULL == pReflectionsDelay)
        return SL_RESULT_PARAMETER_INVALID;
    struct EnvironmentalReverb_interface *this = (struct EnvironmentalReverb_interface *) self;
    interface_lock_peek(this);
    SLmillisecond reflectionsDelay = this->mProperties.reflectionsDelay;
    interface_unlock_peek(this);
    *pReflectionsDelay = reflectionsDelay;
    return SL_RESULT_SUCCESS;
}

static SLresult EnvironmentalReverb_SetReverbLevel(
    SLEnvironmentalReverbItf self, SLmillibel reverbLevel)
{
    struct EnvironmentalReverb_interface *this = (struct EnvironmentalReverb_interface *) self;
    interface_lock_exclusive(this);
    this->mProperties.reverbLevel = reverbLevel;
    interface_unlock_exclusive(this);
    return SL_RESULT_SUCCESS;
}

static SLresult EnvironmentalReverb_GetReverbLevel(
    SLEnvironmentalReverbItf self, SLmillibel *pReverbLevel)
{
    if (NULL == pReverbLevel)
        return SL_RESULT_PARAMETER_INVALID;
    struct EnvironmentalReverb_interface *this = (struct EnvironmentalReverb_interface *) self;
    interface_lock_peek(this);
    SLmillibel reverbLevel = this->mProperties.reverbLevel;
    interface_unlock_peek(this);
    *pReverbLevel = reverbLevel;
    return SL_RESULT_SUCCESS;
}

static SLresult EnvironmentalReverb_SetReverbDelay(
    SLEnvironmentalReverbItf self, SLmillisecond reverbDelay)
{
    struct EnvironmentalReverb_interface *this = (struct EnvironmentalReverb_interface *) self;
    interface_lock_exclusive(this);
    this->mProperties.reverbDelay = reverbDelay;
    interface_unlock_exclusive(this);
    return SL_RESULT_SUCCESS;
}

static SLresult EnvironmentalReverb_GetReverbDelay(
    SLEnvironmentalReverbItf self, SLmillisecond *pReverbDelay)
{
    if (NULL == pReverbDelay)
        return SL_RESULT_PARAMETER_INVALID;
    struct EnvironmentalReverb_interface *this = (struct EnvironmentalReverb_interface *) self;
    interface_lock_peek(this);
    SLmillisecond reverbDelay = this->mProperties.reverbDelay;
    interface_unlock_peek(this);
    *pReverbDelay = reverbDelay;
    return SL_RESULT_SUCCESS;
}

static SLresult EnvironmentalReverb_SetDiffusion(
    SLEnvironmentalReverbItf self, SLpermille diffusion)
{
    struct EnvironmentalReverb_interface *this = (struct EnvironmentalReverb_interface *) self;
    interface_lock_exclusive(this);
    this->mProperties.diffusion = diffusion;
    interface_unlock_exclusive(this);
    return SL_RESULT_SUCCESS;
}

static SLresult EnvironmentalReverb_GetDiffusion(SLEnvironmentalReverbItf self,
     SLpermille *pDiffusion)
{
    if (NULL == pDiffusion)
        return SL_RESULT_PARAMETER_INVALID;
    struct EnvironmentalReverb_interface *this = (struct EnvironmentalReverb_interface *) self;
    interface_lock_peek(this);
    SLpermille diffusion = this->mProperties.diffusion;
    interface_unlock_peek(this);
    *pDiffusion = diffusion;
    return SL_RESULT_SUCCESS;
}

static SLresult EnvironmentalReverb_SetDensity(SLEnvironmentalReverbItf self,
    SLpermille density)
{
    struct EnvironmentalReverb_interface *this = (struct EnvironmentalReverb_interface *) self;
    interface_lock_exclusive(this);
    this->mProperties.density = density;
    interface_unlock_exclusive(this);
    return SL_RESULT_SUCCESS;
}

static SLresult EnvironmentalReverb_GetDensity(SLEnvironmentalReverbItf self,
    SLpermille *pDensity)
{
    if (NULL == pDensity)
        return SL_RESULT_PARAMETER_INVALID;
    struct EnvironmentalReverb_interface *this = (struct EnvironmentalReverb_interface *) self;
    interface_lock_peek(this);
    SLpermille density = this->mProperties.density;
    interface_unlock_peek(this);
    *pDensity = density;
    return SL_RESULT_SUCCESS;
}

static SLresult EnvironmentalReverb_SetEnvironmentalReverbProperties(
    SLEnvironmentalReverbItf self,
    const SLEnvironmentalReverbSettings *pProperties)
{
    if (NULL == pProperties)
        return SL_RESULT_PARAMETER_INVALID;
    struct EnvironmentalReverb_interface *this = (struct EnvironmentalReverb_interface *) self;
    SLEnvironmentalReverbSettings properties = *pProperties;
    interface_lock_exclusive(this);
    this->mProperties = properties;
    interface_unlock_exclusive(this);
    return SL_RESULT_SUCCESS;
}

static SLresult EnvironmentalReverb_GetEnvironmentalReverbProperties(
    SLEnvironmentalReverbItf self, SLEnvironmentalReverbSettings *pProperties)
{
    if (NULL == pProperties)
        return SL_RESULT_PARAMETER_INVALID;
    struct EnvironmentalReverb_interface *this = (struct EnvironmentalReverb_interface *) self;
    interface_lock_shared(this);
    SLEnvironmentalReverbSettings properties = this->mProperties;
    interface_unlock_shared(this);
    *pProperties = properties;
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
    interface_lock_poke(this);
    this->mEnabled = enabled;
    interface_unlock_poke(this);
    return SL_RESULT_SUCCESS;
}

static SLresult Virtualizer_IsEnabled(SLVirtualizerItf self,
    SLboolean *pEnabled)
{
    if (NULL == pEnabled)
        return SL_RESULT_PARAMETER_INVALID;
    struct Virtualizer_interface *this = (struct Virtualizer_interface *) self;
    interface_lock_peek(this);
    SLboolean enabled = this->mEnabled;
    interface_unlock_peek(this);
    *pEnabled = enabled;
    return SL_RESULT_SUCCESS;
}

static SLresult Virtualizer_SetStrength(SLVirtualizerItf self,
    SLpermille strength)
{
    struct Virtualizer_interface *this = (struct Virtualizer_interface *) self;
    interface_lock_poke(this);
    this->mStrength = strength;
    interface_unlock_poke(this);
    return SL_RESULT_SUCCESS;
}

static SLresult Virtualizer_GetRoundedStrength(SLVirtualizerItf self,
    SLpermille *pStrength)
{
    if (NULL == pStrength)
        return SL_RESULT_PARAMETER_INVALID;
    struct Virtualizer_interface *this = (struct Virtualizer_interface *) self;
    interface_lock_peek(this);
    SLpermille strength = this->mStrength;
    interface_unlock_peek(this);
    *pStrength = strength;
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

// MIDIMessage implementation

static SLresult MIDIMessage_SendMessage(SLMIDIMessageItf self, const SLuint8 *data, SLuint32 length)
{
    if (NULL == data)
        return SL_RESULT_PARAMETER_INVALID;
    //struct MIDIMessage_interface *this = (struct MIDIMessage_interface *) self;
    return SL_RESULT_SUCCESS;
}

static SLresult MIDIMessage_RegisterMetaEventCallback(SLMIDIMessageItf self,
    slMetaEventCallback callback, void *pContext)
{
    struct MIDIMessage_interface *this = (struct MIDIMessage_interface *) self;
    interface_lock_exclusive(this);
    this->mMetaEventCallback = callback;
    this->mMetaEventContext = pContext;
    interface_unlock_exclusive(this);
    return SL_RESULT_SUCCESS;
}

static SLresult MIDIMessage_RegisterMIDIMessageCallback(SLMIDIMessageItf self,
    slMIDIMessageCallback callback, void *pContext)
{
    struct MIDIMessage_interface *this = (struct MIDIMessage_interface *) self;
    interface_lock_exclusive(this);
    this->mMessageCallback = callback;
    this->mMessageContext = pContext;
    interface_unlock_exclusive(this);
    return SL_RESULT_SUCCESS;
}

static SLresult MIDIMessage_AddMIDIMessageCallbackFilter(SLMIDIMessageItf self,
    SLuint32 messageType)
{
    //struct MIDIMessage_interface *this = (struct MIDIMessage_interface *) self;
    return SL_RESULT_SUCCESS;
}

static SLresult MIDIMessage_ClearMIDIMessageCallbackFilter(SLMIDIMessageItf self)
{
    //struct MIDIMessage_interface *this = (struct MIDIMessage_interface *) self;
    return SL_RESULT_SUCCESS;
}

/*static*/ const struct SLMIDIMessageItf_ MIDIMessage_MIDIMessageItf = {
    MIDIMessage_SendMessage,
    MIDIMessage_RegisterMetaEventCallback,
    MIDIMessage_RegisterMIDIMessageCallback,
    MIDIMessage_AddMIDIMessageCallbackFilter,
    MIDIMessage_ClearMIDIMessageCallbackFilter
};

// MIDIMuteSolo implementation

static SLresult MIDIMuteSolo_SetChannelMute(SLMIDIMuteSoloItf self, SLuint8 channel, SLboolean mute)
{
    struct MIDIMuteSolo_interface *this = (struct MIDIMuteSolo_interface *) self;
    SLuint16 mask = 1 << channel;
    interface_lock_exclusive(this);
    if (mute)
        this->mChannelMuteMask |= mask;
    else
        this->mChannelMuteMask &= ~mask;
    interface_unlock_exclusive(this);
    return SL_RESULT_SUCCESS;
}

static SLresult MIDIMuteSolo_GetChannelMute(SLMIDIMuteSoloItf self, SLuint8 channel,
    SLboolean *pMute)
{
    if (NULL == pMute)
        return SL_RESULT_PARAMETER_INVALID;
    struct MIDIMuteSolo_interface *this = (struct MIDIMuteSolo_interface *) self;
    interface_lock_peek(this);
    SLuint16 mask = this->mChannelMuteMask;
    interface_unlock_peek(this);
    *pMute = (mask >> channel) & 1;
    return SL_RESULT_SUCCESS;
}

static SLresult MIDIMuteSolo_SetChannelSolo(SLMIDIMuteSoloItf self, SLuint8 channel, SLboolean solo)
{
    struct MIDIMuteSolo_interface *this = (struct MIDIMuteSolo_interface *) self;
    SLuint16 mask = 1 << channel;
    interface_lock_exclusive(this);
    if (solo)
        this->mChannelSoloMask |= mask;
    else
        this->mChannelSoloMask &= ~mask;
    interface_unlock_exclusive(this);
    return SL_RESULT_SUCCESS;
}

static SLresult MIDIMuteSolo_GetChannelSolo(SLMIDIMuteSoloItf self, SLuint8 channel,
    SLboolean *pSolo)
{
    if (NULL == pSolo)
        return SL_RESULT_PARAMETER_INVALID;
    struct MIDIMuteSolo_interface *this = (struct MIDIMuteSolo_interface *) self;
    interface_lock_peek(this);
    SLuint16 mask = this->mChannelSoloMask;
    interface_unlock_peek(this);
    *pSolo = (mask >> channel) & 1;
    return SL_RESULT_SUCCESS;
}

static SLresult MIDIMuteSolo_GetTrackCount(SLMIDIMuteSoloItf self, SLuint16 *pCount)
{
    if (NULL == pCount)
        return SL_RESULT_PARAMETER_INVALID;
    struct MIDIMuteSolo_interface *this = (struct MIDIMuteSolo_interface *) self;
    // FIXME is this const? if so, remove the lock
    interface_lock_peek(this);
    SLuint16 trackCount = this->mTrackCount;
    interface_unlock_peek(this);
    *pCount = trackCount;
    return SL_RESULT_SUCCESS;
}

static SLresult MIDIMuteSolo_SetTrackMute(SLMIDIMuteSoloItf self, SLuint16 track, SLboolean mute)
{
    struct MIDIMuteSolo_interface *this = (struct MIDIMuteSolo_interface *) self;
    SLuint32 mask = 1 << track;
    interface_lock_exclusive(this);
    if (mute)
        this->mTrackMuteMask |= mask;
    else
        this->mTrackMuteMask &= ~mask;
    interface_unlock_exclusive(this);
    return SL_RESULT_SUCCESS;
}

static SLresult MIDIMuteSolo_GetTrackMute(SLMIDIMuteSoloItf self, SLuint16 track, SLboolean *pMute)
{
    if (NULL == pMute)
        return SL_RESULT_PARAMETER_INVALID;
    struct MIDIMuteSolo_interface *this = (struct MIDIMuteSolo_interface *) self;
    interface_lock_peek(this);
    SLuint32 mask = this->mTrackMuteMask;
    interface_unlock_peek(this);
    *pMute = (mask >> track) & 1;
    return SL_RESULT_SUCCESS;
}

static SLresult MIDIMuteSolo_SetTrackSolo(SLMIDIMuteSoloItf self, SLuint16 track, SLboolean solo)
{
    struct MIDIMuteSolo_interface *this = (struct MIDIMuteSolo_interface *) self;
    SLuint32 mask = 1 << track;
    interface_lock_exclusive(this);
    if (solo)
        this->mTrackSoloMask |= mask;
    else
        this->mTrackSoloMask &= ~mask;
    interface_unlock_exclusive(this);
    return SL_RESULT_SUCCESS;
}

static SLresult MIDIMuteSolo_GetTrackSolo(SLMIDIMuteSoloItf self, SLuint16 track, SLboolean *pSolo)
{
    if (NULL == pSolo)
        return SL_RESULT_PARAMETER_INVALID;
    struct MIDIMuteSolo_interface *this = (struct MIDIMuteSolo_interface *) self;
    interface_lock_peek(this);
    SLuint32 mask = this->mTrackSoloMask;
    interface_unlock_peek(this);
    *pSolo = (mask >> track) & 1;
    return SL_RESULT_SUCCESS;
}

/*static*/ const struct SLMIDIMuteSoloItf_ MIDIMuteSolo_MIDIMuteSoloItf = {
    MIDIMuteSolo_SetChannelMute,
    MIDIMuteSolo_GetChannelMute,
    MIDIMuteSolo_SetChannelSolo,
    MIDIMuteSolo_GetChannelSolo,
    MIDIMuteSolo_GetTrackCount,
    MIDIMuteSolo_SetTrackMute,
    MIDIMuteSolo_GetTrackMute,
    MIDIMuteSolo_SetTrackSolo,
    MIDIMuteSolo_GetTrackSolo
};

// MIDITempo implementation

static SLresult MIDITempo_SetTicksPerQuarterNote(SLMIDITempoItf self, SLuint32 tpqn)
{
    struct MIDITempo_interface *this = (struct MIDITempo_interface *) self;
    interface_lock_poke(this);
    this->mTicksPerQuarterNote = tpqn;
    interface_unlock_poke(this);
    return SL_RESULT_SUCCESS;
}

static SLresult MIDITempo_GetTicksPerQuarterNote(SLMIDITempoItf self, SLuint32 *pTpqn)
{
    if (NULL == pTpqn)
        return SL_RESULT_PARAMETER_INVALID;
    struct MIDITempo_interface *this = (struct MIDITempo_interface *) self;
    interface_lock_peek(this);
    SLuint32 ticksPerQuarterNote = this->mTicksPerQuarterNote;
    interface_unlock_peek(this);
    *pTpqn = ticksPerQuarterNote;
    return SL_RESULT_SUCCESS;
}

static SLresult MIDITempo_SetMicrosecondsPerQuarterNote(SLMIDITempoItf self, SLmicrosecond uspqn)
{
    struct MIDITempo_interface *this = (struct MIDITempo_interface *) self;
    interface_lock_poke(this);
    this->mMicrosecondsPerQuarterNote = uspqn;
    interface_unlock_poke(this);
    return SL_RESULT_SUCCESS;
}

static SLresult MIDITempo_GetMicrosecondsPerQuarterNote(SLMIDITempoItf self, SLmicrosecond *uspqn)
{
    if (NULL == uspqn)
        return SL_RESULT_PARAMETER_INVALID;
    struct MIDITempo_interface *this = (struct MIDITempo_interface *) self;
    interface_lock_peek(this);
    SLuint32 microsecondsPerQuarterNote = this->mMicrosecondsPerQuarterNote;
    interface_unlock_peek(this);
    *uspqn = microsecondsPerQuarterNote;
    return SL_RESULT_SUCCESS;
}

/*static*/ const struct SLMIDITempoItf_ MIDITempo_MIDITempoItf = {
    MIDITempo_SetTicksPerQuarterNote,
    MIDITempo_GetTicksPerQuarterNote,
    MIDITempo_SetMicrosecondsPerQuarterNote,
    MIDITempo_GetMicrosecondsPerQuarterNote
};

// MIDITime implementation

static SLresult MIDITime_GetDuration(SLMIDITimeItf self, SLuint32 *pDuration)
{
    if (NULL == pDuration)
        return SL_RESULT_PARAMETER_INVALID;
    struct MIDITime_interface *this = (struct MIDITime_interface *) self;
    SLuint32 duration = this->mDuration;
    *pDuration = duration;
    return SL_RESULT_SUCCESS;
}

static SLresult MIDITime_SetPosition(SLMIDITimeItf self, SLuint32 position)
{
    struct MIDITime_interface *this = (struct MIDITime_interface *) self;
    interface_lock_poke(this);
    this->mPosition = position;
    interface_unlock_poke(this);
    return SL_RESULT_SUCCESS;
}

static SLresult MIDITime_GetPosition(SLMIDITimeItf self, SLuint32 *pPosition)
{
    if (NULL == pPosition)
        return SL_RESULT_PARAMETER_INVALID;
    struct MIDITime_interface *this = (struct MIDITime_interface *) self;
    interface_lock_peek(this);
    SLuint32 position = this->mPosition;
    interface_unlock_peek(this);
    *pPosition = position;
    return SL_RESULT_SUCCESS;
}

static SLresult MIDITime_SetLoopPoints(SLMIDITimeItf self, SLuint32 startTick, SLuint32 numTicks)
{
    struct MIDITime_interface *this = (struct MIDITime_interface *) self;
    interface_lock_exclusive(this);
    this->mStartTick = startTick;
    this->mNumTicks = numTicks;
    interface_unlock_exclusive(this);
    return SL_RESULT_SUCCESS;
}

static SLresult MIDITime_GetLoopPoints(SLMIDITimeItf self, SLuint32 *pStartTick,
    SLuint32 *pNumTicks)
{
    if (NULL == pStartTick || NULL == pNumTicks)
        return SL_RESULT_PARAMETER_INVALID;
    struct MIDITime_interface *this = (struct MIDITime_interface *) self;
    interface_lock_shared(this);
    SLuint32 startTick = this->mStartTick;
    SLuint32 numTicks = this->mNumTicks;
    interface_unlock_shared(this);
    *pStartTick = startTick;
    *pNumTicks = numTicks;
    return SL_RESULT_SUCCESS;
}

/*static */ const struct SLMIDITimeItf_ MIDITime_MIDITimeItf = {
    MIDITime_GetDuration,
    MIDITime_SetPosition,
    MIDITime_GetPosition,
    MIDITime_SetLoopPoints,
    MIDITime_GetLoopPoints
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
    SLboolean lossOfControlGlobal = SL_BOOLEAN_FALSE;
    if (NULL != pEngineOptions) {
        SLuint32 i;
        const SLEngineOption *option = pEngineOptions;
        for (i = 0; i < numOptions; ++i, ++option) {
            switch (option->feature) {
            case SL_ENGINEOPTION_THREADSAFE:
                threadSafe = (SLboolean) option->data;
                break;
            case SL_ENGINEOPTION_LOSSOFCONTROL:
                lossOfControlGlobal = (SLboolean) option->data;
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
    struct Engine_class *this = (struct Engine_class *)
        construct(&Engine_class, exposedMask, NULL);
    if (NULL == this)
        return SL_RESULT_MEMORY_FAILURE;
    this->mObject.mLossOfControlMask = lossOfControlGlobal ? ~0 : 0;
    this->mEngine.mLossOfControlGlobal = lossOfControlGlobal;
    this->mEngineCapabilities.mThreadSafe = threadSafe;
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
    // FIXME Finding one interface from another, but is it exposed?
    struct OutputMix_interface *this =
        &((struct OutputMix_class *) thisExt->mThis)->mOutputMix;
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
