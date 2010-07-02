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

/* Engine implementation */

#include "sles_allinclusive.h"

static SLresult IEngine_CreateLEDDevice(SLEngineItf self, SLObjectItf *pDevice, SLuint32 deviceID,
    SLuint32 numInterfaces, const SLInterfaceID *pInterfaceIds, const SLboolean *pInterfaceRequired)
{
    if (NULL == pDevice || SL_DEFAULTDEVICEID_LED != deviceID)
        return SL_RESULT_PARAMETER_INVALID;
    *pDevice = NULL;
    unsigned exposedMask;
    const ClassTable *pCLEDDevice_class = objectIDtoClass(SL_OBJECTID_LEDDEVICE);
    SLresult result = checkInterfaces(pCLEDDevice_class, numInterfaces,
        pInterfaceIds, pInterfaceRequired, &exposedMask);
    if (SL_RESULT_SUCCESS != result)
        return result;
    CLEDDevice *this = (CLEDDevice *) construct(pCLEDDevice_class, exposedMask, self);
    if (NULL == this)
        return SL_RESULT_MEMORY_FAILURE;
    this->mDeviceID = deviceID;
    *pDevice = &this->mObject.mItf;
    return SL_RESULT_SUCCESS;
}

static SLresult IEngine_CreateVibraDevice(SLEngineItf self, SLObjectItf *pDevice, SLuint32 deviceID,
    SLuint32 numInterfaces, const SLInterfaceID *pInterfaceIds, const SLboolean *pInterfaceRequired)
{
    if (NULL == pDevice || SL_DEFAULTDEVICEID_VIBRA != deviceID)
        return SL_RESULT_PARAMETER_INVALID;
    *pDevice = NULL;
    unsigned exposedMask;
    const ClassTable *pCVibraDevice_class = objectIDtoClass(SL_OBJECTID_VIBRADEVICE);
    SLresult result = checkInterfaces(pCVibraDevice_class, numInterfaces,
        pInterfaceIds, pInterfaceRequired, &exposedMask);
    if (SL_RESULT_SUCCESS != result)
        return result;
    CVibraDevice *this = (CVibraDevice *) construct(pCVibraDevice_class, exposedMask, self);
    if (NULL == this)
        return SL_RESULT_MEMORY_FAILURE;
    this->mDeviceID = deviceID;
    *pDevice = &this->mObject.mItf;
    return SL_RESULT_SUCCESS;
}

static SLresult IEngine_CreateAudioPlayer(SLEngineItf self, SLObjectItf *pPlayer,
    SLDataSource *pAudioSrc, SLDataSink *pAudioSnk, SLuint32 numInterfaces,
    const SLInterfaceID *pInterfaceIds, const SLboolean *pInterfaceRequired)
{
    fprintf(stderr, "entering IEngine_CreateAudioPlayer()\n");

    if (NULL == pPlayer)
        return SL_RESULT_PARAMETER_INVALID;
    *pPlayer = NULL;
    unsigned exposedMask;
    const ClassTable *pCAudioPlayer_class = objectIDtoClass(SL_OBJECTID_AUDIOPLAYER);
    SLresult result = checkInterfaces(pCAudioPlayer_class, numInterfaces,
        pInterfaceIds, pInterfaceRequired, &exposedMask);
    if (SL_RESULT_SUCCESS != result)
        return result;

    // Construct our new AudioPlayer instance
    CAudioPlayer *this = (CAudioPlayer *) construct(pCAudioPlayer_class, exposedMask, self);
    if (NULL == this) {
        return SL_RESULT_MEMORY_FAILURE;
    }

    // Initialize private fields not associated with an interface
    this->mMuteMask = 0;
    this->mSoloMask = 0;
    // const, will be set later by the containing AudioPlayer or MidiPlayer
    this->mNumChannels = 0;
    this->mSampleRateMilliHz = 0;

    // Check the source and sink parameters against generic constraints,
    // and make a local copy of all parameters in case other application threads
    // change memory concurrently.

    result = checkDataSource(pAudioSrc, &this->mDataSource);
    if (SL_RESULT_SUCCESS != result)
        goto abort;

    result = checkDataSink(pAudioSnk, &this->mDataSink);
    if (SL_RESULT_SUCCESS != result)
        goto abort;

    //fprintf(stderr, "\t after checkDataSource()/Sink()\n");

    // check the audio source and sink parameters against platform support

#ifdef ANDROID
    result = sles_to_android_checkAudioPlayerSourceSink(this);
    if (SL_RESULT_SUCCESS != result)
        goto abort;
    //fprintf(stderr, "\t after sles_to_android_checkAudioPlayerSourceSink()\n");
#else
    {
    // FIXME This is b/c we init buffer queues in SndFile below, which is not always present
    SLuint32 locatorType = *(SLuint32 *) pAudioSrc->pLocator;
    if (locatorType == SL_DATALOCATOR_BUFFERQUEUE)
        this->mBufferQueue.mNumBuffers = ((SLDataLocator_BufferQueue *)
            pAudioSrc->pLocator)->numBuffers;
    }
#endif

#ifdef USE_SNDFILE
    result = SndFile_checkAudioPlayerSourceSink(this);
    if (SL_RESULT_SUCCESS != result)
        goto abort;
#endif

#ifdef USE_OUTPUTMIXEXT
    result = IOutputMixExt_checkAudioPlayerSourceSink(this);
    if (SL_RESULT_SUCCESS != result)
        goto abort;
#endif

    // Allocate memory for buffer queue
    //if (0 != this->mBufferQueue.mNumBuffers) {
        // inline allocation of circular mArray, up to a typical max
        if (BUFFER_HEADER_TYPICAL >= this->mBufferQueue.mNumBuffers) {
            this->mBufferQueue.mArray = this->mBufferQueue.mTypical;
        } else {
            // Avoid possible integer overflow during multiplication; this arbitrary maximum is big
            // enough to not interfere with real applications, but small enough to not overflow.
            if (this->mBufferQueue.mNumBuffers >= 256) {
                result = SL_RESULT_MEMORY_FAILURE;
                goto abort;
            }
            this->mBufferQueue.mArray = (BufferHeader *)
                    malloc((this->mBufferQueue.mNumBuffers + 1) * sizeof(BufferHeader));
            if (NULL == this->mBufferQueue.mArray) {
                result = SL_RESULT_MEMORY_FAILURE;
                goto abort;
            }
        }
        this->mBufferQueue.mFront = this->mBufferQueue.mArray;
        this->mBufferQueue.mRear = this->mBufferQueue.mArray;
    //}

    // used to store the data source of our audio player
    this->mDynamicSource.mDataSource = &this->mDataSource.u.mSource;

    // platform-specific initialization

#ifdef ANDROID
    sles_to_android_audioPlayerCreate(this);
#endif

    // return the new audio player object
    *pPlayer = &this->mObject.mItf;
    return SL_RESULT_SUCCESS;

abort:
    (*this->mObject.mItf->Destroy)(&this->mObject.mItf);
    return result;
}

