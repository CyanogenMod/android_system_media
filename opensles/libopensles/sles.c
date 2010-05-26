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

/* Private functions */

// Map SLInterfaceID to its minimal perfect hash (MPH), or -1 if unknown

/*static*/ int IID_to_MPH(const SLInterfaceID iid)
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

SLresult checkInterfaces(const ClassTable *class__,
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

// private helper shared by decoder and encoder
SLresult GetCodecCapabilities(SLuint32 decoderId, SLuint32 *pIndex,
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

/* Interface initialization hooks */

#ifdef USE_OUTPUTMIXEXT
extern const struct SLOutputMixExtItf_ OutputMixExt_OutputMixExtItf;

static void OutputMixExt_init(void *self)
{
    IOutputMixExt *this =
        (IOutputMixExt *) self;
    this->mItf = &OutputMixExt_OutputMixExtItf;
}
#endif // USE_OUTPUTMIXEXT

#ifdef __cplusplus
extern "C" {
#endif

extern void
    I3DCommit_init(void *),
    I3DDoppler_init(void *),
    I3DGrouping_init(void *),
    I3DLocation_init(void *),
    I3DMacroscopic_init(void *),
    I3DSource_init(void *),
    IAudioDecoderCapabilities_init(void *),
    IAudioEncoderCapabilities_init(void *),
    IAudioEncoder_init(void *),
    IAudioIODeviceCapabilities_init(void *),
    IBassBoost_init(void *),
    IBufferQueue_init(void *),
    IDeviceVolume_init(void *),
    IDynamicInterfaceManagement_init(void *),
    IDynamicSource_init(void *),
    IEffectSend_init(void *),
    IEngineCapabilities_init(void *),
    IEngine_init(void *),
    IEnvironmentalReverb_init(void *),
    IEqualizer_init(void *),
    ILEDArray_init(void *),
    IMIDIMessage_init(void *),
    IMIDIMuteSolo_init(void *),
    IMIDITempo_init(void *),
    IMIDITime_init(void *),
    IMetadataExtraction_init(void *),
    IMetadataTraversal_init(void *),
    IMuteSolo_init(void *),
    IObject_init(void *),
    IOutputMix_init(void *),
    IPitch_init(void *),
    IPlay_init(void *),
    IPlaybackRate_init(void *),
    IPrefetchStatus_init(void *),
    IPresetReverb_init(void *),
    IRatePitch_init(void *),
    IRecord_init(void *),
    ISeek_init(void *),
    IThreadSync_init(void *),
    IVibra_init(void *),
    IVirtualizer_init(void *),
    IVisualization_init(void *),
    IVolume_init(void *);

#ifdef __cplusplus
}
#endif

/*static*/ const struct MPH_init MPH_init_table[MPH_MAX] = {
    { /* MPH_3DCOMMIT, */ I3DCommit_init, NULL },
    { /* MPH_3DDOPPLER, */ I3DDoppler_init, NULL },
    { /* MPH_3DGROUPING, */ I3DGrouping_init, NULL },
    { /* MPH_3DLOCATION, */ I3DLocation_init, NULL },
    { /* MPH_3DMACROSCOPIC, */ I3DMacroscopic_init, NULL },
    { /* MPH_3DSOURCE, */ I3DSource_init, NULL },
    { /* MPH_AUDIODECODERCAPABILITIES, */ IAudioDecoderCapabilities_init, NULL },
    { /* MPH_AUDIOENCODER, */ IAudioEncoder_init, NULL },
    { /* MPH_AUDIOENCODERCAPABILITIES, */ IAudioEncoderCapabilities_init, NULL },
    { /* MPH_AUDIOIODEVICECAPABILITIES, */ IAudioIODeviceCapabilities_init,
        NULL },
    { /* MPH_BASSBOOST, */ IBassBoost_init, NULL },
    { /* MPH_BUFFERQUEUE, */ IBufferQueue_init, NULL },
    { /* MPH_DEVICEVOLUME, */ IDeviceVolume_init, NULL },
    { /* MPH_DYNAMICINTERFACEMANAGEMENT, */ IDynamicInterfaceManagement_init,
        NULL },
    { /* MPH_DYNAMICSOURCE, */ IDynamicSource_init, NULL },
    { /* MPH_EFFECTSEND, */ IEffectSend_init, NULL },
    { /* MPH_ENGINE, */ IEngine_init, NULL },
    { /* MPH_ENGINECAPABILITIES, */ IEngineCapabilities_init, NULL },
    { /* MPH_ENVIRONMENTALREVERB, */ IEnvironmentalReverb_init, NULL },
    { /* MPH_EQUALIZER, */ IEqualizer_init, NULL },
    { /* MPH_LED, */ ILEDArray_init, NULL },
    { /* MPH_METADATAEXTRACTION, */ IMetadataExtraction_init, NULL },
    { /* MPH_METADATATRAVERSAL, */ IMetadataTraversal_init, NULL },
    { /* MPH_MIDIMESSAGE, */ IMIDIMessage_init, NULL },
    { /* MPH_MIDITIME, */ IMIDITime_init, NULL },
    { /* MPH_MIDITEMPO, */ IMIDITempo_init, NULL },
    { /* MPH_MIDIMUTESOLO, */ IMIDIMuteSolo_init, NULL },
    { /* MPH_MUTESOLO, */ IMuteSolo_init, NULL },
    { /* MPH_NULL, */ NULL, NULL },
    { /* MPH_OBJECT, */ IObject_init, NULL },
    { /* MPH_OUTPUTMIX, */ IOutputMix_init, NULL },
    { /* MPH_PITCH, */ IPitch_init, NULL },
    { /* MPH_PLAY, */ IPlay_init, NULL },
    { /* MPH_PLAYBACKRATE, */ IPlaybackRate_init, NULL },
    { /* MPH_PREFETCHSTATUS, */ IPrefetchStatus_init, NULL },
    { /* MPH_PRESETREVERB, */ IPresetReverb_init, NULL },
    { /* MPH_RATEPITCH, */ IRatePitch_init, NULL },
    { /* MPH_RECORD, */ IRecord_init, NULL },
    { /* MPH_SEEK, */ ISeek_init, NULL },
    { /* MPH_THREADSYNC, */ IThreadSync_init, NULL },
    { /* MPH_VIBRA, */ IVibra_init, NULL },
    { /* MPH_VIRTUALIZER, */ IVirtualizer_init, NULL },
    { /* MPH_VISUALIZATION, */ IVisualization_init, NULL },
    { /* MPH_VOLUME, */ IVolume_init, NULL },
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
        offsetof(C3DGroup, mObject)},
    {MPH_DYNAMICINTERFACEMANAGEMENT, INTERFACE_IMPLICIT,
        offsetof(C3DGroup, mDynamicInterfaceManagement)},
    {MPH_3DLOCATION, INTERFACE_IMPLICIT,
        offsetof(C3DGroup, m3DLocation)},
    {MPH_3DDOPPLER, INTERFACE_DYNAMIC_GAME,
        offsetof(C3DGroup, m3DDoppler)},
    {MPH_3DSOURCE, INTERFACE_GAME,
        offsetof(C3DGroup, m3DSource)},
    {MPH_3DMACROSCOPIC, INTERFACE_OPTIONAL,
        offsetof(C3DGroup, m3DMacroscopic)},
};

/*static*/ const ClassTable C3DGroup_class = {
    _3DGroup_interfaces,
    sizeof(_3DGroup_interfaces)/sizeof(_3DGroup_interfaces[0]),
    MPH_to_3DGroup,
    //"3DGroup",
    sizeof(C3DGroup),
    SL_OBJECTID_3DGROUP,
    NULL,
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
    CAudioPlayer *this = (CAudioPlayer *) self;
    SLresult result = SL_RESULT_SUCCESS;

#ifdef USE_ANDROID
    // FIXME move this to android specific files
    result = sles_to_android_audioPlayerRealize(this);
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
            IBufferQueue *thisBQ =
                (IBufferQueue *) bufferQueue;
            thisBQ->mCallback = SndFile_Callback;
            thisBQ->mContext = &this->mSndFile;
        }
    }
