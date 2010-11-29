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

/** Data locator, data format, data source, and data sink support */

#include "sles_allinclusive.h"


/** \brief Check a data locator and make local deep copy */

static SLresult checkDataLocator(void *pLocator, DataLocator *pDataLocator)
{
    if (NULL == pLocator) {
        pDataLocator->mLocatorType = SL_DATALOCATOR_NULL;
        return SL_RESULT_SUCCESS;
    }
    SLresult result;
    SLuint32 locatorType = *(SLuint32 *)pLocator;
    switch (locatorType) {

    case SL_DATALOCATOR_ADDRESS:
        pDataLocator->mAddress = *(SLDataLocator_Address *)pLocator;
        // if length is greater than zero, then the address must be non-NULL
        if ((0 < pDataLocator->mAddress.length) && (NULL == pDataLocator->mAddress.pAddress)) {
            SL_LOGE("pAddress is NULL");
            return SL_RESULT_PARAMETER_INVALID;
        }
        break;

    case SL_DATALOCATOR_BUFFERQUEUE:
#ifdef ANDROID
    // This is an alias that is _not_ converted; the rest of the code must check for both locator
    // types. That's because it is only an alias for audio players, not audio recorder objects
    // so we have to remember the distinction.
    case SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE:
#endif
        pDataLocator->mBufferQueue = *(SLDataLocator_BufferQueue *)pLocator;
        // number of buffers must be specified, there is no default value, and must not be excessive
        if (!((1 <= pDataLocator->mBufferQueue.numBuffers) &&
            (pDataLocator->mBufferQueue.numBuffers <= 255))) {
            SL_LOGE("numBuffers=%u", (unsigned) pDataLocator->mBufferQueue.numBuffers);
            return SL_RESULT_PARAMETER_INVALID;
        }
        break;

    case SL_DATALOCATOR_IODEVICE:
        {
        pDataLocator->mIODevice = *(SLDataLocator_IODevice *)pLocator;
        SLuint32 deviceType = pDataLocator->mIODevice.deviceType;
        SLObjectItf device = pDataLocator->mIODevice.device;
        if (NULL != device) {
            pDataLocator->mIODevice.deviceID = 0;
            SLuint32 expectedObjectID;
            switch (deviceType) {
            case SL_IODEVICE_LEDARRAY:
                expectedObjectID = SL_OBJECTID_LEDDEVICE;
                break;
            case SL_IODEVICE_VIBRA:
                expectedObjectID = SL_OBJECTID_VIBRADEVICE;
                break;
            // audio input and audio output cannot be specified via objects
            case SL_IODEVICE_AUDIOINPUT:
            // worse yet, an SL_IODEVICE enum constant for audio output does not exist yet
            // case SL_IODEVICE_AUDIOOUTPUT:
            default:
                SL_LOGE("invalid deviceType %lu", deviceType);
                pDataLocator->mIODevice.device = NULL;
                return SL_RESULT_PARAMETER_INVALID;
            }
            // check that device has the correct object ID and is realized,
            // and acquire a strong reference to it
            result = AcquireStrongRef((IObject *) device, expectedObjectID);
            if (SL_RESULT_SUCCESS != result) {
                SL_LOGE("locator type is IODEVICE, but device field %p has wrong object ID or is " \
                    "not realized", device);
                pDataLocator->mIODevice.device = NULL;
                return result;
            }
        } else {
            SLuint32 deviceID = pDataLocator->mIODevice.deviceID;
            switch (deviceType) {
            case SL_IODEVICE_LEDARRAY:
                if (SL_DEFAULTDEVICEID_LED != deviceID) {
                    SL_LOGE("invalid LED deviceID %lu", deviceID);
                    return SL_RESULT_PARAMETER_INVALID;
                }
                break;
            case SL_IODEVICE_VIBRA:
                if (SL_DEFAULTDEVICEID_VIBRA != deviceID) {
                    SL_LOGE("invalid vibra deviceID %lu", deviceID);
                    return SL_RESULT_PARAMETER_INVALID;
                }
                break;
            case SL_IODEVICE_AUDIOINPUT:
                if (SL_DEFAULTDEVICEID_AUDIOINPUT != deviceID) {
                    SL_LOGE("invalid audio input deviceID %lu", deviceID);
                    return SL_RESULT_PARAMETER_INVALID;
                }
                break;
            default:
                SL_LOGE("invalid deviceType %lu", deviceType);
                return SL_RESULT_PARAMETER_INVALID;
            }
        }
        }
        break;

    case SL_DATALOCATOR_MIDIBUFFERQUEUE:
        pDataLocator->mMIDIBufferQueue = *(SLDataLocator_MIDIBufferQueue *)pLocator;
        if (0 == pDataLocator->mMIDIBufferQueue.tpqn) {
            pDataLocator->mMIDIBufferQueue.tpqn = 192;
        }
        // number of buffers must be specified, there is no default value, and must not be excessive
        if (!((1 <= pDataLocator->mMIDIBufferQueue.numBuffers) &&
            (pDataLocator->mMIDIBufferQueue.numBuffers <= 255))) {
            SL_LOGE("invalid MIDI buffer queue");
            return SL_RESULT_PARAMETER_INVALID;
        }
        break;

    case SL_DATALOCATOR_OUTPUTMIX:
        pDataLocator->mOutputMix = *(SLDataLocator_OutputMix *)pLocator;
        // check that output mix object has the correct object ID and is realized,
        // and acquire a strong reference to it
        result = AcquireStrongRef((IObject *) pDataLocator->mOutputMix.outputMix,
            SL_OBJECTID_OUTPUTMIX);
        if (SL_RESULT_SUCCESS != result) {
            SL_LOGE("locatorType is SL_DATALOCATOR_OUTPUTMIX, but outputMix field %p does not " \
                "refer to an SL_OBJECTID_OUTPUTMIX or the output mix is not realized", \
                pDataLocator->mOutputMix.outputMix);
            pDataLocator->mOutputMix.outputMix = NULL;
            return result;
        }
        break;

    case SL_DATALOCATOR_URI:
        {
        pDataLocator->mURI = *(SLDataLocator_URI *)pLocator;
        if (NULL == pDataLocator->mURI.URI) {
            SL_LOGE("invalid URI");
            return SL_RESULT_PARAMETER_INVALID;
        }
        // NTH verify URI address for validity
        size_t len = strlen((const char *) pDataLocator->mURI.URI);
        SLchar *myURI = (SLchar *) malloc(len + 1);
        if (NULL == myURI) {
            pDataLocator->mURI.URI = NULL;
            return SL_RESULT_MEMORY_FAILURE;
        }
        memcpy(myURI, pDataLocator->mURI.URI, len + 1);
        // Verify that another thread didn't change the NUL-terminator after we used it
        // to determine length of string to copy. It's OK if the string became shorter.
        if ('\0' != myURI[len]) {
            free(myURI);
            pDataLocator->mURI.URI = NULL;
            return SL_RESULT_PARAMETER_INVALID;
        }
        pDataLocator->mURI.URI = myURI;
        }
        break;

#ifdef ANDROID
    case SL_DATALOCATOR_ANDROIDFD:
        {
        pDataLocator->mFD = *(SLDataLocator_AndroidFD *)pLocator;
        SL_LOGV("Data locator FD: fd=%ld offset=%lld length=%lld", pDataLocator->mFD.fd,
                pDataLocator->mFD.offset, pDataLocator->mFD.length);
        // NTH check against process fd limit
        if (0 > pDataLocator->mFD.fd) {
            return SL_RESULT_PARAMETER_INVALID;
        }
        }
        break;
    case SL_DATALOCATOR_ANDROIDBUFFERQUEUE:
        {
        pDataLocator->mBQ = *(SLDataLocator_AndroidBufferQueue*)pLocator;
        }
        break;
#endif

    default:
        SL_LOGE("invalid locatorType %lu", locatorType);
        return SL_RESULT_PARAMETER_INVALID;
    }

    // Verify that another thread didn't change the locatorType field after we used it
    // to determine sizeof struct to copy.
    if (locatorType != pDataLocator->mLocatorType) {
        return SL_RESULT_PARAMETER_INVALID;
    }
    return SL_RESULT_SUCCESS;
}


