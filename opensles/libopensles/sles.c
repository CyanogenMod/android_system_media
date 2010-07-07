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

/* Private functions */

// Map an IObject to it's "object ID" (which is really a class ID)

SLuint32 IObjectToObjectID(IObject *this)
{
    assert(NULL != this);
    return this->mClass->mObjectID;
}

// Convert POSIX pthread error code to OpenSL ES result code

SLresult err_to_result(int err)
{
    if (EAGAIN == err || ENOMEM == err)
        return SL_RESULT_RESOURCE_ERROR;
    if (0 != err)
        return SL_RESULT_INTERNAL_ERROR;
    return SL_RESULT_SUCCESS;
}

// Check the interface IDs passed into a Create operation

SLresult checkInterfaces(const ClassTable *class__, SLuint32 numInterfaces,
    const SLInterfaceID *pInterfaceIds, const SLboolean *pInterfaceRequired, unsigned *pExposedMask)
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
            // can we request dynamic interfaces here?
            exposedMask |= (1 << index);
        }
    }
    *pExposedMask = exposedMask;
    return SL_RESULT_SUCCESS;
}

// private helper shared by decoder and encoder
SLresult GetCodecCapabilities(SLuint32 codecId, SLuint32 *pIndex,
    SLAudioCodecDescriptor *pDescriptor, const struct CodecDescriptor *codecDescriptors)
{
    if (NULL == pIndex)
        return SL_RESULT_PARAMETER_INVALID;
    const struct CodecDescriptor *cd = codecDescriptors;
    SLuint32 index;
    if (NULL == pDescriptor) {
        for (index = 0 ; NULL != cd->mDescriptor; ++cd)
            if (cd->mCodecID == codecId)
                ++index;
        *pIndex = index;
        return SL_RESULT_SUCCESS;
    }
    index = *pIndex;
    for ( ; NULL != cd->mDescriptor; ++cd) {
        if (cd->mCodecID == codecId) {
            if (0 == index) {
                *pDescriptor = *cd->mDescriptor;
#if 0   // FIXME Temporary workaround for Khronos bug 6331
                if (0 < pDescriptor->numSampleRatesSupported) {
                    // FIXME The malloc is not in the 1.0.1 specification
                    SLmilliHertz *temp = (SLmilliHertz *) malloc(sizeof(SLmilliHertz) *
                        pDescriptor->numSampleRatesSupported);
                    assert(NULL != temp);
                    memcpy(temp, pDescriptor->pSampleRatesSupported, sizeof(SLmilliHertz) *
                        pDescriptor->numSampleRatesSupported);
                    pDescriptor->pSampleRatesSupported = temp;
                } else {
                    pDescriptor->pSampleRatesSupported = NULL;
                }
#endif
                return SL_RESULT_SUCCESS;
            }
            --index;
        }
    }
    return SL_RESULT_PARAMETER_INVALID;
}