#endif // USE_SNDFILE
    return result;
}

static void AudioPlayer_Destroy(void *self)
{
    CAudioPlayer *this = (CAudioPlayer *) self;
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
#ifdef USE_ANDROID
    sles_to_android_audioPlayerDestroy(this);
#endif
}

static const struct iid_vtable AudioPlayer_interfaces[] = {
    {MPH_OBJECT, INTERFACE_IMPLICIT,
        offsetof(CAudioPlayer, mObject)},
    {MPH_DYNAMICINTERFACEMANAGEMENT, INTERFACE_IMPLICIT,
        offsetof(CAudioPlayer, mDynamicInterfaceManagement)},
    {MPH_PLAY, INTERFACE_IMPLICIT,
        offsetof(CAudioPlayer, mPlay)},
    {MPH_3DDOPPLER, INTERFACE_DYNAMIC_GAME,
        offsetof(CAudioPlayer, m3DDoppler)},
    {MPH_3DGROUPING, INTERFACE_GAME,
        offsetof(CAudioPlayer, m3DGrouping)},
    {MPH_3DLOCATION, INTERFACE_GAME,
        offsetof(CAudioPlayer, m3DLocation)},
    {MPH_3DSOURCE, INTERFACE_GAME,
        offsetof(CAudioPlayer, m3DSource)},
    // FIXME Currently we create an internal buffer queue for playing files
    {MPH_BUFFERQUEUE, /* INTERFACE_GAME */ INTERFACE_IMPLICIT,
        offsetof(CAudioPlayer, mBufferQueue)},
    {MPH_EFFECTSEND, INTERFACE_MUSIC_GAME,
        offsetof(CAudioPlayer, mEffectSend)},
    {MPH_MUTESOLO, INTERFACE_GAME,
        offsetof(CAudioPlayer, mMuteSolo)},
    {MPH_METADATAEXTRACTION, INTERFACE_MUSIC_GAME,
        offsetof(CAudioPlayer, mMetadataExtraction)},
    {MPH_METADATATRAVERSAL, INTERFACE_MUSIC_GAME,
        offsetof(CAudioPlayer, mMetadataTraversal)},
    {MPH_PREFETCHSTATUS, INTERFACE_TBD,
        offsetof(CAudioPlayer, mPrefetchStatus)},
    {MPH_RATEPITCH, INTERFACE_DYNAMIC_GAME,
        offsetof(CAudioPlayer, mRatePitch)},
    {MPH_SEEK, INTERFACE_TBD,
        offsetof(CAudioPlayer, mSeek)},
    {MPH_VOLUME, INTERFACE_TBD,
        offsetof(CAudioPlayer, mVolume)},
    {MPH_3DMACROSCOPIC, INTERFACE_OPTIONAL,
        offsetof(CAudioPlayer, m3DMacroscopic)},
    {MPH_BASSBOOST, INTERFACE_OPTIONAL,
        offsetof(CAudioPlayer, mBassBoost)},
    {MPH_DYNAMICSOURCE, INTERFACE_OPTIONAL,
        offsetof(CAudioPlayer, mDynamicSource)},
    {MPH_ENVIRONMENTALREVERB, INTERFACE_OPTIONAL,
        offsetof(CAudioPlayer, mEnvironmentalReverb)},
    {MPH_EQUALIZER, INTERFACE_OPTIONAL,
        offsetof(CAudioPlayer, mEqualizer)},
    {MPH_PITCH, INTERFACE_OPTIONAL,
        offsetof(CAudioPlayer, mPitch)},
    {MPH_PRESETREVERB, INTERFACE_OPTIONAL,
        offsetof(CAudioPlayer, mPresetReverb)},
    {MPH_PLAYBACKRATE, INTERFACE_OPTIONAL,
        offsetof(CAudioPlayer, mPlaybackRate)},
    {MPH_VIRTUALIZER, INTERFACE_OPTIONAL,
        offsetof(CAudioPlayer, mVirtualizer)},
    {MPH_VISUALIZATION, INTERFACE_OPTIONAL,
        offsetof(CAudioPlayer, mVisualization)}
};