/** \brief Free the local deep copy of a data locator */

static void freeDataLocator(DataLocator *pDataLocator)
{
    switch (pDataLocator->mLocatorType) {
    case SL_DATALOCATOR_URI:
        if (NULL != pDataLocator->mURI.URI) {
            free(pDataLocator->mURI.URI);
            pDataLocator->mURI.URI = NULL;
        }
        pDataLocator->mURI.URI = NULL;
        break;
    case SL_DATALOCATOR_IODEVICE:
        if (NULL != pDataLocator->mIODevice.device) {
            ReleaseStrongRef((IObject *) pDataLocator->mIODevice.device);
            pDataLocator->mIODevice.device = NULL;
        }
        break;
    case SL_DATALOCATOR_OUTPUTMIX:
        if (NULL != pDataLocator->mOutputMix.outputMix) {
            ReleaseStrongRef((IObject *) pDataLocator->mOutputMix.outputMix);
            pDataLocator->mOutputMix.outputMix = NULL;
        }
        break;
#ifdef ANDROID
    case SL_DATALOCATOR_ANDROIDBUFFERQUEUE:

        break;
#endif
    default:
        break;
    }
}


/** \brief Check a data format and make local deep copy */

static SLresult checkDataFormat(void *pFormat, DataFormat *pDataFormat)
{
    SLresult result = SL_RESULT_SUCCESS;

    if (NULL == pFormat) {
        pDataFormat->mFormatType = SL_DATAFORMAT_NULL;
    } else {
        SLuint32 formatType = *(SLuint32 *)pFormat;
        switch (formatType) {

        case SL_DATAFORMAT_PCM:
            pDataFormat->mPCM = *(SLDataFormat_PCM *)pFormat;
            do {

                // check the channel count
                switch (pDataFormat->mPCM.numChannels) {
                case 1:     // mono
                case 2:     // stereo
                    break;
                case 0:     // unknown
                    result = SL_RESULT_PARAMETER_INVALID;
                    break;
                default:    // multi-channel
                    result = SL_RESULT_CONTENT_UNSUPPORTED;
                    break;
                }
                if (SL_RESULT_SUCCESS != result) {
                    SL_LOGE("numChannels=%u", (unsigned) pDataFormat->mPCM.numChannels);
                    break;
                }

                // check the sampling rate
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
                    result = SL_RESULT_PARAMETER_INVALID;
                    break;
                default:
                    result = SL_RESULT_CONTENT_UNSUPPORTED;
                    break;
                }
                if (SL_RESULT_SUCCESS != result) {
                    SL_LOGE("samplesPerSec=%u", (unsigned) pDataFormat->mPCM.samplesPerSec);
                    break;
                }

                // check the sample bit depth
                switch (pDataFormat->mPCM.bitsPerSample) {
                case SL_PCMSAMPLEFORMAT_FIXED_8:
                case SL_PCMSAMPLEFORMAT_FIXED_16:
                    break;
                case SL_PCMSAMPLEFORMAT_FIXED_20:
                case SL_PCMSAMPLEFORMAT_FIXED_24:
                case SL_PCMSAMPLEFORMAT_FIXED_28:
                case SL_PCMSAMPLEFORMAT_FIXED_32:
                    result = SL_RESULT_CONTENT_UNSUPPORTED;
                    break;
                default:
                    result = SL_RESULT_PARAMETER_INVALID;
                    break;
                }
                if (SL_RESULT_SUCCESS != result) {
                    SL_LOGE("bitsPerSample=%u", (unsigned) pDataFormat->mPCM.bitsPerSample);
                    break;
                }

                // check the container bit depth
                if (pDataFormat->mPCM.containerSize < pDataFormat->mPCM.bitsPerSample) {
                    result = SL_RESULT_PARAMETER_INVALID;
                } else if (pDataFormat->mPCM.containerSize != pDataFormat->mPCM.bitsPerSample) {
                    result = SL_RESULT_CONTENT_UNSUPPORTED;
                }
                if (SL_RESULT_SUCCESS != result) {
                    SL_LOGE("containerSize=%u, bitsPerSample=%u",
                            (unsigned) pDataFormat->mPCM.containerSize,
                            (unsigned) pDataFormat->mPCM.bitsPerSample);
                    break;
                }

                // check the channel mask
                switch (pDataFormat->mPCM.channelMask) {
                case SL_SPEAKER_FRONT_LEFT | SL_SPEAKER_FRONT_RIGHT:
                    if (2 != pDataFormat->mPCM.numChannels) {
                        result = SL_RESULT_PARAMETER_INVALID;
                    }
                    break;
                case SL_SPEAKER_FRONT_LEFT:
                case SL_SPEAKER_FRONT_RIGHT:
                case SL_SPEAKER_FRONT_CENTER:
                    if (1 != pDataFormat->mPCM.numChannels) {
                        result = SL_RESULT_PARAMETER_INVALID;
                    }
                    break;
                case 0:
                    pDataFormat->mPCM.channelMask = pDataFormat->mPCM.numChannels == 2 ?
                        SL_SPEAKER_FRONT_LEFT | SL_SPEAKER_FRONT_RIGHT : SL_SPEAKER_FRONT_CENTER;
                    break;
                default:
                    result = SL_RESULT_PARAMETER_INVALID;
                    break;
                }
                if (SL_RESULT_SUCCESS != result) {
                    SL_LOGE("channelMask=0x%lx numChannels=%lu", pDataFormat->mPCM.channelMask,
                        pDataFormat->mPCM.numChannels);
                    break;
                }

                // check the endianness / byte order
                switch (pDataFormat->mPCM.endianness) {
                case SL_BYTEORDER_LITTLEENDIAN:
                case SL_BYTEORDER_BIGENDIAN:
                    break;
                // native is proposed but not yet in spec
                default:
                    result = SL_RESULT_PARAMETER_INVALID;
                    break;
                }
                if (SL_RESULT_SUCCESS != result) {
                    SL_LOGE("endianness=%u", (unsigned) pDataFormat->mPCM.endianness);
                    break;
                }

                // here if all checks passed successfully

            } while(0);
            break;

        case SL_DATAFORMAT_MIME:
            pDataFormat->mMIME = *(SLDataFormat_MIME *)pFormat;
            if (NULL != pDataFormat->mMIME.mimeType) {
                // NTH check address for validity
                size_t len = strlen((const char *) pDataFormat->mMIME.mimeType);
                SLchar *myMIME = (SLchar *) malloc(len + 1);
                if (NULL == myMIME) {
                    result = SL_RESULT_MEMORY_FAILURE;
                } else {
                    memcpy(myMIME, pDataFormat->mMIME.mimeType, len + 1);
                    // make sure MIME string was not modified asynchronously
                    if ('\0' != myMIME[len]) {
                        free(myMIME);
                        myMIME = NULL;
                        result = SL_RESULT_PRECONDITIONS_VIOLATED;
                    }
                }
                pDataFormat->mMIME.mimeType = myMIME;
            }
            break;

        default:
            result = SL_RESULT_PARAMETER_INVALID;
            SL_LOGE("formatType=%u", (unsigned) formatType);
            break;

        }

        // make sure format type was not modified asynchronously
        if ((SL_RESULT_SUCCESS == result) && (formatType != pDataFormat->mFormatType)) {
            result = SL_RESULT_PRECONDITIONS_VIOLATED;
        }

    }

    return result;
}