static SLresult checkDataLocator(void *pLocator, DataLocator *pDataLocator)
{
    if (NULL == pLocator) {
        pDataLocator->mLocatorType = SL_DATALOCATOR_NULL;
        return SL_RESULT_SUCCESS;
    }
    SLuint32 locatorType = *(SLuint32 *)pLocator;
    switch (locatorType) {
    case SL_DATALOCATOR_ADDRESS:
        pDataLocator->mAddress = *(SLDataLocator_Address *)pLocator;
        if (0 < pDataLocator->mAddress.length && NULL == pDataLocator->mAddress.pAddress)
            return SL_RESULT_PARAMETER_INVALID;
        break;
    case SL_DATALOCATOR_BUFFERQUEUE:
        pDataLocator->mBufferQueue = *(SLDataLocator_BufferQueue *)pLocator;
        if (0 == pDataLocator->mBufferQueue.numBuffers)
            return SL_RESULT_PARAMETER_INVALID;
            // pDataLocator->mBufferQueue.numBuffers = 2;
        // FIXME check against a max
        break;
    case SL_DATALOCATOR_IODEVICE:
        {
        pDataLocator->mIODevice = *(SLDataLocator_IODevice *)pLocator;
        SLuint32 deviceType = pDataLocator->mIODevice.deviceType;
        if (NULL != pDataLocator->mIODevice.device) {
            switch (IObjectToObjectID((IObject *) pDataLocator->mIODevice.device)) {
            case SL_OBJECTID_LEDDEVICE:
                if (SL_IODEVICE_LEDARRAY != deviceType)
                    return SL_RESULT_PARAMETER_INVALID;
                break;
            case SL_OBJECTID_VIBRADEVICE:
                if (SL_IODEVICE_VIBRA != deviceType)
                    return SL_RESULT_PARAMETER_INVALID;
                break;
            default:
                return SL_RESULT_PARAMETER_INVALID;
            }
            pDataLocator->mIODevice.deviceID = 0;
        } else {
            switch (pDataLocator->mIODevice.deviceID) {
            case SL_DEFAULTDEVICEID_LED:
                if (SL_IODEVICE_LEDARRAY != deviceType)
                    return SL_RESULT_PARAMETER_INVALID;
                break;
            case SL_DEFAULTDEVICEID_VIBRA:
                if (SL_IODEVICE_VIBRA != deviceType)
                    return SL_RESULT_PARAMETER_INVALID;
                break;
            case SL_DEFAULTDEVICEID_AUDIOINPUT:
                if (SL_IODEVICE_AUDIOINPUT != deviceType)
                    return SL_RESULT_PARAMETER_INVALID;
                break;
            default:
                return SL_RESULT_PARAMETER_INVALID;
            }
        }
        }
        break;
    case SL_DATALOCATOR_MIDIBUFFERQUEUE:
        pDataLocator->mMIDIBufferQueue = *(SLDataLocator_MIDIBufferQueue *)pLocator;
        if (0 == pDataLocator->mMIDIBufferQueue.tpqn)
            pDataLocator->mMIDIBufferQueue.tpqn = 192;
        if (0 == pDataLocator->mMIDIBufferQueue.numBuffers)
            pDataLocator->mMIDIBufferQueue.numBuffers = 2;
        break;
    case SL_DATALOCATOR_OUTPUTMIX:
        pDataLocator->mOutputMix = *(SLDataLocator_OutputMix *)pLocator;
        if ((NULL == pDataLocator->mOutputMix.outputMix)
                || (SL_OBJECTID_OUTPUTMIX
                        != IObjectToObjectID((IObject *) pDataLocator->mOutputMix.outputMix)))
            return SL_RESULT_PARAMETER_INVALID;
        break;
    case SL_DATALOCATOR_URI:
        {
        pDataLocator->mURI = *(SLDataLocator_URI *)pLocator;
        if (NULL == pDataLocator->mURI.URI)
            return SL_RESULT_PARAMETER_INVALID;
        size_t len = strlen((const char *) pDataLocator->mURI.URI);
        SLchar *myURI = (SLchar *) malloc(len + 1);
        if (NULL == myURI)
            return SL_RESULT_MEMORY_FAILURE;
        memcpy(myURI, pDataLocator->mURI.URI, len + 1);
        // Verify that another thread didn't change the NUL-terminator after we used it
        // to determine length of string to copy. It's OK if the string became shorter.
        if ('\0' != myURI[len]) {
            free(myURI);
            return SL_RESULT_PARAMETER_INVALID;
        }
        pDataLocator->mURI.URI = myURI;
        }
        break;
#ifdef ANDROID
    case SL_DATALOCATOR_ANDROIDFD:
        {
        pDataLocator->mFD = *(SLDataLocator_AndroidFD *)pLocator;
        fprintf(stdout, "Data locator FD: fd=%ld offset=%lld length=%lld\n", pDataLocator->mFD.fd,
                pDataLocator->mFD.offset, pDataLocator->mFD.length);
        if (-1 == pDataLocator->mFD.fd) {
            return SL_RESULT_PARAMETER_INVALID;
        }
        }
        break;
#endif
    default:
        return SL_RESULT_PARAMETER_INVALID;
    }
    // Verify that another thread didn't change the locatorType field after we used it
    // to determine sizeof struct to copy.
    if (locatorType != pDataLocator->mLocatorType) {
        return SL_RESULT_PARAMETER_INVALID;
    }
    return SL_RESULT_SUCCESS;
}

static void freeDataLocator(DataLocator *pDataLocator)
{
    switch (pDataLocator->mLocatorType) {
    case SL_DATALOCATOR_URI:
        free(pDataLocator->mURI.URI);
        pDataLocator->mURI.URI = NULL;
        break;
    default:
        break;
    }
}