static const ClassTable CAudioPlayer_class = {
    AudioPlayer_interfaces,
    sizeof(AudioPlayer_interfaces)/sizeof(AudioPlayer_interfaces[0]),
    MPH_to_AudioPlayer,
    //"AudioPlayer",
    sizeof(CAudioPlayer),
    SL_OBJECTID_AUDIOPLAYER,
    AudioPlayer_Realize,
    NULL /*AudioPlayer_Resume*/,
    AudioPlayer_Destroy
};

// AudioRecorder class

static const struct iid_vtable AudioRecorder_interfaces[] = {
    {MPH_OBJECT, INTERFACE_IMPLICIT,
        offsetof(CAudioRecorder, mObject)},
    {MPH_DYNAMICINTERFACEMANAGEMENT, INTERFACE_IMPLICIT,
        offsetof(CAudioRecorder, mDynamicInterfaceManagement)},
    {MPH_RECORD, INTERFACE_IMPLICIT,
        offsetof(CAudioRecorder, mRecord)},
    {MPH_AUDIOENCODER, INTERFACE_TBD,
        offsetof(CAudioRecorder, mAudioEncoder)},
    {MPH_BASSBOOST, INTERFACE_OPTIONAL,
        offsetof(CAudioRecorder, mBassBoost)},
    {MPH_DYNAMICSOURCE, INTERFACE_OPTIONAL,
        offsetof(CAudioRecorder, mDynamicSource)},
    {MPH_EQUALIZER, INTERFACE_OPTIONAL,
        offsetof(CAudioRecorder, mEqualizer)},
    {MPH_VISUALIZATION, INTERFACE_OPTIONAL,
        offsetof(CAudioRecorder, mVisualization)},
    {MPH_VOLUME, INTERFACE_OPTIONAL,
        offsetof(CAudioRecorder, mVolume)}
};

