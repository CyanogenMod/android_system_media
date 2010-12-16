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

#ifndef OPENMAX_AL_ANDROID_H_
#define OPENMAX_AL_ANDROID_H_

#ifdef __cplusplus
extern "C" {
#endif

/*---------------------------------------------------------------------------*/
/* Android common types                                                      */
/*---------------------------------------------------------------------------*/

typedef XAuint32               XAAbufferQueueEvent;


/*---------------------------------------------------------------------------*/
/* Android Buffer Qeueue Interface                                           */
/*---------------------------------------------------------------------------*/

extern XA_API const XAInterfaceID XA_IID_ANDROIDBUFFERQUEUE;

struct XAAndroidBufferQueueItf_;
typedef const struct XAAndroidBufferQueueItf_ * const * XAAndroidBufferQueueItf;

#define XA_ANDROIDBUFFERQUEUE_EVENT_NONE              ((XAuint32) 0x00000000)
#define XA_ANDROIDBUFFERQUEUE_EVENT_EOS               ((XAuint32) 0x00000001)
#define XA_ANDROIDBUFFERQUEUE_EVENT_DISCONTINUITY     ((XAuint32) 0x00000002)

typedef XAresult (XAAPIENTRY *xaAndroidBufferQueueCallback)(
    XAAndroidBufferQueueItf caller,/* input */
    void *pContext,                /* input */
    XAuint32 bufferId,             /* input */
    XAuint32 bufferLength,         /* input */
    void *pBufferDataLocation      /* input */
);

struct XAAndroidBufferQueueItf_ {
    XAresult (*RegisterCallback) (
        XAAndroidBufferQueueItf self,
        xaAndroidBufferQueueCallback callback,
        void* pContext
    );

    XAresult (*Clear) (
        XAAndroidBufferQueueItf self
    );

    XAresult (*Enqueue) (
        XAAndroidBufferQueueItf self,
        XAuint32 bufferId,
        XAuint32 length,
        XAAbufferQueueEvent event,
        void *pData // FIXME ignored for now, subject to change
    );
};


/*---------------------------------------------------------------------------*/
/* Android Buffer Queue Data Locator                                         */
/*---------------------------------------------------------------------------*/

/** Addendum to Data locator macros  */
#define XA_DATALOCATOR_ANDROIDBUFFERQUEUE       ((XAuint32) 0x800007BE)

/** Android Buffer Queue-based data locator definition,
 *  locatorType must be XA_DATALOCATOR_ANDROIDBUFFERQUEUE */
typedef struct XADataLocator_AndroidBufferQueue_ {
    XAuint32    locatorType;
    XAuint32    numBuffers;  // FIXME ignored for now, subject to change
    XAuint32    queueSize;   // FIXME ignored for now, subject to change
} XADataLocator_AndroidBufferQueue;

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* OPENMAX_AL_ANDROID_H_ */