static SLresult IEngine_CreateAudioRecorder(SLEngineItf self, SLObjectItf *pRecorder,
    SLDataSource *pAudioSrc, SLDataSink *pAudioSnk, SLuint32 numInterfaces,
    const SLInterfaceID *pInterfaceIds, const SLboolean *pInterfaceRequired)
{
    if (NULL == pRecorder)
        return SL_RESULT_PARAMETER_INVALID;
    *pRecorder = NULL;
    unsigned exposedMask;
    const ClassTable *pCAudioRecorder_class = objectIDtoClass(SL_OBJECTID_AUDIORECORDER);
    SLresult result = checkInterfaces(pCAudioRecorder_class, numInterfaces,
        pInterfaceIds, pInterfaceRequired, &exposedMask);
    if (SL_RESULT_SUCCESS != result)
        return result;

    // Construct our new AudioRecorder instance
    CAudioRecorder *this = (CAudioRecorder *) construct(pCAudioRecorder_class, exposedMask, self);
    if (NULL == this)
        return SL_RESULT_MEMORY_FAILURE;

    // Check the source and sink parameters, and make a local copy of all parameters.

    result = checkDataSource(pAudioSrc, &this->mDataSource);
    if (SL_RESULT_SUCCESS != result)
        goto abort;

    result = checkDataSink(pAudioSnk, &this->mDataSink);
    if (SL_RESULT_SUCCESS != result)
        goto abort;

    // FIXME This section is not yet fully implemented

    // return the new audio recorder object
    *pRecorder = &this->mObject.mItf;
    return SL_RESULT_SUCCESS;

abort:
    (*this->mObject.mItf->Destroy)(&this->mObject.mItf);
    return result;
}

static SLresult IEngine_CreateMidiPlayer(SLEngineItf self, SLObjectItf *pPlayer,
    SLDataSource *pMIDISrc, SLDataSource *pBankSrc, SLDataSink *pAudioOutput,
    SLDataSink *pVibra, SLDataSink *pLEDArray, SLuint32 numInterfaces,
    const SLInterfaceID *pInterfaceIds, const SLboolean *pInterfaceRequired)
{
    if (NULL == pPlayer)
        return SL_RESULT_PARAMETER_INVALID;
    *pPlayer = NULL;
    unsigned exposedMask;
    const ClassTable *pCMidiPlayer_class = objectIDtoClass(SL_OBJECTID_MIDIPLAYER);
    SLresult result = checkInterfaces(pCMidiPlayer_class, numInterfaces,
        pInterfaceIds, pInterfaceRequired, &exposedMask);
    if (SL_RESULT_SUCCESS != result)
        return result;
    if (NULL == pMIDISrc || NULL == pAudioOutput)
        return SL_RESULT_PARAMETER_INVALID;
    CMidiPlayer *this = (CMidiPlayer *) construct(pCMidiPlayer_class, exposedMask, self);
    if (NULL == this)
        return SL_RESULT_MEMORY_FAILURE;
    // return the new MIDI player object
    *pPlayer = &this->mObject.mItf;
    return SL_RESULT_SUCCESS;
}