static const ClassTable CAudioRecorder_class = {
    AudioRecorder_interfaces,
    sizeof(AudioRecorder_interfaces)/sizeof(AudioRecorder_interfaces[0]),
    MPH_to_AudioRecorder,
    //"AudioRecorder",
    sizeof(CAudioRecorder),
    SL_OBJECTID_AUDIORECORDER,
    NULL,
    NULL,
    NULL
};

// Engine class

static const struct iid_vtable Engine_interfaces[] = {
    {MPH_OBJECT, INTERFACE_IMPLICIT,
        offsetof(CEngine, mObject)},
    {MPH_DYNAMICINTERFACEMANAGEMENT, INTERFACE_IMPLICIT,
        offsetof(CEngine, mDynamicInterfaceManagement)},
    {MPH_ENGINE, INTERFACE_IMPLICIT,
        offsetof(CEngine, mEngine)},
    {MPH_ENGINECAPABILITIES, INTERFACE_IMPLICIT,
        offsetof(CEngine, mEngineCapabilities)},
    {MPH_THREADSYNC, INTERFACE_IMPLICIT,
        offsetof(CEngine, mThreadSync)},
    {MPH_AUDIOIODEVICECAPABILITIES, INTERFACE_IMPLICIT,
        offsetof(CEngine, mAudioIODeviceCapabilities)},
    {MPH_AUDIODECODERCAPABILITIES, INTERFACE_EXPLICIT,
        offsetof(CEngine, mAudioDecoderCapabilities)},
    {MPH_AUDIOENCODERCAPABILITIES, INTERFACE_EXPLICIT,
        offsetof(CEngine, mAudioEncoderCapabilities)},
    {MPH_3DCOMMIT, INTERFACE_EXPLICIT_GAME,
        offsetof(CEngine, m3DCommit)},
    {MPH_DEVICEVOLUME, INTERFACE_OPTIONAL,
        offsetof(CEngine, mDeviceVolume)}
};

