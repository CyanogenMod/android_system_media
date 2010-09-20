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
    SL_ENTER_INTERFACE

#ifdef USE_OPTIONAL
    if ((NULL == pDevice) || (SL_DEFAULTDEVICEID_LED != deviceID)) {
        result = SL_RESULT_PARAMETER_INVALID;
    } else {
        *pDevice = NULL;
        unsigned exposedMask;
        const ClassTable *pCLEDDevice_class = objectIDtoClass(SL_OBJECTID_LEDDEVICE);
        result = checkInterfaces(pCLEDDevice_class, numInterfaces, pInterfaceIds,
            pInterfaceRequired, &exposedMask);
        if (SL_RESULT_SUCCESS == result) {
            CLEDDevice *this = (CLEDDevice *) construct(pCLEDDevice_class, exposedMask, self);
            if (NULL == this) {
                result = SL_RESULT_MEMORY_FAILURE;
            } else {
                this->mDeviceID = deviceID;
                IObject_Publish(&this->mObject);
                // return the new LED object
                *pDevice = &this->mObject.mItf;
            }
        }
    }
#else
    result = SL_RESULT_FEATURE_UNSUPPORTED;
#endif

    SL_LEAVE_INTERFACE
}


static SLresult IEngine_CreateVibraDevice(SLEngineItf self, SLObjectItf *pDevice, SLuint32 deviceID,
    SLuint32 numInterfaces, const SLInterfaceID *pInterfaceIds, const SLboolean *pInterfaceRequired)
{
    SL_ENTER_INTERFACE

#ifdef USE_OPTIONAL
    if ((NULL == pDevice) || (SL_DEFAULTDEVICEID_VIBRA != deviceID)) {
        result = SL_RESULT_PARAMETER_INVALID;
    } else {
        *pDevice = NULL;
        unsigned exposedMask;
        const ClassTable *pCVibraDevice_class = objectIDtoClass(SL_OBJECTID_VIBRADEVICE);
        result = checkInterfaces(pCVibraDevice_class, numInterfaces,
            pInterfaceIds, pInterfaceRequired, &exposedMask);
        if (SL_RESULT_SUCCESS == result) {
            CVibraDevice *this = (CVibraDevice *) construct(pCVibraDevice_class, exposedMask, self);
            if (NULL == this) {
                result = SL_RESULT_MEMORY_FAILURE;
            } else {
                this->mDeviceID = deviceID;
                IObject_Publish(&this->mObject);
                // return the new vibra object
                *pDevice = &this->mObject.mItf;
            }
        }
    }
#else
    result = SL_RESULT_FEATURE_UNSUPPORTED;
#endif

    SL_LEAVE_INTERFACE
}