static SLresult checkDataFormat(void *pFormat, DataFormat *pDataFormat)
{
    if (NULL == pFormat) {
        pDataFormat->mFormatType = SL_DATAFORMAT_NULL;
        return SL_RESULT_SUCCESS;
    }
    SLuint32 formatType = *(SLuint32 *)pFormat;
    switch (formatType) {
    case SL_DATAFORMAT_PCM:
        pDataFormat->mPCM = *(SLDataFormat_PCM *)pFormat;
        switch (pDataFormat->mPCM.numChannels) {
        case 1:
        case 2:
            break;
        case 0:
            return SL_RESULT_PARAMETER_INVALID;
        default:
            return SL_RESULT_CONTENT_UNSUPPORTED;
        }
        switch (pDataFormat->mPCM.samplesPerSec) {
        case SL_SAMPLINGRATE_8:
        case SL_SAMPLINGRATE_11_025:
        case SL_SAMPLINGRATE_12:
        case SL_SAMPLINGRATE_16:
        case SL_SAMPLINGRATE_22_05:
        case SL_SAMPLINGRATE_24:
        case SL_SAMPLINGRATE_32:
        case SL_SAMPLINGRATE_44_1:
        case SL_SAMPLINGRATE_48:
        case SL_SAMPLINGRATE_64:
        case SL_SAMPLINGRATE_88_2:
        case SL_SAMPLINGRATE_96:
        case SL_SAMPLINGRATE_192:
            break;
        case 0:
            return SL_RESULT_PARAMETER_INVALID;
        default:
            return SL_RESULT_CONTENT_UNSUPPORTED;
        }
        switch (pDataFormat->mPCM.bitsPerSample) {
        case SL_PCMSAMPLEFORMAT_FIXED_8:
        case SL_PCMSAMPLEFORMAT_FIXED_16:
            break;
        case SL_PCMSAMPLEFORMAT_FIXED_20:
        case SL_PCMSAMPLEFORMAT_FIXED_24:
        case SL_PCMSAMPLEFORMAT_FIXED_28:
        case SL_PCMSAMPLEFORMAT_FIXED_32:
            return SL_RESULT_CONTENT_UNSUPPORTED;
        default:
            return SL_RESULT_PARAMETER_INVALID;
        }
        switch (pDataFormat->mPCM.containerSize) {
        case SL_PCMSAMPLEFORMAT_FIXED_8:
        case SL_PCMSAMPLEFORMAT_FIXED_16:
            if (pDataFormat->mPCM.containerSize != pDataFormat->mPCM.bitsPerSample)
                return SL_RESULT_CONTENT_UNSUPPORTED;
            break;
        default:
            return SL_RESULT_CONTENT_UNSUPPORTED;
        }
        switch (pDataFormat->mPCM.channelMask) {
        case SL_SPEAKER_FRONT_LEFT | SL_SPEAKER_FRONT_RIGHT:
            if (2 != pDataFormat->mPCM.numChannels)
                return SL_RESULT_PARAMETER_INVALID;
            break;
        case SL_SPEAKER_FRONT_LEFT:
        case SL_SPEAKER_FRONT_RIGHT:
        case SL_SPEAKER_FRONT_CENTER:
            if (1 != pDataFormat->mPCM.numChannels)
                return SL_RESULT_PARAMETER_INVALID;
            break;
        case 0:
            pDataFormat->mPCM.channelMask = pDataFormat->mPCM.numChannels == 2 ?
                SL_SPEAKER_FRONT_LEFT | SL_SPEAKER_FRONT_RIGHT : SL_SPEAKER_FRONT_CENTER;
            break;
        default:
            return SL_RESULT_PARAMETER_INVALID;
        }
        switch (pDataFormat->mPCM.endianness) {
        case SL_BYTEORDER_LITTLEENDIAN:
            break;
        case SL_BYTEORDER_BIGENDIAN:
            return SL_RESULT_CONTENT_UNSUPPORTED;
        default:
            return SL_RESULT_PARAMETER_INVALID;
        }
        break;
    case SL_DATAFORMAT_MIME:
        {
        pDataFormat->mMIME = *(SLDataFormat_MIME *)pFormat;
        if (NULL == pDataFormat->mMIME.mimeType)
            return SL_RESULT_SUCCESS;
        size_t len = strlen((const char *) pDataFormat->mMIME.mimeType);
        SLchar *myMIME = (SLchar *) malloc(len + 1);
        if (NULL == myMIME)
            return SL_RESULT_MEMORY_FAILURE;
        memcpy(myMIME, pDataFormat->mMIME.mimeType, len + 1);
        // ditto
        if ('\0' != myMIME[len]) {
            free(myMIME);
            return SL_RESULT_PARAMETER_INVALID;
        }
        pDataFormat->mMIME.mimeType = myMIME;
        }
        break;
    default:
        return SL_RESULT_PARAMETER_INVALID;
    }
    if (formatType != pDataFormat->mFormatType)
        return SL_RESULT_PARAMETER_INVALID;
    return SL_RESULT_SUCCESS;
}

