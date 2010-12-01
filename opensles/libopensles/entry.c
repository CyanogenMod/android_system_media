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

/* Initial global entry points */

#include "sles_allinclusive.h"


/** \brief Internal code shared by slCreateEngine and xaCreateEngine */

static SLresult liCreateEngine(SLObjectItf *pEngine, SLuint32 numOptions,
    const SLEngineOption *pEngineOptions, SLuint32 numInterfaces,
    const SLInterfaceID *pInterfaceIds, const SLboolean *pInterfaceRequired,
    const ClassTable *pCEngine_class)
{
    SLresult result;

    int ok;
    ok = pthread_mutex_lock(&theOneTrueMutex);
    assert(0 == ok);

    do {

#ifdef ANDROID
        android::ProcessState::self()->startThreadPool();
#ifndef USE_BACKPORT
        android::DataSource::RegisterDefaultSniffers();
#endif
#endif

        if (NULL == pEngine) {
            result = SL_RESULT_PARAMETER_INVALID;
            break;
        }
        *pEngine = NULL;

        if (NULL != theOneTrueEngine) {
            SL_LOGE("slCreateEngine while another engine %p is active", theOneTrueEngine);
            result = SL_RESULT_RESOURCE_ERROR;
            break;
        }

        if ((0 < numOptions) && (NULL == pEngineOptions)) {
            SL_LOGE("numOptions=%lu and pEngineOptions=NULL", numOptions);
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
                SL_LOGE("unknown engine option: feature=%lu data=%lu",
                    option->feature, option->data);
                result = SL_RESULT_PARAMETER_INVALID;
                break;
            }
        }
        if (SL_RESULT_SUCCESS != result) {
            break;
        }

        unsigned exposedMask;
        // const ClassTable *pCEngine_class = objectIDtoClass(SL_OBJECTID_ENGINE);
        assert(NULL != pCEngine_class);
        result = checkInterfaces(pCEngine_class, numInterfaces,
            pInterfaceIds, pInterfaceRequired, &exposedMask);
        if (SL_RESULT_SUCCESS != result) {
            break;
        }

        CEngine *this = (CEngine *) construct(pCEngine_class, exposedMask, NULL);
        if (NULL == this) {
            result = SL_RESULT_MEMORY_FAILURE;
            break;
        }

        // initialize fields not associated with an interface
        // mThreadPool is initialized in CEngine_Realize
        memset(&this->mThreadPool, 0, sizeof(ThreadPool));
        memset(&this->mSyncThread, 0, sizeof(pthread_t));
#if defined(ANDROID) && !defined(USE_BACKPORT)
        this->mEqNumPresets = 0;
        this->mEqPresetNames = NULL;
#endif
        // initialize fields related to an interface
        this->mObject.mLossOfControlMask = lossOfControlGlobal ? ~0 : 0;
        this->mEngine.mLossOfControlGlobal = lossOfControlGlobal;
        this->mEngineCapabilities.mThreadSafe = threadSafe;
        IObject_Publish(&this->mObject);
        theOneTrueEngine = this;
        // return the new engine object
        *pEngine = &this->mObject.mItf;

    } while(0);

    ok = pthread_mutex_unlock(&theOneTrueMutex);
    assert(0 == ok);

    return result;
}


/** \brief slCreateEngine Function */

SL_API SLresult SLAPIENTRY slCreateEngine(SLObjectItf *pEngine, SLuint32 numOptions,
    const SLEngineOption *pEngineOptions, SLuint32 numInterfaces,
    const SLInterfaceID *pInterfaceIds, const SLboolean *pInterfaceRequired)
{
    SL_ENTER_GLOBAL

    result = liCreateEngine(pEngine, numOptions, pEngineOptions, numInterfaces, pInterfaceIds,
            pInterfaceRequired, objectIDtoClass(SL_OBJECTID_ENGINE));

    SL_LEAVE_GLOBAL
}


/** Internal function for slQuerySupportedEngineInterfaces and xaQuerySupportedEngineInterfaces */