static SLresult IEngine_CreateAudioPlayer(SLEngineItf self, SLObjectItf *pPlayer,
    SLDataSource *pAudioSrc, SLDataSink *pAudioSnk, SLuint32 numInterfaces,
    const SLInterfaceID *pInterfaceIds, const SLboolean *pInterfaceRequired)
{
    SL_ENTER_INTERFACE

    if (NULL == pPlayer) {
       result = SL_RESULT_PARAMETER_INVALID;
    } else {
        *pPlayer = NULL;
        unsigned exposedMask;
        const ClassTable *pCAudioPlayer_class = objectIDtoClass(SL_OBJECTID_AUDIOPLAYER);
        result = checkInterfaces(pCAudioPlayer_class, numInterfaces,
            pInterfaceIds, pInterfaceRequired, &exposedMask);
        if (SL_RESULT_SUCCESS == result) {

            // Construct our new AudioPlayer instance
            CAudioPlayer *this = (CAudioPlayer *) construct(pCAudioPlayer_class, exposedMask, self);
            if (NULL == this) {
                result = SL_RESULT_MEMORY_FAILURE;
            } else {

                do {

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
                    if (SL_RESULT_SUCCESS != result) {
                        break;
                    }

                    result = checkSourceFormatVsInterfacesCompatibility(&this->mDataSource,
                            numInterfaces, pInterfaceIds, pInterfaceRequired);
                    if (SL_RESULT_SUCCESS != result) {
                        break;
                    }

                    result = checkDataSink(pAudioSnk, &this->mDataSink, SL_OBJECTID_AUDIOPLAYER);
                    if (SL_RESULT_SUCCESS != result) {
                        break;
                    }

                    // copy the buffer queue count from source locator to the buffer queue interface
                    // we have already range-checked the value down to a smaller width

                    this->mBufferQueue.mNumBuffers = (SL_DATALOCATOR_BUFFERQUEUE == *(SLuint32 *)
                        pAudioSrc->pLocator) ? (SLuint16) ((SLDataLocator_BufferQueue *)
                        pAudioSrc->pLocator)->numBuffers : 0;

                    // check the audio source and sink parameters against platform support
#ifdef ANDROID
                    result = android_audioPlayer_checkSourceSink(this);
                    if (SL_RESULT_SUCCESS != result) {
                        break;
                    }
#endif

#ifdef USE_SNDFILE
                    result = SndFile_checkAudioPlayerSourceSink(this);
                    if (SL_RESULT_SUCCESS != result) {
                        break;
                    }
#endif

#ifdef USE_OUTPUTMIXEXT
                    result = IOutputMixExt_checkAudioPlayerSourceSink(this);
                    if (SL_RESULT_SUCCESS != result) {
                        break;
                    }
#endif

                    // FIXME move to dedicated function
                    // Allocate memory for buffer queue

                    //if (0 != this->mBufferQueue.mNumBuffers) {
                        // inline allocation of circular mArray, up to a typical max
                        if (BUFFER_HEADER_TYPICAL >= this->mBufferQueue.mNumBuffers) {
                            this->mBufferQueue.mArray = this->mBufferQueue.mTypical;
                        } else {
                            // Avoid possible integer overflow during multiplication; this arbitrary
                            // maximum is big enough to not interfere with real applications, but
                            // small enough to not overflow.
                            if (this->mBufferQueue.mNumBuffers >= 256) {
                                result = SL_RESULT_MEMORY_FAILURE;
                                break;
                            }
                            this->mBufferQueue.mArray = (BufferHeader *) malloc((this->mBufferQueue.
                                mNumBuffers + 1) * sizeof(BufferHeader));
                            if (NULL == this->mBufferQueue.mArray) {
                                result = SL_RESULT_MEMORY_FAILURE;
                                break;
                            }
                        }
                        this->mBufferQueue.mFront = this->mBufferQueue.mArray;
                        this->mBufferQueue.mRear = this->mBufferQueue.mArray;
                        //}

                        // used to store the data source of our audio player
                        this->mDynamicSource.mDataSource = &this->mDataSource.u.mSource;

                        // platform-specific initialization
#ifdef ANDROID
                        android_audioPlayer_create(this);
#endif

                } while (0);

                if (SL_RESULT_SUCCESS != result) {
                    IObject_Destroy(&this->mObject.mItf);
                } else {
                    IObject_Publish(&this->mObject);
                    // return the new audio player object
                    *pPlayer = &this->mObject.mItf;
                }

            }
        }

    }

    SL_LEAVE_INTERFACE
}


