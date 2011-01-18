/*
 * Copyright (C) 2011 The Android Open Source Project
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

/* StreamInformation implementation */

#include "sles_allinclusive.h"

static XAresult IStreamInformation_QueryMediaContainerInformation( XAStreamInformationItf self,
        XAMediaContainerInformation * info /* [out] */)
{
    XA_ENTER_INTERFACE

#ifdef ANDROID
    IStreamInformation *thiz = (IStreamInformation *) self;
    interface_lock_exclusive(thiz);
    info = (XAMediaContainerInformation*)&(thiz->mStreamInfoTable.itemAt(0).containerInfo);
    interface_unlock_exclusive(thiz);
    // even though the pointer to the media container info is returned, the values aren't set
    //  for the actual container in this version, they are simply initialized to defaults
    //  (see IStreamInformation_init)
    result = XA_RESULT_CONTENT_UNSUPPORTED;
#else
    SL_LOGE("QueryMediaContainerInformation is unsupported");
    info = NULL;
    result = XA_RESULT_CONTENT_UNSUPPORTED;
#endif

    XA_LEAVE_INTERFACE
}


static XAresult IStreamInformation_QueryStreamType( XAStreamInformationItf self,
        XAuint32 streamIndex, /* [in] */
        XAuint32 *domain)     /* [out] */
{
    XA_ENTER_INTERFACE

    if (NULL == domain) {
        result = XA_RESULT_PARAMETER_INVALID;
        XA_LEAVE_INTERFACE;
    }

#ifndef ANDROID
    *domain =  XA_DOMAINTYPE_UNKNOWN;
#else
    if (0 == streamIndex) {
        // stream 0 is reserved for the container
        result = XA_RESULT_PARAMETER_INVALID;
    } else {
        IStreamInformation *thiz = (IStreamInformation *) self;

        interface_lock_exclusive(thiz);
        XAuint32 nbStreams = thiz->mStreamInfoTable.itemAt(0).containerInfo.numStreams;
        // streams in the container are numbered 1..nbStreams
        if (streamIndex <= nbStreams) {
            result = XA_RESULT_SUCCESS;
            *domain = thiz->mStreamInfoTable.itemAt(streamIndex).domain;
        } else {
            SL_LOGE("Querying stream type for stream %ld, only %ld streams available",
                    streamIndex, nbStreams);
            result = XA_RESULT_PARAMETER_INVALID;
        }
        interface_unlock_exclusive(thiz);
    }
#endif

    XA_LEAVE_INTERFACE
}


static XAresult IStreamInformation_QueryStreamInformation( XAStreamInformationItf self,
        XAuint32 streamIndex, /* [in] */
        void * info)          /* [out] */
{
    XA_ENTER_INTERFACE

    SL_LOGE("unsupported XAStreamInformationItf function");
    result = XA_RESULT_CONTENT_UNSUPPORTED;

    XA_LEAVE_INTERFACE
}


static XAresult IStreamInformation_QueryStreamName( XAStreamInformationItf self,
        XAuint32 streamIndex, /* [in] */
        XAuint16 * pNameSize, /* [in/out] */
        XAchar * pName)       /* [out] */
{
    XA_ENTER_INTERFACE

    SL_LOGE("unsupported XAStreamInformationItf function");
    result = XA_RESULT_CONTENT_UNSUPPORTED;

    XA_LEAVE_INTERFACE
}


static XAresult IStreamInformation_RegisterStreamChangeCallback( XAStreamInformationItf self,
        xaStreamEventChangeCallback callback, /* [in] */
        void * pContext)                      /* [in] */
{
    XA_ENTER_INTERFACE

    IStreamInformation *thiz = (IStreamInformation *) self;

    interface_lock_exclusive(thiz);

    thiz->mCallback = callback;
    thiz->mContext = pContext;

    switch (InterfaceToObjectID(thiz)) {

    case XA_OBJECTID_MEDIAPLAYER:
        SL_LOGI("IStreamInformation_RegisterStreamChangeCallback()");
        result = SL_RESULT_SUCCESS;
        break;
    default:
        result = SL_RESULT_PARAMETER_INVALID;
        break;
    }

    interface_unlock_exclusive(thiz);

    XA_LEAVE_INTERFACE
}


static XAresult IStreamInformation_QueryActiveStreams( XAStreamInformationItf self,
        XAuint32 *numStreams,      /* [in/out] */
        XAboolean *activeStreams)  /* [out] */
{
    XA_ENTER_INTERFACE

    if (NULL == numStreams) {
        result = XA_RESULT_PARAMETER_INVALID;
        XA_LEAVE_INTERFACE;
    }

    SL_LOGE("unsupported XAStreamInformationItf function");
    result = XA_RESULT_CONTENT_UNSUPPORTED;
    *numStreams = 0;
    activeStreams = NULL;

    XA_LEAVE_INTERFACE
}


static XAresult IStreamInformation_SetActiveStream( XAStreamInformationItf self,
        XAuint32   streamNum, /* [in] */
        XAboolean  active,    /* [in] */
        XAboolean  commitNow) /* [in] */
{
    XA_ENTER_INTERFACE

    SL_LOGE("unsupported XAStreamInformationItf function");
    result = XA_RESULT_FEATURE_UNSUPPORTED;

    XA_LEAVE_INTERFACE
}


static const struct XAStreamInformationItf_ IStreamInformation_Itf = {
    IStreamInformation_QueryMediaContainerInformation,
    IStreamInformation_QueryStreamType,
    IStreamInformation_QueryStreamInformation,
    IStreamInformation_QueryStreamName,
    IStreamInformation_RegisterStreamChangeCallback,
    IStreamInformation_QueryActiveStreams,
    IStreamInformation_SetActiveStream
};

void IStreamInformation_init(void *self)
{
    IStreamInformation *thiz = (IStreamInformation *) self;
    thiz->mItf = &IStreamInformation_Itf;

    thiz->mCallback = NULL;
    thiz->mContext = NULL;

#ifdef ANDROID
    // initialize container info
    StreamInfo contInf;
    contInf.domain = XA_DOMAINTYPE_UNKNOWN; // there should really be a domain for the container!
    contInf.containerInfo.containerType = XA_CONTAINERTYPE_UNSPECIFIED;
    contInf.containerInfo.mediaDuration = XA_TIME_UNKNOWN;
    contInf.containerInfo.numStreams = 0;
    thiz->mStreamInfoTable.add(contInf);
#endif
}