/** \brief Check interface ID compatibility with respect to a particular data locator format */

SLresult checkSourceFormatVsInterfacesCompatibility(const DataLocatorFormat *pDataLocatorFormat,
        const ClassTable *class__, unsigned exposedMask) {
    int index;
    switch (pDataLocatorFormat->mLocator.mLocatorType) {
    case SL_DATALOCATOR_BUFFERQUEUE:
#ifdef ANDROID
    case SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE:
#endif
        // can't request SLSeekItf if data source is a buffer queue
        index = class__->mMPH_to_index[MPH_SEEK];
        if (0 <= index) {
            if (exposedMask & (1 << index)) {
                SL_LOGE("can't request SL_IID_SEEK with a buffer queue data source");
                return SL_RESULT_FEATURE_UNSUPPORTED;
            }
        }
        // can't request SLMuteSoloItf if data source is a mono buffer queue
        index = class__->mMPH_to_index[MPH_MUTESOLO];
        if (0 <= index) {
            if ((exposedMask & (1 << index)) &&
                    (SL_DATAFORMAT_PCM == pDataLocatorFormat->mFormat.mFormatType) &&
                    (1 == pDataLocatorFormat->mFormat.mPCM.numChannels)) {
                SL_LOGE("can't request SL_IID_MUTESOLO with a mono buffer queue data source");
                return SL_RESULT_FEATURE_UNSUPPORTED;
            }
        }
        break;
    default:
        // can't request SLBufferQueueItf or its alias SLAndroidSimpleBufferQueueItf
        // if the data source is not a buffer queue
        index = class__->mMPH_to_index[MPH_BUFFERQUEUE];
#ifdef ANDROID
        assert(index == class__->mMPH_to_index[MPH_ANDROIDSIMPLEBUFFERQUEUE]);
#endif
        if (0 <= index) {
            if (exposedMask & (1 << index)) {
                SL_LOGE("can't request SL_IID_BUFFERQUEUE "
#ifdef ANDROID
                        "or SL_IID_ANDROIDSIMPLEBUFFERQUEUE "
#endif
                        "with a non-buffer queue data source");
                return SL_RESULT_FEATURE_UNSUPPORTED;
            }
        }
        break;
    }
    return SL_RESULT_SUCCESS;
}