SLresult checkSourceFormatVsInterfacesCompatibility(const DataLocatorFormat *pDataLocatorFormat,
        SLuint32 numInterfaces, const SLInterfaceID *pInterfaceIds,
            const SLboolean *pInterfaceRequired) {
    // can't request SLSeekItf if data source is a buffer queue
    if (SL_DATALOCATOR_BUFFERQUEUE == pDataLocatorFormat->mLocator.mLocatorType) {
        for (int i = 0; i < numInterfaces; i++) {
            if (pInterfaceRequired[i] && (SL_IID_SEEK == pInterfaceIds[i])) {
                fprintf(stderr, "Error: can't request SL_IID_SEEK with a buffer queue data source\n");
                return SL_RESULT_FEATURE_UNSUPPORTED;
            }
        }
    }
    return SL_RESULT_SUCCESS;
}

static void freeDataFormat(DataFormat *pDataFormat)
{
    switch (pDataFormat->mFormatType) {
    case SL_DATAFORMAT_MIME:
        free(pDataFormat->mMIME.mimeType);
        pDataFormat->mMIME.mimeType = NULL;
        break;
    default:
        break;
    }
}

SLresult checkDataSource(const SLDataSource *pDataSrc, DataLocatorFormat *pDataLocatorFormat)
{
    if (NULL == pDataSrc)
        return SL_RESULT_PARAMETER_INVALID;
    SLDataSource myDataSrc = *pDataSrc;
    SLresult result;
    result = checkDataLocator(myDataSrc.pLocator, &pDataLocatorFormat->mLocator);
    if (SL_RESULT_SUCCESS != result)
        return result;
    switch (pDataLocatorFormat->mLocator.mLocatorType) {
    case SL_DATALOCATOR_URI:
    case SL_DATALOCATOR_ADDRESS:
    case SL_DATALOCATOR_BUFFERQUEUE:
    case SL_DATALOCATOR_MIDIBUFFERQUEUE:
        result = checkDataFormat(myDataSrc.pFormat, &pDataLocatorFormat->mFormat);
        if (SL_RESULT_SUCCESS != result) {
            freeDataLocator(&pDataLocatorFormat->mLocator);
            return result;
        }
        break;
    case SL_DATALOCATOR_NULL:
    case SL_DATALOCATOR_OUTPUTMIX:
    default:
        // invalid but fall through; the invalid locator will be caught later
    case SL_DATALOCATOR_IODEVICE:
        // for these data locator types, ignore the pFormat as it might be uninitialized
        pDataLocatorFormat->mFormat.mFormatType = SL_DATAFORMAT_NULL;
        break;
    }
    pDataLocatorFormat->u.mSource.pLocator = &pDataLocatorFormat->mLocator;
    pDataLocatorFormat->u.mSource.pFormat = &pDataLocatorFormat->mFormat;
    return SL_RESULT_SUCCESS;
}

SLresult checkDataSink(const SLDataSink *pDataSink, DataLocatorFormat *pDataLocatorFormat)
{
    if (NULL == pDataSink)
        return SL_RESULT_PARAMETER_INVALID;
    SLDataSink myDataSink = *pDataSink;
    SLresult result;
    result = checkDataLocator(myDataSink.pLocator, &pDataLocatorFormat->mLocator);
    if (SL_RESULT_SUCCESS != result)
        return result;
    switch (pDataLocatorFormat->mLocator.mLocatorType) {
    case SL_DATALOCATOR_URI:
    case SL_DATALOCATOR_ADDRESS:
        result = checkDataFormat(myDataSink.pFormat, &pDataLocatorFormat->mFormat);
        if (SL_RESULT_SUCCESS != result) {
            freeDataLocator(&pDataLocatorFormat->mLocator);
            return result;
        }
        break;
    case SL_DATALOCATOR_NULL:
    case SL_DATALOCATOR_BUFFERQUEUE:
    case SL_DATALOCATOR_MIDIBUFFERQUEUE:
    default:
        // invalid but fall through; the invalid locator will be caught later
    case SL_DATALOCATOR_IODEVICE:
    case SL_DATALOCATOR_OUTPUTMIX:
        // for these data locator types, ignore the pFormat as it might be uninitialized
        pDataLocatorFormat->mFormat.mFormatType = SL_DATAFORMAT_NULL;
        break;
    }
    pDataLocatorFormat->u.mSink.pLocator = &pDataLocatorFormat->mLocator;
    pDataLocatorFormat->u.mSink.pFormat = &pDataLocatorFormat->mFormat;
    return SL_RESULT_SUCCESS;
}