static SLresult IEngine_CreateListener(SLEngineItf self, SLObjectItf *pListener,
    SLuint32 numInterfaces, const SLInterfaceID *pInterfaceIds, const SLboolean *pInterfaceRequired)
{
    if (NULL == pListener)
        return SL_RESULT_PARAMETER_INVALID;
    *pListener = NULL;
    unsigned exposedMask;
    const ClassTable *pCListener_class = objectIDtoClass(SL_OBJECTID_LISTENER);
    SLresult result = checkInterfaces(pCListener_class, numInterfaces,
        pInterfaceIds, pInterfaceRequired, &exposedMask);
    if (SL_RESULT_SUCCESS != result)
        return result;
    CListener *this = (CListener *) construct(pCListener_class, exposedMask, self);
    if (NULL == this)
        return SL_RESULT_MEMORY_FAILURE;
    // return the new listener object
    *pListener = &this->mObject.mItf;
    return SL_RESULT_SUCCESS;
}

static SLresult IEngine_Create3DGroup(SLEngineItf self, SLObjectItf *pGroup, SLuint32 numInterfaces,
    const SLInterfaceID *pInterfaceIds, const SLboolean *pInterfaceRequired)
{
    if (NULL == pGroup)
        return SL_RESULT_PARAMETER_INVALID;
    *pGroup = NULL;
    unsigned exposedMask;
    const ClassTable *pC3DGroup_class = objectIDtoClass(SL_OBJECTID_3DGROUP);
    SLresult result = checkInterfaces(pC3DGroup_class, numInterfaces,
        pInterfaceIds, pInterfaceRequired, &exposedMask);
    if (SL_RESULT_SUCCESS != result)
        return result;
    C3DGroup *this = (C3DGroup *) construct(pC3DGroup_class, exposedMask, self);
    if (NULL == this)
        return SL_RESULT_MEMORY_FAILURE;
    this->mMemberMask = 0;
    // return the new 3DGroup object
    *pGroup = &this->mObject.mItf;
    return SL_RESULT_SUCCESS;
}

static SLresult IEngine_CreateOutputMix(SLEngineItf self, SLObjectItf *pMix, SLuint32 numInterfaces,
    const SLInterfaceID *pInterfaceIds, const SLboolean *pInterfaceRequired)
{
    if (NULL == pMix)
        return SL_RESULT_PARAMETER_INVALID;
    *pMix = NULL;
    unsigned exposedMask;
    const ClassTable *pCOutputMix_class = objectIDtoClass(SL_OBJECTID_OUTPUTMIX);
    SLresult result = checkInterfaces(pCOutputMix_class, numInterfaces,
        pInterfaceIds, pInterfaceRequired, &exposedMask);
    if (SL_RESULT_SUCCESS != result)
        return result;
    COutputMix *this = (COutputMix *) construct(pCOutputMix_class, exposedMask, self);
    if (NULL == this)
        return SL_RESULT_MEMORY_FAILURE;
    *pMix = &this->mObject.mItf;
    return SL_RESULT_SUCCESS;
}

static SLresult IEngine_CreateMetadataExtractor(SLEngineItf self, SLObjectItf *pMetadataExtractor,
    SLDataSource *pDataSource, SLuint32 numInterfaces, const SLInterfaceID *pInterfaceIds,
    const SLboolean *pInterfaceRequired)
{
    if (NULL == pMetadataExtractor)
        return SL_RESULT_PARAMETER_INVALID;
    *pMetadataExtractor = NULL;
    unsigned exposedMask;
    const ClassTable *pCMetadataExtractor_class = objectIDtoClass(SL_OBJECTID_METADATAEXTRACTOR);
    SLresult result = checkInterfaces(pCMetadataExtractor_class, numInterfaces,
        pInterfaceIds, pInterfaceRequired, &exposedMask);
    if (SL_RESULT_SUCCESS != result)
        return result;
    CMetadataExtractor *this = (CMetadataExtractor *)
        construct(pCMetadataExtractor_class, exposedMask, self);
    if (NULL == this)
        return SL_RESULT_MEMORY_FAILURE;
    *pMetadataExtractor = &this->mObject.mItf;
    return SL_RESULT_SUCCESS;
}