static SLresult liQueryNumSupportedInterfaces(SLuint32 *pNumSupportedInterfaces,
        const ClassTable *class__)
{
    SLresult result;
    if (NULL == pNumSupportedInterfaces) {
        result = SL_RESULT_PARAMETER_INVALID;
    } else {
        assert(NULL != class__);
        SLuint32 count = 0;
        SLuint32 i;
        for (i = 0; i < class__->mInterfaceCount; ++i) {
            switch (class__->mInterfaces[i].mInterface) {
            case INTERFACE_IMPLICIT:
            case INTERFACE_IMPLICIT_PREREALIZE:
            case INTERFACE_EXPLICIT:
            case INTERFACE_EXPLICIT_PREREALIZE:
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
    return result;
}


/** \brief slQueryNumSupportedEngineInterfaces Function */

SL_API SLresult SLAPIENTRY slQueryNumSupportedEngineInterfaces(SLuint32 *pNumSupportedInterfaces)
{
    SL_ENTER_GLOBAL

    result = liQueryNumSupportedInterfaces(pNumSupportedInterfaces,
            objectIDtoClass(SL_OBJECTID_ENGINE));

    SL_LEAVE_GLOBAL
}


/** Internal function for slQuerySupportedEngineInterfaces and xaQuerySupportedEngineInterfaces */

static SLresult liQuerySupportedInterfaces(SLuint32 index, SLInterfaceID *pInterfaceId,
        const ClassTable *class__)
{
    SLresult result;
    if (NULL == pInterfaceId) {
        result = SL_RESULT_PARAMETER_INVALID;
    } else {
        *pInterfaceId = NULL;
        assert(NULL != class__);
        result = SL_RESULT_PARAMETER_INVALID;   // will be reset later
        SLuint32 i;
        for (i = 0; i < class__->mInterfaceCount; ++i) {
            switch (class__->mInterfaces[i].mInterface) {
            case INTERFACE_IMPLICIT:
            case INTERFACE_IMPLICIT_PREREALIZE:
            case INTERFACE_EXPLICIT:
            case INTERFACE_EXPLICIT_PREREALIZE:
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
    return result;
}


/** \brief slQuerySupportedEngineInterfaces Function */

SL_API SLresult SLAPIENTRY slQuerySupportedEngineInterfaces(SLuint32 index,
        SLInterfaceID *pInterfaceId)
{
    SL_ENTER_GLOBAL

    result = liQuerySupportedInterfaces(index, pInterfaceId, objectIDtoClass(SL_OBJECTID_ENGINE));

    SL_LEAVE_GLOBAL
}


/** \brief xaCreateEngine Function */

XA_API XAresult XAAPIENTRY xaCreateEngine(XAObjectItf *pEngine, XAuint32 numOptions,
        const XAEngineOption *pEngineOptions, XAuint32 numInterfaces,
        const XAInterfaceID *pInterfaceIds, const XAboolean *pInterfaceRequired)
{
    XA_ENTER_GLOBAL

    result = liCreateEngine((SLObjectItf *) pEngine, numOptions,
            (const SLEngineOption *) pEngineOptions, numInterfaces,
            (const SLInterfaceID *) pInterfaceIds, (const SLboolean *) pInterfaceRequired,
            objectIDtoClass(XA_OBJECTID_ENGINE));

    XA_LEAVE_GLOBAL
}


/** \brief xaQueryNumSupportedEngineInterfaces Function */

XA_API XAresult XAAPIENTRY xaQueryNumSupportedEngineInterfaces(XAuint32 *pNumSupportedInterfaces)
{
    XA_ENTER_GLOBAL

    result = liQueryNumSupportedInterfaces(pNumSupportedInterfaces,
            objectIDtoClass(XA_OBJECTID_ENGINE));

    XA_LEAVE_GLOBAL
}


/** \brief xaQuerySupportedEngineInterfaces Function */

XA_API XAresult XAAPIENTRY xaQuerySupportedEngineInterfaces(XAuint32 index,
        XAInterfaceID *pInterfaceId)
{
    XA_ENTER_GLOBAL

    result = liQuerySupportedInterfaces(index, (SLInterfaceID *) pInterfaceId,
            objectIDtoClass(XA_OBJECTID_ENGINE));

    XA_LEAVE_GLOBAL
}