void freeDataLocatorFormat(DataLocatorFormat *dlf)
{
    freeDataLocator(&dlf->mLocator);
    freeDataFormat(&dlf->mFormat);
}

/* Interface initialization hooks */

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
#ifdef ANDROID
extern void
    IAndroidStreamType_init(void *);
#endif

#ifdef USE_OUTPUTMIXEXT
extern void
    IOutputMixExt_init(void *);
#endif

/*static*/ const struct MPH_init MPH_init_table[MPH_MAX] = {
    { /* MPH_3DCOMMIT, */ I3DCommit_init, NULL, NULL },
    { /* MPH_3DDOPPLER, */ I3DDoppler_init, NULL, NULL },
    { /* MPH_3DGROUPING, */ I3DGrouping_init, NULL, NULL },
    { /* MPH_3DLOCATION, */ I3DLocation_init, NULL, NULL },
    { /* MPH_3DMACROSCOPIC, */ I3DMacroscopic_init, NULL, NULL },
    { /* MPH_3DSOURCE, */ I3DSource_init, NULL, NULL },
    { /* MPH_AUDIODECODERCAPABILITIES, */ IAudioDecoderCapabilities_init, NULL, NULL },
    { /* MPH_AUDIOENCODER, */ IAudioEncoder_init, NULL, NULL },
    { /* MPH_AUDIOENCODERCAPABILITIES, */ IAudioEncoderCapabilities_init, NULL, NULL },
    { /* MPH_AUDIOIODEVICECAPABILITIES, */ IAudioIODeviceCapabilities_init, NULL, NULL },
    { /* MPH_BASSBOOST, */ IBassBoost_init, NULL, NULL },
    { /* MPH_BUFFERQUEUE, */ IBufferQueue_init, NULL, NULL },
    { /* MPH_DEVICEVOLUME, */ IDeviceVolume_init, NULL, NULL },
    { /* MPH_DYNAMICINTERFACEMANAGEMENT, */ IDynamicInterfaceManagement_init, NULL, NULL },
    { /* MPH_DYNAMICSOURCE, */ IDynamicSource_init, NULL, NULL },
    { /* MPH_EFFECTSEND, */ IEffectSend_init, NULL, NULL },
    { /* MPH_ENGINE, */ IEngine_init, NULL, NULL },
    { /* MPH_ENGINECAPABILITIES, */ IEngineCapabilities_init, NULL, NULL },
    { /* MPH_ENVIRONMENTALREVERB, */ IEnvironmentalReverb_init, NULL, NULL },
    { /* MPH_EQUALIZER, */ IEqualizer_init, NULL, NULL },
    { /* MPH_LED, */ ILEDArray_init, NULL, NULL },
    { /* MPH_METADATAEXTRACTION, */ IMetadataExtraction_init, NULL, NULL },
    { /* MPH_METADATATRAVERSAL, */ IMetadataTraversal_init, NULL, NULL },
    { /* MPH_MIDIMESSAGE, */ IMIDIMessage_init, NULL, NULL },
    { /* MPH_MIDITIME, */ IMIDITime_init, NULL, NULL },
    { /* MPH_MIDITEMPO, */ IMIDITempo_init, NULL, NULL },
    { /* MPH_MIDIMUTESOLO, */ IMIDIMuteSolo_init, NULL, NULL },
    { /* MPH_MUTESOLO, */ IMuteSolo_init, NULL, NULL },
    { /* MPH_NULL, */ NULL, NULL, NULL },
    { /* MPH_OBJECT, */ IObject_init, NULL, NULL },
    { /* MPH_OUTPUTMIX, */ IOutputMix_init, NULL, NULL },
    { /* MPH_PITCH, */ IPitch_init, NULL, NULL },
    { /* MPH_PLAY, */ IPlay_init, NULL, NULL },
    { /* MPH_PLAYBACKRATE, */ IPlaybackRate_init, NULL, NULL },
    { /* MPH_PREFETCHSTATUS, */ IPrefetchStatus_init, NULL, NULL },
    { /* MPH_PRESETREVERB, */ IPresetReverb_init, NULL, NULL },
    { /* MPH_RATEPITCH, */ IRatePitch_init, NULL, NULL },
    { /* MPH_RECORD, */ IRecord_init, NULL, NULL },
    { /* MPH_SEEK, */ ISeek_init, NULL, NULL },
    { /* MPH_THREADSYNC, */ IThreadSync_init, NULL, NULL },
    { /* MPH_VIBRA, */ IVibra_init, NULL, NULL },
    { /* MPH_VIRTUALIZER, */ IVirtualizer_init, NULL, NULL },
    { /* MPH_VISUALIZATION, */ IVisualization_init, NULL, NULL },
    { /* MPH_VOLUME, */ IVolume_init, NULL, NULL },
#ifdef USE_OUTPUTMIXEXT
    { /* MPH_OUTPUTMIXEXT, */ IOutputMixExt_init, NULL, NULL },
#else
    { /* MPH_OUTPUTMIXEXT, */ NULL, NULL, NULL },
#endif
#ifdef ANDROID
    { /* MPH_ANDROIDSTREAMTYPE */ IAndroidStreamType_init, NULL, NULL }
#else
    { /* MPH_ANDROIDSTREAMTYPE */ NULL, NULL, NULL }
#endif
};