static const ClassTable CEngine_class = {
    Engine_interfaces,
    sizeof(Engine_interfaces)/sizeof(Engine_interfaces[0]),
    MPH_to_Engine,
    //"Engine",
    sizeof(CEngine),
    SL_OBJECTID_ENGINE,
    NULL,
    NULL,
    NULL
};

// LEDDevice class

static const struct iid_vtable LEDDevice_interfaces[] = {
    {MPH_OBJECT, INTERFACE_IMPLICIT,
        offsetof(CLEDDevice, mObject)},
    {MPH_DYNAMICINTERFACEMANAGEMENT, INTERFACE_IMPLICIT,
        offsetof(CLEDDevice, mDynamicInterfaceManagement)},
    {MPH_LED, INTERFACE_IMPLICIT,
        offsetof(CLEDDevice, mLEDArray)}
};

static const ClassTable CLEDDevice_class = {
    LEDDevice_interfaces,
    sizeof(LEDDevice_interfaces)/sizeof(LEDDevice_interfaces[0]),
    MPH_to_LEDDevice,
    //"LEDDevice",
    sizeof(CLEDDevice),
    SL_OBJECTID_LEDDEVICE,
    NULL,
    NULL,
    NULL
};

// Listener class

static const struct iid_vtable Listener_interfaces[] = {
    {MPH_OBJECT, INTERFACE_IMPLICIT,
        offsetof(CListener, mObject)},
    {MPH_DYNAMICINTERFACEMANAGEMENT, INTERFACE_IMPLICIT,
        offsetof(CListener, mDynamicInterfaceManagement)},
    {MPH_3DDOPPLER, INTERFACE_DYNAMIC_GAME,
        offsetof(C3DGroup, m3DDoppler)},
    {MPH_3DLOCATION, INTERFACE_EXPLICIT_GAME,
        offsetof(C3DGroup, m3DLocation)}
};

static const ClassTable CListener_class = {
    Listener_interfaces,
    sizeof(Listener_interfaces)/sizeof(Listener_interfaces[0]),
    MPH_to_Listener,
    //"Listener",
    sizeof(CListener),
    SL_OBJECTID_LISTENER,
    NULL,
    NULL,
    NULL
};

// MetadataExtractor class

static const struct iid_vtable MetadataExtractor_interfaces[] = {
    {MPH_OBJECT, INTERFACE_IMPLICIT,
        offsetof(CMetadataExtractor, mObject)},
    {MPH_DYNAMICINTERFACEMANAGEMENT, INTERFACE_IMPLICIT,
        offsetof(CMetadataExtractor, mDynamicInterfaceManagement)},
    {MPH_DYNAMICSOURCE, INTERFACE_IMPLICIT,
        offsetof(CMetadataExtractor, mDynamicSource)},
    {MPH_METADATAEXTRACTION, INTERFACE_IMPLICIT,
        offsetof(CMetadataExtractor, mMetadataExtraction)},
    {MPH_METADATATRAVERSAL, INTERFACE_IMPLICIT,
        offsetof(CMetadataExtractor, mMetadataTraversal)}
};

static const ClassTable CMetadataExtractor_class = {
    MetadataExtractor_interfaces,
    sizeof(MetadataExtractor_interfaces) /
        sizeof(MetadataExtractor_interfaces[0]),
    MPH_to_MetadataExtractor,
    //"MetadataExtractor",
    sizeof(CMetadataExtractor),
    SL_OBJECTID_METADATAEXTRACTOR,
    NULL,
    NULL,
    NULL
};

// MidiPlayer class