static SLresult IEngine_CreateAudioRecorder(SLEngineItf self, SLObjectItf *pRecorder,
    SLDataSource *pAudioSrc, SLDataSink *pAudioSnk, SLuint32 numInterfaces,
    const SLInterfaceID *pInterfaceIds, const SLboolean *pInterfaceRequired)
{
    SL_ENTER_INTERFACE

#if defined(USE_OPTIONAL) || defined(ANDROID)
    if (NULL == pRecorder) {
        result = SL_RESULT_PARAMETER_INVALID;
    } else {
        *pRecorder = NULL;
        unsigned exposedMask;
        const ClassTable *pCAudioRecorder_class = objectIDtoClass(SL_OBJECTID_AUDIORECORDER);
        result = checkInterfaces(pCAudioRecorder_class, numInterfaces,
                pInterfaceIds, pInterfaceRequired, &exposedMask);

        if (SL_RESULT_SUCCESS == result) {

            // Construct our new AudioRecorder instance
            CAudioRecorder *this = (CAudioRecorder *) construct(pCAudioRecorder_class, exposedMask,
                    self);
            if (NULL == this) {
                result = SL_RESULT_MEMORY_FAILURE;
            } else {

                do {
                    // const, will be set later by the containing AudioRecorder
                    this->mNumChannels = 0;
                    this->mSampleRateMilliHz = 0;

                    // Check the source and sink parameters, and make a local copy of all parameters
                    result = checkDataSource(pAudioSrc, &this->mDataSource);
                    if (SL_RESULT_SUCCESS != result) {
                        break;
                    }
                    result = checkDataSink(pAudioSnk, &this->mDataSink, SL_OBJECTID_AUDIORECORDER);
                    if (SL_RESULT_SUCCESS != result) {
                        break;
                    }

                    // check the audio source and sink parameters against platform support
#ifdef ANDROID
                    result = android_audioRecorder_checkSourceSinkSupport(this);
                    if (SL_RESULT_SUCCESS != result) {
                        SL_LOGE("Cannot create AudioRecorder: invalid source or sink");
                        break;
                    }
#endif

#ifdef ANDROID
                    // FIXME move to dedicated function
                    SLuint32 locatorType = *(SLuint32 *) pAudioSnk->pLocator;
                    if (locatorType == SL_DATALOCATOR_BUFFERQUEUE) {
                        this->mBufferQueue.mNumBuffers = ((SLDataLocator_BufferQueue *)
                                pAudioSnk->pLocator)->numBuffers;
                        // inline allocation of circular Buffer Queue mArray, up to a typical max
                        if (BUFFER_HEADER_TYPICAL >= this->mBufferQueue.mNumBuffers) {
                            this->mBufferQueue.mArray = this->mBufferQueue.mTypical;
                        } else {
                            // Avoid possible integer overflow during multiplication; this arbitrary
                            // maximum is big enough to not interfere with real applications, but
                            // small enough to not overflow.
                            if (this->mBufferQueue.mNumBuffers >= 256) {
                                result = SL_RESULT_MEMORY_FAILURE;
                                break;
                            }
                            this->mBufferQueue.mArray = (BufferHeader *) malloc((this->mBufferQueue.
                                    mNumBuffers + 1) * sizeof(BufferHeader));
                            if (NULL == this->mBufferQueue.mArray) {
                                result = SL_RESULT_MEMORY_FAILURE;
                                break;
                            }
                        }
                        this->mBufferQueue.mFront = this->mBufferQueue.mArray;
                        this->mBufferQueue.mRear = this->mBufferQueue.mArray;
                    }
#endif

                    // platform-specific initialization
#ifdef ANDROID
                    android_audioRecorder_create(this);
#endif

                } while (0);

                if (SL_RESULT_SUCCESS != result) {
                    IObject_Destroy(&this->mObject.mItf);
                } else {
                    IObject_Publish(&this->mObject);
                    // return the new audio recorder object
                    *pRecorder = &this->mObject.mItf;
                }
            }

        }

    }
#else
    result = SL_RESULT_FEATURE_UNSUPPORTED;
#endif

    SL_LEAVE_INTERFACE
}