// Construct a new instance of the specified class, exposing selected interfaces

IObject *construct(const ClassTable *class__, unsigned exposedMask, SLEngineItf engine)
{
    IObject *this;
    this = (IObject *) calloc(1, class__->mSize);
    if (NULL != this) {
        unsigned lossOfControlMask = 0;
        // a NULL engine means we are constructing the engine
        IEngine *thisEngine = (IEngine *) engine;
        if (NULL == thisEngine) {
            thisEngine = &((CEngine *) this)->mEngine;
        } else {
            interface_lock_exclusive(thisEngine);
            if (MAX_INSTANCE <= thisEngine->mInstanceCount) {
                // too many objects
                interface_unlock_exclusive(thisEngine);
                free(this);
                return NULL;
            }
            // pre-allocate a pending slot, but don't assign bit from mInstanceMask yet
            ++thisEngine->mInstanceCount;
            assert(((unsigned) ~0) != thisEngine->mInstanceMask);
            interface_unlock_exclusive(thisEngine);
            // const, no lock needed
            if (thisEngine->mLossOfControlGlobal)
                lossOfControlMask = ~0;
        }
        this->mLossOfControlMask = lossOfControlMask;
        this->mClass = class__;
        this->mEngine = thisEngine;
        const struct iid_vtable *x = class__->mInterfaces;
        SLuint8 *interfaceStateP = this->mInterfaceStates;
        SLuint32 index;
        for (index = 0; index < class__->mInterfaceCount; ++index, ++x, exposedMask >>= 1) {
            SLuint8 state;
            if (exposedMask & 1) {
                void *self = (char *) this + x->mOffset;
                // IObject does not have an mThis, so [1] is not always defined
                if (index)
                    ((IObject **) self)[1] = this;
                VoidHook init = MPH_init_table[x->mMPH].mInit;
                if (NULL != init)
                    (*init)(self);
                state = INTERFACE_EXPOSED;
            } else
                state = INTERFACE_UNINITIALIZED;
            *interfaceStateP++ = state;
        }
        // only expose new object to sync thread after it is fully initialized
        interface_lock_exclusive(thisEngine);
        unsigned availMask = ~thisEngine->mInstanceMask;
        assert(availMask);
        unsigned i = ctz(availMask);
        assert(MAX_INSTANCE > i);
        assert(NULL == thisEngine->mInstances[i]);
        thisEngine->mInstances[i] = this;
        thisEngine->mInstanceMask |= 1 << i;
        // avoid zero as a valid instance ID
        this->mInstanceID = i + 1;
#ifdef USE_SDL
        SLboolean unpause = SL_BOOLEAN_FALSE;
        if (SL_OBJECTID_OUTPUTMIX == class__->mObjectID && NULL == thisEngine->mOutputMix) {
            thisEngine->mOutputMix = (COutputMix *) this;
            unpause = SL_BOOLEAN_TRUE;
        }
#endif
        interface_unlock_exclusive(thisEngine);
#ifdef USE_SDL
        if (unpause)
            SDL_PauseAudio(0);
#endif
    }
    return this;
}