/** \brief Free the local deep copy of a data format */

static void freeDataFormat(DataFormat *pDataFormat)
{
    switch (pDataFormat->mFormatType) {
    case SL_DATAFORMAT_MIME:
        if (NULL != pDataFormat->mMIME.mimeType) {
            free(pDataFormat->mMIME.mimeType);
            pDataFormat->mMIME.mimeType = NULL;
        }
        break;
    default:
        break;
    }
}


/** \brief Check a data source and make local deep copy */

SLresult checkDataSource(const SLDataSource *pDataSrc, DataLocatorFormat *pDataLocatorFormat)
{
    if (NULL == pDataSrc) {
        SL_LOGE("pDataSrc NULL");
        return SL_RESULT_PARAMETER_INVALID;
    }
    SLDataSource myDataSrc = *pDataSrc;
    SLresult result;
    result = checkDataLocator(myDataSrc.pLocator, &pDataLocatorFormat->mLocator);
    if (SL_RESULT_SUCCESS != result) {
        return result;
    }
    switch (pDataLocatorFormat->mLocator.mLocatorType) {

    case SL_DATALOCATOR_URI:
    case SL_DATALOCATOR_ADDRESS:
    case SL_DATALOCATOR_BUFFERQUEUE:
    case SL_DATALOCATOR_MIDIBUFFERQUEUE:
#ifdef ANDROID
    case SL_DATALOCATOR_ANDROIDFD:
    case SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE:
    case SL_DATALOCATOR_ANDROIDBUFFERQUEUE:
#endif
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
        SL_LOGE("mLocatorType=%u", (unsigned) pDataLocatorFormat->mLocator.mLocatorType);
        // keep going

    case SL_DATALOCATOR_IODEVICE:
        // for these data locator types, ignore the pFormat as it might be uninitialized
        pDataLocatorFormat->mFormat.mFormatType = SL_DATAFORMAT_NULL;
        break;
    }

    pDataLocatorFormat->u.mSource.pLocator = &pDataLocatorFormat->mLocator;
    pDataLocatorFormat->u.mSource.pFormat = &pDataLocatorFormat->mFormat;
    return SL_RESULT_SUCCESS;
}


