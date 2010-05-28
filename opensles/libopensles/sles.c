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

// Map an IObject to it's "object ID" (which is really a class ID)

SLuint32 IObjectToObjectID(IObject *this)
{
    assert(NULL != this);
    return this->mClass->mObjectID;
}

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

#ifdef USE_OUTPUTMIXEXT
extern void
    IOutputMixExt_init(void *);
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
        IOutputMixExt_init, NULL
#else
        NULL, NULL
#endif
        }
};

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

// The sync thread runs periodically to synchronize audio state between
// the application and platform-specific device driver; for best results
// it should run about every graphics frame (e.g. 20 Hz to 50 Hz).

static void *sync_body(void *arg)
{
    CEngine *this = (CEngine *) arg;
    for (;;) {
        usleep(20000*50);
        unsigned i;
        for (i = 0; i < INSTANCE_MAX; ++i) {
            IObject *instance = (IObject *) this->mEngine.mInstances[i];
            if (NULL == instance)
                continue;
            switch (IObjectToObjectID(instance)) {
            case SL_OBJECTID_AUDIOPLAYER:
                write(1, ".", 1);
                break;
            default:
                break;
            }
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
    const ClassTable *pCEngine_class = objectIDtoClass(SL_OBJECTID_ENGINE);
    SLresult result = checkInterfaces(pCEngine_class, numInterfaces,
        pInterfaceIds, pInterfaceRequired, &exposedMask);
    if (SL_RESULT_SUCCESS != result)
        return result;
    CEngine *this = (CEngine *) construct(pCEngine_class, exposedMask, NULL);
    if (NULL == this)
        return SL_RESULT_MEMORY_FAILURE;
    this->mObject.mLossOfControlMask = lossOfControlGlobal ? ~0 : 0;
    this->mEngine.mLossOfControlGlobal = lossOfControlGlobal;
    this->mEngineCapabilities.mThreadSafe = threadSafe;
    int ok;
    ok = pthread_create(&this->mSyncThread, (const pthread_attr_t *) NULL, sync_body, this);
    assert(ok == 0);
    *pEngine = &this->mObject.mItf;
    return SL_RESULT_SUCCESS;
}

SLresult SLAPIENTRY slQueryNumSupportedEngineInterfaces(
    SLuint32 *pNumSupportedInterfaces)
{
    if (NULL == pNumSupportedInterfaces)
        return SL_RESULT_PARAMETER_INVALID;
    const ClassTable *pCEngine_class = objectIDtoClass(SL_OBJECTID_ENGINE);
    *pNumSupportedInterfaces = pCEngine_class->mInterfaceCount;
    return SL_RESULT_SUCCESS;
}

SLresult SLAPIENTRY slQuerySupportedEngineInterfaces(SLuint32 index,
    SLInterfaceID *pInterfaceId)
{
    if (NULL == pInterfaceId)
        return SL_RESULT_PARAMETER_INVALID;
    const ClassTable *pCEngine_class = objectIDtoClass(SL_OBJECTID_ENGINE);
    if (pCEngine_class->mInterfaceCount <= index)
        return SL_RESULT_PARAMETER_INVALID;
    *pInterfaceId = &SL_IID_array[pCEngine_class->mInterfaces[index].mMPH];
    return SL_RESULT_SUCCESS;
}

/* End */