static SLresult IEngine_CreateMidiPlayer(SLEngineItf self, SLObjectItf *pPlayer,
    SLDataSource *pMIDISrc, SLDataSource *pBankSrc, SLDataSink *pAudioOutput,
    SLDataSink *pVibra, SLDataSink *pLEDArray, SLuint32 numInterfaces,
    const SLInterfaceID *pInterfaceIds, const SLboolean *pInterfaceRequired)
{
    SL_ENTER_INTERFACE

#if defined(USE_GAME) || defined(USE_PHONE)
    if ((NULL == pPlayer) || (NULL == pMIDISrc) || (NULL == pAudioOutput)) {
        result = SL_RESULT_PARAMETER_INVALID;
    } else {
        *pPlayer = NULL;
        unsigned exposedMask;
        const ClassTable *pCMidiPlayer_class = objectIDtoClass(SL_OBJECTID_MIDIPLAYER);
        result = checkInterfaces(pCMidiPlayer_class, numInterfaces,
            pInterfaceIds, pInterfaceRequired, &exposedMask);
        if (SL_RESULT_SUCCESS == result) {
            CMidiPlayer *this = (CMidiPlayer *) construct(pCMidiPlayer_class, exposedMask, self);
            if (NULL == this) {
                result = SL_RESULT_MEMORY_FAILURE;
            } else {
                // FIXME a fake value - why not use value from IPlay_init? what does CT check for?
                this->mPlay.mDuration = 0;
                IObject_Publish(&this->mObject);
                // return the new MIDI player object
                *pPlayer = &this->mObject.mItf;
            }
        }
    }
#else
    result = SL_RESULT_FEATURE_UNSUPPORTED;
#endif

    SL_LEAVE_INTERFACE
}


static SLresult IEngine_CreateListener(SLEngineItf self, SLObjectItf *pListener,
    SLuint32 numInterfaces, const SLInterfaceID *pInterfaceIds, const SLboolean *pInterfaceRequired)
{
    SL_ENTER_INTERFACE

#ifdef USE_GAME
    if (NULL == pListener) {
        result = SL_RESULT_PARAMETER_INVALID;
    } else {
        *pListener = NULL;
        unsigned exposedMask;
        const ClassTable *pCListener_class = objectIDtoClass(SL_OBJECTID_LISTENER);
        result = checkInterfaces(pCListener_class, numInterfaces,
            pInterfaceIds, pInterfaceRequired, &exposedMask);
        if (SL_RESULT_SUCCESS == result) {
            CListener *this = (CListener *) construct(pCListener_class, exposedMask, self);
            if (NULL == this) {
                result = SL_RESULT_MEMORY_FAILURE;
            } else {
                IObject_Publish(&this->mObject);
                // return the new 3D listener object
                *pListener = &this->mObject.mItf;
            }
        }
    }
#else
    result = SL_RESULT_FEATURE_UNSUPPORTED;
#endif

    SL_LEAVE_INTERFACE
}


static SLresult IEngine_Create3DGroup(SLEngineItf self, SLObjectItf *pGroup, SLuint32 numInterfaces,
    const SLInterfaceID *pInterfaceIds, const SLboolean *pInterfaceRequired)
{
    SL_ENTER_INTERFACE

#ifdef USE_GAME
    if (NULL == pGroup) {
        result = SL_RESULT_PARAMETER_INVALID;
    } else {
        *pGroup = NULL;
        unsigned exposedMask;
        const ClassTable *pC3DGroup_class = objectIDtoClass(SL_OBJECTID_3DGROUP);
        result = checkInterfaces(pC3DGroup_class, numInterfaces,
            pInterfaceIds, pInterfaceRequired, &exposedMask);
        if (SL_RESULT_SUCCESS == result) {
            C3DGroup *this = (C3DGroup *) construct(pC3DGroup_class, exposedMask, self);
            if (NULL == this) {
                result = SL_RESULT_MEMORY_FAILURE;
            } else {
                this->mMemberMask = 0;
                IObject_Publish(&this->mObject);
                // return the new 3D group object
                *pGroup = &this->mObject.mItf;
            }
        }
    }
#else
    result = SL_RESULT_FEATURE_UNSUPPORTED;
#endif

    SL_LEAVE_INTERFACE
}