/** \brief Check a data sink and make local deep copy */

SLresult checkDataSink(const SLDataSink *pDataSink, DataLocatorFormat *pDataLocatorFormat,
        SLuint32 objType)
{
    if (NULL == pDataSink) {
        SL_LOGE("pDataSink NULL");
        return SL_RESULT_PARAMETER_INVALID;
    }
    SLDataSink myDataSink = *pDataSink;
    SLresult result;
    result = checkDataLocator(myDataSink.pLocator, &pDataLocatorFormat->mLocator);
    if (SL_RESULT_SUCCESS != result) {
        return result;
    }
    switch (pDataLocatorFormat->mLocator.mLocatorType) {

    case SL_DATALOCATOR_URI:
    case SL_DATALOCATOR_ADDRESS:
        result = checkDataFormat(myDataSink.pFormat, &pDataLocatorFormat->mFormat);
        if (SL_RESULT_SUCCESS != result) {
            freeDataLocator(&pDataLocatorFormat->mLocator);
            return result;
        }
        break;

    case SL_DATALOCATOR_BUFFERQUEUE:
#ifdef ANDROID
    case SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE:
#endif
        if (SL_OBJECTID_AUDIOPLAYER == objType) {
            SL_LOGE("buffer queue can't be used as data sink for audio player");
            result = SL_RESULT_PARAMETER_INVALID;
        } else if (SL_OBJECTID_AUDIORECORDER == objType) {
#ifdef ANDROID
            if (SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE !=
                pDataLocatorFormat->mLocator.mLocatorType) {
                SL_LOGE("audio recorder source locator must be SL_DATALOCATOR_ANDROIDBUFFERQUEUE");
                result = SL_RESULT_PARAMETER_INVALID;
            } else {
                result = checkDataFormat(myDataSink.pFormat, &pDataLocatorFormat->mFormat);
            }
#else
            SL_LOGE("mLocatorType=%u", (unsigned) pDataLocatorFormat->mLocator.mLocatorType);
            result = SL_RESULT_PARAMETER_INVALID;
#endif
        }
        if (SL_RESULT_SUCCESS != result) {
            freeDataLocator(&pDataLocatorFormat->mLocator);
            return result;
        }
        break;

    case SL_DATALOCATOR_NULL:
    case SL_DATALOCATOR_MIDIBUFFERQUEUE:
    default:
        // invalid but fall through; the invalid locator will be caught later
        SL_LOGE("mLocatorType=%u", (unsigned) pDataLocatorFormat->mLocator.mLocatorType);
        // keep going

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


/** \brief Free the local deep copy of a data locator format */

void freeDataLocatorFormat(DataLocatorFormat *dlf)
{
    freeDataLocator(&dlf->mLocator);
    freeDataFormat(&dlf->mFormat);
}