static const struct iid_vtable MidiPlayer_interfaces[] = {
    {MPH_OBJECT, INTERFACE_IMPLICIT,
        offsetof(CMidiPlayer, mObject)},
    {MPH_DYNAMICINTERFACEMANAGEMENT, INTERFACE_IMPLICIT,
        offsetof(CMidiPlayer, mDynamicInterfaceManagement)},
    {MPH_PLAY, INTERFACE_IMPLICIT,
        offsetof(CMidiPlayer, mPlay)},
    {MPH_3DDOPPLER, INTERFACE_DYNAMIC_GAME,
        offsetof(C3DGroup, m3DDoppler)},
    {MPH_3DGROUPING, INTERFACE_GAME,
        offsetof(CMidiPlayer, m3DGrouping)},
    {MPH_3DLOCATION, INTERFACE_GAME,
        offsetof(CMidiPlayer, m3DLocation)},
    {MPH_3DSOURCE, INTERFACE_GAME,
        offsetof(CMidiPlayer, m3DSource)},
    {MPH_BUFFERQUEUE, INTERFACE_GAME,
        offsetof(CMidiPlayer, mBufferQueue)},
    {MPH_EFFECTSEND, INTERFACE_GAME,
        offsetof(CMidiPlayer, mEffectSend)},
    {MPH_MUTESOLO, INTERFACE_GAME,
        offsetof(CMidiPlayer, mMuteSolo)},
    {MPH_METADATAEXTRACTION, INTERFACE_GAME,
        offsetof(CMidiPlayer, mMetadataExtraction)},
    {MPH_METADATATRAVERSAL, INTERFACE_GAME,
        offsetof(CMidiPlayer, mMetadataTraversal)},
    {MPH_MIDIMESSAGE, INTERFACE_PHONE_GAME,
        offsetof(CMidiPlayer, mMIDIMessage)},
    {MPH_MIDITIME, INTERFACE_PHONE_GAME,
        offsetof(CMidiPlayer, mMIDITime)},
    {MPH_MIDITEMPO, INTERFACE_PHONE_GAME,
        offsetof(CMidiPlayer, mMIDITempo)},
    {MPH_MIDIMUTESOLO, INTERFACE_GAME,
        offsetof(CMidiPlayer, mMIDIMuteSolo)},
    {MPH_PREFETCHSTATUS, INTERFACE_PHONE_GAME,
        offsetof(CMidiPlayer, mPrefetchStatus)},
    {MPH_SEEK, INTERFACE_PHONE_GAME,
        offsetof(CMidiPlayer, mSeek)},
    {MPH_VOLUME, INTERFACE_PHONE_GAME,
        offsetof(CMidiPlayer, mVolume)},
    {MPH_3DMACROSCOPIC, INTERFACE_OPTIONAL,
        offsetof(CMidiPlayer, m3DMacroscopic)},
    {MPH_BASSBOOST, INTERFACE_OPTIONAL,
        offsetof(CMidiPlayer, mBassBoost)},
    {MPH_DYNAMICSOURCE, INTERFACE_OPTIONAL,
        offsetof(CMidiPlayer, mDynamicSource)},
    {MPH_ENVIRONMENTALREVERB, INTERFACE_OPTIONAL,
        offsetof(CMidiPlayer, mEnvironmentalReverb)},
    {MPH_EQUALIZER, INTERFACE_OPTIONAL,
        offsetof(CMidiPlayer, mEqualizer)},
    {MPH_PITCH, INTERFACE_OPTIONAL,
        offsetof(CMidiPlayer, mPitch)},
    {MPH_PRESETREVERB, INTERFACE_OPTIONAL,
        offsetof(CMidiPlayer, mPresetReverb)},
    {MPH_PLAYBACKRATE, INTERFACE_OPTIONAL,
        offsetof(CMidiPlayer, mPlaybackRate)},
    {MPH_VIRTUALIZER, INTERFACE_OPTIONAL,
        offsetof(CMidiPlayer, mVirtualizer)},
    {MPH_VISUALIZATION, INTERFACE_OPTIONAL,
        offsetof(CMidiPlayer, mVisualization)}
};