static SLresult IEngine_CreateOutputMix(SLEngineItf self, SLObjectItf *pMix, SLuint32 numInterfaces,
    const SLInterfaceID *pInterfaceIds, const SLboolean *pInterfaceRequired)
{
    SL_ENTER_INTERFACE

    if (NULL == pMix) {
        result = SL_RESULT_PARAMETER_INVALID;
    } else {
        *pMix = NULL;
        unsigned exposedMask;
        const ClassTable *pCOutputMix_class = objectIDtoClass(SL_OBJECTID_OUTPUTMIX);
        result = checkInterfaces(pCOutputMix_class, numInterfaces,
            pInterfaceIds, pInterfaceRequired, &exposedMask);
        if (SL_RESULT_SUCCESS == result) {
            COutputMix *this = (COutputMix *) construct(pCOutputMix_class, exposedMask, self);
            if (NULL == this) {
                result = SL_RESULT_MEMORY_FAILURE;
            } else {
#ifdef ANDROID
                android_outputMix_create(this);
#endif
#ifdef USE_SDL
                IEngine *thisEngine = this->mObject.mEngine;
                interface_lock_exclusive(thisEngine);
                bool unpause = false;
                if (NULL == thisEngine->mOutputMix) {
                    thisEngine->mOutputMix = this;
                    unpause = true;
                }
                interface_unlock_exclusive(thisEngine);
#endif
                IObject_Publish(&this->mObject);
#ifdef USE_SDL
                if (unpause) {
                    // Enable SDL_callback to be called periodically by SDL's internal thread
                    SDL_PauseAudio(0);
                }
#endif
                // return the new output mix object
                *pMix = &this->mObject.mItf;
            }
        }
    }

    SL_LEAVE_INTERFACE
}


static SLresult IEngine_CreateMetadataExtractor(SLEngineItf self, SLObjectItf *pMetadataExtractor,
    SLDataSource *pDataSource, SLuint32 numInterfaces, const SLInterfaceID *pInterfaceIds,
    const SLboolean *pInterfaceRequired)
{
    SL_ENTER_INTERFACE

#if defined(USE_GAME) || defined(USE_MUSIC)
    if (NULL == pMetadataExtractor) {
        result = SL_RESULT_PARAMETER_INVALID;
    } else {
        *pMetadataExtractor = NULL;
        unsigned exposedMask;
        const ClassTable *pCMetadataExtractor_class =
            objectIDtoClass(SL_OBJECTID_METADATAEXTRACTOR);
        result = checkInterfaces(pCMetadataExtractor_class, numInterfaces,
            pInterfaceIds, pInterfaceRequired, &exposedMask);
        if (SL_RESULT_SUCCESS == result) {
            CMetadataExtractor *this = (CMetadataExtractor *)
                construct(pCMetadataExtractor_class, exposedMask, self);
            if (NULL == this) {
                result = SL_RESULT_MEMORY_FAILURE;
            } else {
                IObject_Publish(&this->mObject);
                // return the new metadata extractor object
                *pMetadataExtractor = &this->mObject.mItf;
                result = SL_RESULT_SUCCESS;
            }
        }
    }
#else
    result = SL_RESULT_FEATURE_UNSUPPORTED;
#endif

    SL_LEAVE_INTERFACE
}


static SLresult IEngine_CreateExtensionObject(SLEngineItf self, SLObjectItf *pObject,
    void *pParameters, SLuint32 objectID, SLuint32 numInterfaces,
    const SLInterfaceID *pInterfaceIds, const SLboolean *pInterfaceRequired)
{
    SL_ENTER_INTERFACE

    if (NULL == pObject) {
        result = SL_RESULT_PARAMETER_INVALID;
    } else {
        *pObject = NULL;
        result = SL_RESULT_FEATURE_UNSUPPORTED;
    }

    SL_LEAVE_INTERFACE
}


