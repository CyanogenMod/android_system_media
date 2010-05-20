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

static SLresult checkInterfaces(const ClassTable *class__,
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

extern const struct SLBufferQueueItf_ BufferQueue_BufferQueueItf;

static void BufferQueue_init(void *self)
{
    IBufferQueue *this = (IBufferQueue *) self;
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

extern const struct SLEngineItf_ Engine_EngineItf;

static void Engine_init(void *self)
{
    IEngine *this = (IEngine *) self;
    this->mItf = &Engine_EngineItf;
    // mLossOfControlGlobal is initialized in CreateEngine
#ifndef NDEBUG
    this->mInstanceCount = 0;
    unsigned i;
    for (i = 0; i < INSTANCE_MAX; ++i)
        this->mInstances[i] = NULL;
#endif
}

extern const struct SLEngineCapabilitiesItf_
    EngineCapabilities_EngineCapabilitiesItf;

static void EngineCapabilities_init(void *self)
{
    IEngineCapabilities *this =
        (IEngineCapabilities *) self;
    this->mItf = &EngineCapabilities_EngineCapabilitiesItf;
}

extern const struct SLMIDIMessageItf_ MIDIMessage_MIDIMessageItf;

static void MIDIMessage_init(void *self)
{
    IMIDIMessage *this = (IMIDIMessage *) self;
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
    IMIDIMuteSolo *this = (IMIDIMuteSolo *) self;
    this->mItf = &MIDIMuteSolo_MIDIMuteSoloItf;
}

extern const struct SLMIDITempoItf_ MIDITempo_MIDITempoItf;

static void MIDITempo_init(void *self)
{
    IMIDITempo *this = (IMIDITempo *) self;
    this->mItf = &MIDITempo_MIDITempoItf;
}

extern const struct SLMIDITimeItf_ MIDITime_MIDITimeItf;

static void MIDITime_init(void *self)
{
    IMIDITime *this = (IMIDITime *) self;
    this->mItf = &MIDITime_MIDITimeItf;
}

extern const struct SLOutputMixItf_ OutputMix_OutputMixItf;

static void OutputMix_init(void *self)
{
    IOutputMix *this = (IOutputMix *) self;
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
    IOutputMixExt *this =
        (IOutputMixExt *) self;
    this->mItf = &OutputMixExt_OutputMixExtItf;
}
#endif // USE_OUTPUTMIXEXT

extern const struct SLPlayItf_ Play_PlayItf;

static void Play_init(void *self)
{
    IPlay *this = (IPlay *) self;
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

extern const struct SLSeekItf_ Seek_SeekItf;

static void Seek_init(void *self)
{
    ISeek *this = (ISeek *) self;
    this->mItf = &Seek_SeekItf;
    this->mPos = (SLmillisecond) -1;
    this->mStartPos = (SLmillisecond) -1;
    this->mEndPos = (SLmillisecond) -1;
#ifndef NDEBUG
    this->mLoopEnabled = SL_BOOLEAN_FALSE;
#endif
}

extern const struct SLVolumeItf_ Volume_VolumeItf;

static void Volume_init(void *self)
{
    IVolume *this = (IVolume *) self;
    this->mItf = &Volume_VolumeItf;
#ifndef NDEBUG
    this->mLevel = 0;
    this->mMute = SL_BOOLEAN_FALSE;
    this->mEnableStereoPosition = SL_BOOLEAN_FALSE;
    this->mStereoPosition = 0;
#endif
}

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
    IDeviceVolume_init(void *),
    IDynamicInterfaceManagement_init(void *),
    IDynamicSource_init(void *),
    IEffectSend_init(void *),
    IEnvironmentalReverb_init(void *),
    IEqualizer_init(void *),
    ILEDArray_init(void *),
    IMetadataExtraction_init(void *),
    IMetadataTraversal_init(void *),
    IMuteSolo_init(void *),
    IObject_init(void *),
    IPitch_init(void *),
    IPlaybackRate_init(void *),
    IPrefetchStatus_init(void *),
    IPresetReverb_init(void *),
    IRatePitch_init(void *),
    IRecord_init(void *),
    IThreadSync_init(void *),
    IVibra_init(void *),
    IVirtualizer_init(void *),
    IVisualization_init(void *);

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
    { /* MPH_BUFFERQUEUE, */ BufferQueue_init, NULL },
    { /* MPH_DEVICEVOLUME, */ IDeviceVolume_init, NULL },
    { /* MPH_DYNAMICINTERFACEMANAGEMENT, */ IDynamicInterfaceManagement_init,
        NULL },
    { /* MPH_DYNAMICSOURCE, */ IDynamicSource_init, NULL },
    { /* MPH_EFFECTSEND, */ IEffectSend_init, NULL },
    { /* MPH_ENGINE, */ Engine_init, NULL },
    { /* MPH_ENGINECAPABILITIES, */ EngineCapabilities_init, NULL },
    { /* MPH_ENVIRONMENTALREVERB, */ IEnvironmentalReverb_init, NULL },
    { /* MPH_EQUALIZER, */ IEqualizer_init, NULL },
    { /* MPH_LED, */ ILEDArray_init, NULL },
    { /* MPH_METADATAEXTRACTION, */ IMetadataExtraction_init, NULL },
    { /* MPH_METADATATRAVERSAL, */ IMetadataTraversal_init, NULL },
    { /* MPH_MIDIMESSAGE, */ MIDIMessage_init, NULL },
    { /* MPH_MIDITIME, */ MIDITime_init, NULL },
    { /* MPH_MIDITEMPO, */ MIDITempo_init, NULL },
    { /* MPH_MIDIMUTESOLO, */ MIDIMuteSolo_init, NULL },
    { /* MPH_MUTESOLO, */ IMuteSolo_init, NULL },
    { /* MPH_NULL, */ NULL, NULL },
    { /* MPH_OBJECT, */ IObject_init, NULL },
    { /* MPH_OUTPUTMIX, */ OutputMix_init, NULL },
    { /* MPH_PITCH, */ IPitch_init, NULL },
    { /* MPH_PLAY, */ Play_init, NULL },
    { /* MPH_PLAYBACKRATE, */ IPlaybackRate_init, NULL },
    { /* MPH_PREFETCHSTATUS, */ IPrefetchStatus_init, NULL },
    { /* MPH_PRESETREVERB, */ IPresetReverb_init, NULL },
    { /* MPH_RATEPITCH, */ IRatePitch_init, NULL },
    { /* MPH_RECORD, */ IRecord_init, NULL },
    { /* MPH_SEEK, */ Seek_init, NULL },
    { /* MPH_THREADSYNC, */ IThreadSync_init, NULL },
    { /* MPH_VIBRA, */ IVibra_init, NULL },
    { /* MPH_VIRTUALIZER, */ IVirtualizer_init, NULL },
    { /* MPH_VISUALIZATION, */ IVisualization_init, NULL },
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


#ifdef USE_ANDROID
static void *thread_body(void *arg)
{
    CAudioPlayer *this = (CAudioPlayer *) arg;
    android::AudioTrack *at = this->mAudioTrack;
#if 1
    at->start();
#endif
    IBufferQueue *bufferQueue = &this->mBufferQueue;
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
    CAudioPlayer *this = (CAudioPlayer *) self;
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

static const ClassTable *objectIDtoClass(SLuint32 objectID)
{
    SLuint32 objectID0 = classes[0]->mObjectID;
    if (objectID0 <= objectID &&
        classes[sizeof(classes)/sizeof(classes[0])-1]->mObjectID >= objectID)
        return classes[objectID - objectID0];
    return NULL;
}

// Construct a new instance of the specified class, exposing selected interfaces

static IObject *construct(const ClassTable *class__,
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

/* Interface implementations */

// FIXME Sort by interface

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
    IPlay *this = (IPlay *) self;
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
    IPlay *this = (IPlay *) self;
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
    IPlay *this = (IPlay *) self;
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
    IPlay *this = (IPlay *) self;
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
    IPlay *this = (IPlay *) self;
    // FIXME This could be a poke lock, if we had atomic double-word load/store
    interface_lock_exclusive(this);
    this->mCallback = callback;
    this->mContext = pContext;
    interface_unlock_exclusive(this);
    return SL_RESULT_SUCCESS;
}

static SLresult Play_SetCallbackEventsMask(SLPlayItf self, SLuint32 eventFlags)
{
    IPlay *this = (IPlay *) self;
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
    IPlay *this = (IPlay *) self;
    interface_lock_peek(this);
    SLuint32 eventFlags = this->mEventFlags;
    interface_unlock_peek(this);
    *pEventFlags = eventFlags;
    return SL_RESULT_SUCCESS;
}

static SLresult Play_SetMarkerPosition(SLPlayItf self, SLmillisecond mSec)
{
    IPlay *this = (IPlay *) self;
    interface_lock_poke(this);
    this->mMarkerPosition = mSec;
    interface_unlock_poke(this);
    return SL_RESULT_SUCCESS;
}

static SLresult Play_ClearMarkerPosition(SLPlayItf self)
{
    IPlay *this = (IPlay *) self;
    interface_lock_poke(this);
    this->mMarkerPosition = 0;
    interface_unlock_poke(this);
    return SL_RESULT_SUCCESS;
}

static SLresult Play_GetMarkerPosition(SLPlayItf self, SLmillisecond *pMsec)
{
    if (NULL == pMsec)
        return SL_RESULT_PARAMETER_INVALID;
    IPlay *this = (IPlay *) self;
    interface_lock_peek(this);
    SLmillisecond markerPosition = this->mMarkerPosition;
    interface_unlock_peek(this);
    *pMsec = markerPosition;
    return SL_RESULT_SUCCESS;
}

static SLresult Play_SetPositionUpdatePeriod(SLPlayItf self, SLmillisecond mSec)
{
    IPlay *this = (IPlay *) self;
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
    IPlay *this = (IPlay *) self;
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
    IBufferQueue *this = (IBufferQueue *) self;
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
    IBufferQueue *this = (IBufferQueue *) self;
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
    IBufferQueue *this = (IBufferQueue *) self;
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
    IBufferQueue *this = (IBufferQueue *) self;
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
    IVolume *this = (IVolume *) self;
    interface_lock_poke(this);
    this->mLevel = level;
    interface_unlock_poke(this);
    return SL_RESULT_SUCCESS;
}

static SLresult Volume_GetVolumeLevel(SLVolumeItf self, SLmillibel *pLevel)
{
    if (NULL == pLevel)
        return SL_RESULT_PARAMETER_INVALID;
    IVolume *this = (IVolume *) self;
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
    IVolume *this = (IVolume *) self;
    interface_lock_poke(this);
    this->mMute = mute;
    interface_unlock_poke(this);
    return SL_RESULT_SUCCESS;
}

static SLresult Volume_GetMute(SLVolumeItf self, SLboolean *pMute)
{
    if (NULL == pMute)
        return SL_RESULT_PARAMETER_INVALID;
    IVolume *this = (IVolume *) self;
    interface_lock_peek(this);
    SLboolean mute = this->mMute;
    interface_unlock_peek(this);
    *pMute = mute;
    return SL_RESULT_SUCCESS;
}

static SLresult Volume_EnableStereoPosition(SLVolumeItf self, SLboolean enable)
{
    IVolume *this = (IVolume *) self;
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
    IVolume *this = (IVolume *) self;
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
    IVolume *this = (IVolume *) self;
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
    IVolume *this = (IVolume *) self;
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
    SLresult result = checkInterfaces(&CLEDDevice_class, numInterfaces,
        pInterfaceIds, pInterfaceRequired, &exposedMask);
    if (SL_RESULT_SUCCESS != result)
        return result;
    CLEDDevice *this = (CLEDDevice *)
        construct(&CLEDDevice_class, exposedMask, self);
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
    SLresult result = checkInterfaces(&CVibraDevice_class, numInterfaces,
        pInterfaceIds, pInterfaceRequired, &exposedMask);
    if (SL_RESULT_SUCCESS != result)
        return result;
    CVibraDevice *this = (CVibraDevice *)
        construct(&CVibraDevice_class, exposedMask, self);
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
    SLresult result = checkInterfaces(&CAudioPlayer_class, numInterfaces,
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
    struct Track *track = NULL;
    switch (*(SLuint32 *)pAudioSnk->pLocator) {
    case SL_DATALOCATOR_OUTPUTMIX:
        {
        // pAudioSnk->pFormat is ignored
        SLDataLocator_OutputMix *dl_outmix =
            (SLDataLocator_OutputMix *) pAudioSnk->pLocator;
        SLObjectItf outputMix = dl_outmix->outputMix;
        // FIXME make this an operation on Object: GetClass
        if ((NULL == outputMix) || (&COutputMix_class !=
            ((IObject *) outputMix)->mClass))
            return SL_RESULT_PARAMETER_INVALID;
        IOutputMix *om =
            &((COutputMix *) outputMix)->mOutputMix;
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
    CAudioPlayer *this = (CAudioPlayer *)
        construct(&CAudioPlayer_class, exposedMask, self);
    if (NULL == this)
        return SL_RESULT_MEMORY_FAILURE;

    // DataSource specific initializations
    switch(*(SLuint32 *)pAudioSrc->pLocator) {
    case SL_DATALOCATOR_URI:
#ifndef USE_OUTPUTMIXEXT // e.g. USE_ANDROID
        break;
#else
        // fall through
#endif
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
    SLresult result = checkInterfaces(&CAudioRecorder_class, numInterfaces,
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
    SLresult result = checkInterfaces(&CMidiPlayer_class, numInterfaces,
        pInterfaceIds, pInterfaceRequired, &exposedMask);
    if (SL_RESULT_SUCCESS != result)
        return result;
    if (NULL == pMIDISrc || NULL == pAudioOutput)
        return SL_RESULT_PARAMETER_INVALID;
    CMidiPlayer *this = (CMidiPlayer *)
        construct(&CMidiPlayer_class, exposedMask, self);
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
    SLresult result = checkInterfaces(&CListener_class, numInterfaces,
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
    SLresult result = checkInterfaces(&C3DGroup_class, numInterfaces,
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
    SLresult result = checkInterfaces(&COutputMix_class, numInterfaces,
        pInterfaceIds, pInterfaceRequired, &exposedMask);
    if (SL_RESULT_SUCCESS != result)
        return result;
    COutputMix *this = (COutputMix *)
        construct(&COutputMix_class, exposedMask, self);
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
    SLresult result = checkInterfaces(&CMetadataExtractor_class, numInterfaces,
        pInterfaceIds, pInterfaceRequired, &exposedMask);
    if (SL_RESULT_SUCCESS != result)
        return result;
    CMetadataExtractor *this = (CMetadataExtractor *)
        construct(&CMetadataExtractor_class, exposedMask, self);
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
    const ClassTable *class__ = objectIDtoClass(objectID);
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
    const ClassTable *class__ = objectIDtoClass(objectID);
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
    IOutputMix *this = (IOutputMix *) self;
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
    ISeek *this = (ISeek *) self;
    interface_lock_poke(this);
    this->mPos = pos;
    interface_unlock_poke(this);
    return SL_RESULT_SUCCESS;
}

static SLresult Seek_SetLoop(SLSeekItf self, SLboolean loopEnable,
    SLmillisecond startPos, SLmillisecond endPos)
{
    ISeek *this = (ISeek *) self;
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
    ISeek *this = (ISeek *) self;
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
    const struct LED_id_descriptor *id_descriptor = LED_id_descriptors;
    while (NULL != id_descriptor->descriptor)
        ++id_descriptor;
    // FIXME should cache this value
    SLuint32 maxIndex = id_descriptor - LED_id_descriptors;
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
    const struct Vibra_id_descriptor *id_descriptor = Vibra_id_descriptors;
    while (NULL != id_descriptor->descriptor)
        ++id_descriptor;
    // FIXME should cache this value
    SLuint32 maxIndex = id_descriptor - Vibra_id_descriptors;
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
    IEngineCapabilities *this =
        (IEngineCapabilities *) self;
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

// MIDIMessage implementation

static SLresult MIDIMessage_SendMessage(SLMIDIMessageItf self, const SLuint8 *data, SLuint32 length)
{
    if (NULL == data)
        return SL_RESULT_PARAMETER_INVALID;
    //IMIDIMessage *this = (IMIDIMessage *) self;
    return SL_RESULT_SUCCESS;
}

static SLresult MIDIMessage_RegisterMetaEventCallback(SLMIDIMessageItf self,
    slMetaEventCallback callback, void *pContext)
{
    IMIDIMessage *this = (IMIDIMessage *) self;
    interface_lock_exclusive(this);
    this->mMetaEventCallback = callback;
    this->mMetaEventContext = pContext;
    interface_unlock_exclusive(this);
    return SL_RESULT_SUCCESS;
}

static SLresult MIDIMessage_RegisterMIDIMessageCallback(SLMIDIMessageItf self,
    slMIDIMessageCallback callback, void *pContext)
{
    IMIDIMessage *this = (IMIDIMessage *) self;
    interface_lock_exclusive(this);
    this->mMessageCallback = callback;
    this->mMessageContext = pContext;
    interface_unlock_exclusive(this);
    return SL_RESULT_SUCCESS;
}

static SLresult MIDIMessage_AddMIDIMessageCallbackFilter(SLMIDIMessageItf self,
    SLuint32 messageType)
{
    //IMIDIMessage *this = (IMIDIMessage *) self;
    return SL_RESULT_SUCCESS;
}

static SLresult MIDIMessage_ClearMIDIMessageCallbackFilter(SLMIDIMessageItf self)
{
    //IMIDIMessage *this = (IMIDIMessage *) self;
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
    IMIDIMuteSolo *this = (IMIDIMuteSolo *) self;
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
    IMIDIMuteSolo *this = (IMIDIMuteSolo *) self;
    interface_lock_peek(this);
    SLuint16 mask = this->mChannelMuteMask;
    interface_unlock_peek(this);
    *pMute = (mask >> channel) & 1;
    return SL_RESULT_SUCCESS;
}

static SLresult MIDIMuteSolo_SetChannelSolo(SLMIDIMuteSoloItf self, SLuint8 channel, SLboolean solo)
{
    IMIDIMuteSolo *this = (IMIDIMuteSolo *) self;
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
    IMIDIMuteSolo *this = (IMIDIMuteSolo *) self;
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
    IMIDIMuteSolo *this = (IMIDIMuteSolo *) self;
    // FIXME is this const? if so, remove the lock
    interface_lock_peek(this);
    SLuint16 trackCount = this->mTrackCount;
    interface_unlock_peek(this);
    *pCount = trackCount;
    return SL_RESULT_SUCCESS;
}

static SLresult MIDIMuteSolo_SetTrackMute(SLMIDIMuteSoloItf self, SLuint16 track, SLboolean mute)
{
    IMIDIMuteSolo *this = (IMIDIMuteSolo *) self;
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
    IMIDIMuteSolo *this = (IMIDIMuteSolo *) self;
    interface_lock_peek(this);
    SLuint32 mask = this->mTrackMuteMask;
    interface_unlock_peek(this);
    *pMute = (mask >> track) & 1;
    return SL_RESULT_SUCCESS;
}

static SLresult MIDIMuteSolo_SetTrackSolo(SLMIDIMuteSoloItf self, SLuint16 track, SLboolean solo)
{
    IMIDIMuteSolo *this = (IMIDIMuteSolo *) self;
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
    IMIDIMuteSolo *this = (IMIDIMuteSolo *) self;
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
    IMIDITempo *this = (IMIDITempo *) self;
    interface_lock_poke(this);
    this->mTicksPerQuarterNote = tpqn;
    interface_unlock_poke(this);
    return SL_RESULT_SUCCESS;
}

static SLresult MIDITempo_GetTicksPerQuarterNote(SLMIDITempoItf self, SLuint32 *pTpqn)
{
    if (NULL == pTpqn)
        return SL_RESULT_PARAMETER_INVALID;
    IMIDITempo *this = (IMIDITempo *) self;
    interface_lock_peek(this);
    SLuint32 ticksPerQuarterNote = this->mTicksPerQuarterNote;
    interface_unlock_peek(this);
    *pTpqn = ticksPerQuarterNote;
    return SL_RESULT_SUCCESS;
}

static SLresult MIDITempo_SetMicrosecondsPerQuarterNote(SLMIDITempoItf self, SLmicrosecond uspqn)
{
    IMIDITempo *this = (IMIDITempo *) self;
    interface_lock_poke(this);
    this->mMicrosecondsPerQuarterNote = uspqn;
    interface_unlock_poke(this);
    return SL_RESULT_SUCCESS;
}

static SLresult MIDITempo_GetMicrosecondsPerQuarterNote(SLMIDITempoItf self, SLmicrosecond *uspqn)
{
    if (NULL == uspqn)
        return SL_RESULT_PARAMETER_INVALID;
    IMIDITempo *this = (IMIDITempo *) self;
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
    IMIDITime *this = (IMIDITime *) self;
    SLuint32 duration = this->mDuration;
    *pDuration = duration;
    return SL_RESULT_SUCCESS;
}

static SLresult MIDITime_SetPosition(SLMIDITimeItf self, SLuint32 position)
{
    IMIDITime *this = (IMIDITime *) self;
    interface_lock_poke(this);
    this->mPosition = position;
    interface_unlock_poke(this);
    return SL_RESULT_SUCCESS;
}

static SLresult MIDITime_GetPosition(SLMIDITimeItf self, SLuint32 *pPosition)
{
    if (NULL == pPosition)
        return SL_RESULT_PARAMETER_INVALID;
    IMIDITime *this = (IMIDITime *) self;
    interface_lock_peek(this);
    SLuint32 position = this->mPosition;
    interface_unlock_peek(this);
    *pPosition = position;
    return SL_RESULT_SUCCESS;
}

static SLresult MIDITime_SetLoopPoints(SLMIDITimeItf self, SLuint32 startTick, SLuint32 numTicks)
{
    IMIDITime *this = (IMIDITime *) self;
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
    IMIDITime *this = (IMIDITime *) self;
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