static const ClassTable CMidiPlayer_class = {
    MidiPlayer_interfaces,
    sizeof(MidiPlayer_interfaces)/sizeof(MidiPlayer_interfaces[0]),
    MPH_to_MidiPlayer,
    //"MidiPlayer",
    sizeof(CMidiPlayer),
    SL_OBJECTID_MIDIPLAYER,
    NULL,
    NULL,
    NULL
};

// OutputMix class

static const struct iid_vtable OutputMix_interfaces[] = {
    {MPH_OBJECT, INTERFACE_IMPLICIT,
        offsetof(COutputMix, mObject)},
    {MPH_DYNAMICINTERFACEMANAGEMENT, INTERFACE_IMPLICIT,
        offsetof(COutputMix, mDynamicInterfaceManagement)},
    {MPH_OUTPUTMIX, INTERFACE_IMPLICIT,
        offsetof(COutputMix, mOutputMix)},
#ifdef USE_OUTPUTMIXEXT
    {MPH_OUTPUTMIXEXT, INTERFACE_IMPLICIT,
        offsetof(COutputMix, mOutputMixExt)},
#else
    {MPH_OUTPUTMIXEXT, INTERFACE_TBD /*NOT AVAIL*/, 0},
#endif
    {MPH_ENVIRONMENTALREVERB, INTERFACE_DYNAMIC_GAME,
        offsetof(COutputMix, mEnvironmentalReverb)},
    {MPH_EQUALIZER, INTERFACE_DYNAMIC_MUSIC_GAME,
        offsetof(COutputMix, mEqualizer)},
    {MPH_PRESETREVERB, INTERFACE_DYNAMIC_MUSIC,
        offsetof(COutputMix, mPresetReverb)},
    {MPH_VIRTUALIZER, INTERFACE_DYNAMIC_MUSIC_GAME,
        offsetof(COutputMix, mVirtualizer)},
    {MPH_VOLUME, INTERFACE_GAME_MUSIC,
        offsetof(COutputMix, mVolume)},
    {MPH_BASSBOOST, INTERFACE_OPTIONAL_DYNAMIC,
        offsetof(COutputMix, mBassBoost)},
    {MPH_VISUALIZATION, INTERFACE_OPTIONAL,
        offsetof(COutputMix, mVisualization)}
};

static const ClassTable COutputMix_class = {
    OutputMix_interfaces,
    sizeof(OutputMix_interfaces)/sizeof(OutputMix_interfaces[0]),
    MPH_to_OutputMix,
    //"OutputMix",
    sizeof(COutputMix),
    SL_OBJECTID_OUTPUTMIX,
    NULL,
    NULL,
    NULL
};

// Vibra class

static const struct iid_vtable VibraDevice_interfaces[] = {
    {MPH_OBJECT, INTERFACE_OPTIONAL,
        offsetof(CVibraDevice, mObject)},
    {MPH_DYNAMICINTERFACEMANAGEMENT, INTERFACE_OPTIONAL,
        offsetof(CVibraDevice, mDynamicInterfaceManagement)},
    {MPH_VIBRA, INTERFACE_OPTIONAL,
        offsetof(CVibraDevice, mVibra)}
};