static SLresult IEngine_QueryNumSupportedInterfaces(SLEngineItf self,
    SLuint32 objectID, SLuint32 *pNumSupportedInterfaces)
{
    SL_ENTER_INTERFACE

    if (NULL == pNumSupportedInterfaces) {
        result = SL_RESULT_PARAMETER_INVALID;
    } else {
        const ClassTable *class__ = objectIDtoClass(objectID);
        if (NULL == class__) {
            result = SL_RESULT_FEATURE_UNSUPPORTED;
        } else {
            SLuint32 count = 0;
            SLuint32 i;
            for (i = 0; i < class__->mInterfaceCount; ++i) {
                switch (class__->mInterfaces[i].mInterface) {
                case INTERFACE_IMPLICIT:
                case INTERFACE_EXPLICIT:
                case INTERFACE_DYNAMIC:
                    ++count;
                    break;
                case INTERFACE_UNAVAILABLE:
                    break;
                default:
                    assert(false);
                    break;
                }
            }
            *pNumSupportedInterfaces = count;
            result = SL_RESULT_SUCCESS;
        }
    }

    SL_LEAVE_INTERFACE;
}


static SLresult IEngine_QuerySupportedInterfaces(SLEngineItf self,
    SLuint32 objectID, SLuint32 index, SLInterfaceID *pInterfaceId)
{
    SL_ENTER_INTERFACE

    if (NULL == pInterfaceId) {
        result = SL_RESULT_PARAMETER_INVALID;
    } else {
        *pInterfaceId = NULL;
        const ClassTable *class__ = objectIDtoClass(objectID);
        if (NULL == class__) {
            result = SL_RESULT_FEATURE_UNSUPPORTED;
        } else {
            result = SL_RESULT_PARAMETER_INVALID; // will be reset later
            SLuint32 i;
            for (i = 0; i < class__->mInterfaceCount; ++i) {
                switch (class__->mInterfaces[i].mInterface) {
                case INTERFACE_IMPLICIT:
                case INTERFACE_EXPLICIT:
                case INTERFACE_DYNAMIC:
                    break;
                case INTERFACE_UNAVAILABLE:
                    continue;
                default:
                    assert(false);
                    break;
                }
                if (index == 0) {
                    *pInterfaceId = &SL_IID_array[class__->mInterfaces[i].mMPH];
                    result = SL_RESULT_SUCCESS;
                    break;
                }
                --index;
            }
        }
    }

    SL_LEAVE_INTERFACE
};


static SLresult IEngine_QueryNumSupportedExtensions(SLEngineItf self, SLuint32 *pNumExtensions)
{
    SL_ENTER_INTERFACE

    if (NULL == pNumExtensions) {
        result = SL_RESULT_PARAMETER_INVALID;
    } else {
#ifdef ANDROID
        // FIXME support Android extensions
#else
        *pNumExtensions = 0;
#endif
        result = SL_RESULT_SUCCESS;
    }

    SL_LEAVE_INTERFACE
}


static SLresult IEngine_QuerySupportedExtension(SLEngineItf self,
    SLuint32 index, SLchar *pExtensionName, SLint16 *pNameLength)
{
    SL_ENTER_INTERFACE

    // any index >= 0 will be >= number of supported extensions

#ifdef ANDROID
    // FIXME support Android extensions
    result = SL_RESULT_PARAMETER_INVALID;
#else
    result = SL_RESULT_PARAMETER_INVALID;
#endif

    SL_LEAVE_INTERFACE
}


static SLresult IEngine_IsExtensionSupported(SLEngineItf self,
    const SLchar *pExtensionName, SLboolean *pSupported)
{
    SL_ENTER_INTERFACE

    if (NULL == pExtensionName || NULL == pSupported) {
        result = SL_RESULT_PARAMETER_INVALID;
    } else {
#ifdef ANDROID
        // FIXME support Android extensions
        *pSupported = SL_BOOLEAN_FALSE;
#else
        // no extensions are supported
        *pSupported = SL_BOOLEAN_FALSE;
#endif
        result = SL_RESULT_SUCCESS;
    }

    SL_LEAVE_INTERFACE
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
    for (i = 0; i < MAX_INSTANCE; ++i) {
        this->mInstances[i] = NULL;
    }
    this->mShutdown = SL_BOOLEAN_FALSE;
    this->mShutdownAck = SL_BOOLEAN_FALSE;
#if defined(ANDROID) && !defined(USE_BACKPORT)
    this->mEqNumPresets = 0;
    this->mEqPresetNames = NULL;
#endif
}