static SLresult IEngine_CreateExtensionObject(SLEngineItf self, SLObjectItf *pObject,
    void *pParameters, SLuint32 objectID, SLuint32 numInterfaces,
    const SLInterfaceID *pInterfaceIds, const SLboolean *pInterfaceRequired)
{
    if (NULL == pObject)
        return SL_RESULT_PARAMETER_INVALID;
    *pObject = NULL;
    return SL_RESULT_FEATURE_UNSUPPORTED;
}

static SLresult IEngine_QueryNumSupportedInterfaces(SLEngineItf self,
    SLuint32 objectID, SLuint32 *pNumSupportedInterfaces)
{
    if (NULL == pNumSupportedInterfaces)
        return SL_RESULT_PARAMETER_INVALID;
    const ClassTable *class__ = objectIDtoClass(objectID);
    if (NULL == class__)
        return SL_RESULT_FEATURE_UNSUPPORTED;
    // FIXME Cache this value
    SLuint32 count = 0;
    SLuint32 i;
    for (i = 0; i < class__->mInterfaceCount; ++i)
        if (class__->mInterfaces[i].mInterface != INTERFACE_UNAVAILABLE)
            ++count;
    *pNumSupportedInterfaces = count;
    return SL_RESULT_SUCCESS;
}

static SLresult IEngine_QuerySupportedInterfaces(SLEngineItf self,
    SLuint32 objectID, SLuint32 index, SLInterfaceID *pInterfaceId)
{
    if (NULL == pInterfaceId)
        return SL_RESULT_PARAMETER_INVALID;
    const ClassTable *class__ = objectIDtoClass(objectID);
    if (NULL == class__)
        return SL_RESULT_FEATURE_UNSUPPORTED;
    // FIXME O(n)
    SLuint32 i;
    for (i = 0; i < class__->mInterfaceCount; ++i) {
        // FIXME check whether published also (might be internal implicit)
        if (class__->mInterfaces[i].mInterface == INTERFACE_UNAVAILABLE)
            continue;
        if (index == 0) {
            *pInterfaceId = &SL_IID_array[class__->mInterfaces[i].mMPH];
            return SL_RESULT_SUCCESS;
        }
        --index;
    }
    return SL_RESULT_PARAMETER_INVALID;
};

static SLresult IEngine_QueryNumSupportedExtensions(SLEngineItf self, SLuint32 *pNumExtensions)
{
    if (NULL == pNumExtensions)
        return SL_RESULT_PARAMETER_INVALID;
    *pNumExtensions = 0;
    return SL_RESULT_SUCCESS;
}

static SLresult IEngine_QuerySupportedExtension(SLEngineItf self,
    SLuint32 index, SLchar *pExtensionName, SLint16 *pNameLength)
{
    // any index >= 0 will be >= number of supported extensions
    return SL_RESULT_PARAMETER_INVALID;
}

static SLresult IEngine_IsExtensionSupported(SLEngineItf self,
    const SLchar *pExtensionName, SLboolean *pSupported)
{
    if (NULL == pExtensionName || NULL == pSupported)
        return SL_RESULT_PARAMETER_INVALID;
    *pSupported = SL_BOOLEAN_FALSE;
    return SL_RESULT_SUCCESS;
}

static const struct SLEngineItf_ IEngine_Itf = {
    IEngine_CreateLEDDevice,
    IEngine_CreateVibraDevice,
    IEngine_CreateAudioPlayer,
    IEngine_CreateAudioRecorder,
    IEngine_CreateMidiPlayer,
    IEngine_CreateListener,
    IEngine_Create3DGroup,
    IEngine_CreateOutputMix,
    IEngine_CreateMetadataExtractor,
    IEngine_CreateExtensionObject,
    IEngine_QueryNumSupportedInterfaces,
    IEngine_QuerySupportedInterfaces,
    IEngine_QueryNumSupportedExtensions,
    IEngine_QuerySupportedExtension,
    IEngine_IsExtensionSupported
};

void IEngine_init(void *self)
{
    IEngine *this = (IEngine *) self;
    this->mItf = &IEngine_Itf;
    // mLossOfControlGlobal is initialized in CreateEngine
#ifdef USE_SDL
    this->mOutputMix = NULL;
#endif
    this->mInstanceCount = 1; // ourself
    this->mInstanceMask = 0;
    this->mChangedMask = 0;
    unsigned i;
    for (i = 0; i < MAX_INSTANCE; ++i)
        this->mInstances[i] = NULL;
    this->mShutdown = SL_BOOLEAN_FALSE;
    int ok;
    ok = pthread_cond_init(&this->mShutdownCond, (const pthread_condattr_t *) NULL);
    assert(0 == ok);
}