static const ClassTable CVibraDevice_class = {
    VibraDevice_interfaces,
    sizeof(VibraDevice_interfaces)/sizeof(VibraDevice_interfaces[0]),
    MPH_to_Vibra,
    //"VibraDevice",
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
    &CAudioPlayer_class,
    &CAudioRecorder_class,
    &CMidiPlayer_class,
    &CListener_class,
    &C3DGroup_class,
    &CVibraDevice_class,
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

// Construct a new instance of the specified class, exposing selected interfaces

IObject *construct(const ClassTable *class__,
    unsigned exposedMask, SLEngineItf engine)
{
    IObject *this;
#ifndef NDEBUG
    this = (IObject *) malloc(class__->mSize);
#else
    this = (IObject *) calloc(1, class__->mSize);
#endif
    if (NULL != this) {
#ifndef NDEBUG
        // for debugging, to detect uninitialized fields
        memset(this, 0x55, class__->mSize);
#endif
        this->mClass = class__;
        this->mExposedMask = exposedMask;
        unsigned lossOfControlMask = 0;
        IEngine *thisEngine = (IEngine *) engine;
        if (NULL == thisEngine)
            thisEngine = &((CEngine *) this)->mEngine;
        else if (thisEngine->mLossOfControlGlobal)
            lossOfControlMask = ~0;
        this->mLossOfControlMask = lossOfControlMask;
        const struct iid_vtable *x = class__->mInterfaces;
        unsigned i;
        for (i = 0; exposedMask; ++i, ++x, exposedMask >>= 1) {
            if (exposedMask & 1) {
                void *self = (char *) this + x->mOffset;
                ((IObject **) self)[1] = this;
                VoidHook init = MPH_init_table[x->mMPH].mInit;
                if (NULL != init)
                    (*init)(self);
            }
        }
        // FIXME need a lock
        if (INSTANCE_MAX > thisEngine->mInstanceCount) {
            // FIXME ignores Destroy
            thisEngine->mInstances[thisEngine->mInstanceCount++] = this;
        }
        // FIXME else what
    }
    return this;
}

// Runs every graphics frame (50 Hz) to update audio

static void *frame_body(void *arg)
{
    CEngine *this = (CEngine *) arg;
    for (;;) {
        usleep(20000*50);
        unsigned i;
        for (i = 0; i < INSTANCE_MAX; ++i) {
            IObject *instance = (IObject *) this->mEngine.mInstances[i];
            if (NULL == instance)
                continue;
            if (instance->mClass != &CAudioPlayer_class)
                continue;
            write(1, ".", 1);
        }
    }
    return NULL;
}

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
    SLresult result = checkInterfaces(&CEngine_class, numInterfaces,
        pInterfaceIds, pInterfaceRequired, &exposedMask);
    if (SL_RESULT_SUCCESS != result)
        return result;
    CEngine *this = (CEngine *)
        construct(&CEngine_class, exposedMask, NULL);
    if (NULL == this)
        return SL_RESULT_MEMORY_FAILURE;
    this->mObject.mLossOfControlMask = lossOfControlGlobal ? ~0 : 0;
    this->mEngine.mLossOfControlGlobal = lossOfControlGlobal;
    this->mEngineCapabilities.mThreadSafe = threadSafe;
    int ok;
    ok = pthread_create(&this->mFrameThread, (const pthread_attr_t *) NULL,
        frame_body, this);
    assert(ok == 0);
    *pEngine = &this->mObject.mItf;
    return SL_RESULT_SUCCESS;
}

SLresult SLAPIENTRY slQueryNumSupportedEngineInterfaces(
    SLuint32 *pNumSupportedInterfaces)
{
    if (NULL == pNumSupportedInterfaces)
        return SL_RESULT_PARAMETER_INVALID;
    *pNumSupportedInterfaces = CEngine_class.mInterfaceCount;
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
    IOutputMixExt *thisExt =
        (IOutputMixExt *) self;
    // FIXME Finding one interface from another, but is it exposed?
    IOutputMix *this =
        &((COutputMix *) thisExt->mThis)->mOutputMix;
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
        IPlay *play = track->mPlay;
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
            IBufferQueue *bufferQueue;
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
    //IObject *this = (IObject *) self;
    //assert(&COutputMix_class == this->mClass);
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
    // fmt.userdata = &((COutputMix *) this)->mOutputMixExt;
    fmt.userdata = (void *) OutputMixExt;

    if (SDL_OpenAudio(&fmt, NULL) < 0) {
        fprintf(stderr, "Unable to open audio: %s\n", SDL_GetError());
        exit(1);
    }
    SDL_PauseAudio(0);
}

#endif // USE_SDL

/* End */