/* Initial global entry points */

SLresult SLAPIENTRY slCreateEngine(SLObjectItf *pEngine, SLuint32 numOptions,
    const SLEngineOption *pEngineOptions, SLuint32 numInterfaces,
    const SLInterfaceID *pInterfaceIds, const SLboolean *pInterfaceRequired)
{
    SL_ENTER_GLOBAL

    do {

#ifdef ANDROID
        android::ProcessState::self()->startThreadPool();
        android::DataSource::RegisterDefaultSniffers();
#endif

        if (NULL == pEngine) {
            result = SL_RESULT_PARAMETER_INVALID;
            break;
        }
        *pEngine = NULL;

        if ((0 < numOptions) && (NULL == pEngineOptions)) {
            result = SL_RESULT_PARAMETER_INVALID;
            break;
        }

        // default values
        SLboolean threadSafe = SL_BOOLEAN_TRUE;
        SLboolean lossOfControlGlobal = SL_BOOLEAN_FALSE;

        // process engine options
        SLuint32 i;
        const SLEngineOption *option = pEngineOptions;
        result = SL_RESULT_SUCCESS;
        for (i = 0; i < numOptions; ++i, ++option) {
            switch (option->feature) {
            case SL_ENGINEOPTION_THREADSAFE:
                threadSafe = SL_BOOLEAN_FALSE != (SLboolean) option->data; // normalize
                break;
            case SL_ENGINEOPTION_LOSSOFCONTROL:
                lossOfControlGlobal = SL_BOOLEAN_FALSE != (SLboolean) option->data; // normalize
                break;
            default:
                result = SL_RESULT_PARAMETER_INVALID;
                break;
            }
        }
        if (SL_RESULT_SUCCESS != result)
            break;

        unsigned exposedMask;
        const ClassTable *pCEngine_class = objectIDtoClass(SL_OBJECTID_ENGINE);
        result = checkInterfaces(pCEngine_class, numInterfaces,
            pInterfaceIds, pInterfaceRequired, &exposedMask);
        if (SL_RESULT_SUCCESS != result)
            break;

        CEngine *this = (CEngine *) construct(pCEngine_class, exposedMask, NULL);
        if (NULL == this) {
            result = SL_RESULT_MEMORY_FAILURE;
            break;
        }

        this->mObject.mLossOfControlMask = lossOfControlGlobal ? ~0 : 0;
        this->mEngine.mLossOfControlGlobal = lossOfControlGlobal;
        this->mEngineCapabilities.mThreadSafe = threadSafe;
        *pEngine = &this->mObject.mItf;

    } while(0);

    SL_LEAVE_GLOBAL
}


SLresult SLAPIENTRY slQueryNumSupportedEngineInterfaces(SLuint32 *pNumSupportedInterfaces)
{
    SL_ENTER_GLOBAL

    if (NULL == pNumSupportedInterfaces) {
        result = SL_RESULT_PARAMETER_INVALID;
    } else {
        const ClassTable *pCEngine_class = objectIDtoClass(SL_OBJECTID_ENGINE);
        *pNumSupportedInterfaces = pCEngine_class->mInterfaceCount;
        result = SL_RESULT_SUCCESS;
    }

    SL_LEAVE_GLOBAL
}


SLresult SLAPIENTRY slQuerySupportedEngineInterfaces(SLuint32 index, SLInterfaceID *pInterfaceId)
{
    SL_ENTER_GLOBAL

    if (NULL == pInterfaceId) {
        result = SL_RESULT_PARAMETER_INVALID;
    } else {
        const ClassTable *pCEngine_class = objectIDtoClass(SL_OBJECTID_ENGINE);
        if (pCEngine_class->mInterfaceCount <= index) {
            *pInterfaceId = NULL;
            result = SL_RESULT_PARAMETER_INVALID;
        } else {
            *pInterfaceId = &SL_IID_array[pCEngine_class->mInterfaces[index].mMPH];
            result = SL_RESULT_SUCCESS;
        }
    }

    SL_LEAVE_GLOBAL
}


/* End */
