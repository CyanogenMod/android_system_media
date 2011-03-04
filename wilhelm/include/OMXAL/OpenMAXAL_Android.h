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

typedef xa_int64_t             XAAint64;           /* 64 bit signed integer */

typedef XAuint32               XAAbufferQueueEvent;


/*---------------------------------------------------------------------------*/
/* Android Buffer Queue Interface                                            */
/*---------------------------------------------------------------------------*/

extern XA_API const XAInterfaceID XA_IID_ANDROIDBUFFERQUEUE;

struct XAAndroidBufferQueueItf_;
typedef const struct XAAndroidBufferQueueItf_ * const * XAAndroidBufferQueueItf;

#define XA_ANDROID_ITEMKEY_NONE          ((XAuint32) 0x00000000)
#define XA_ANDROID_ITEMKEY_EOS           ((XAuint32) 0x00000001)
#define XA_ANDROID_ITEMKEY_DISCONTINUITY ((XAuint32) 0x00000002)

typedef struct XAAndroidBufferItem_ {
    XAuint32 itemKey;  // identifies the item
    XAuint32 itemSize;
    XAuint8  itemData[0];
} XAAndroidBufferItem;

typedef XAresult (XAAPIENTRY *xaAndroidBufferQueueCallback)(
    XAAndroidBufferQueueItf caller,/* input */
    void *pContext,                /* input */
    const void *pBufferData,       /* input */
    XAuint32 dataSize,             /* input */
    XAuint32 dataUsed,             /* input */
    const XAAndroidBufferItem *pItems,/* input */
    XAuint32 itemsLength           /* input */
);

typedef struct XAAndroidBufferQueueState_ {
    XAuint32    count;
    XAuint32    index;
} XAAndroidBufferQueueState;

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
        const void *pData,
        XAuint32 dataLength,
        const XAAndroidBufferItem *pItems,
        XAuint32 itemsLength
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
    XAuint32    numBuffers;
} XADataLocator_AndroidBufferQueue;


/*---------------------------------------------------------------------------*/
/* Android File Descriptor Data Locator                                      */
/*---------------------------------------------------------------------------*/

/** Addendum to Data locator macros  */
#define XA_DATALOCATOR_ANDROIDFD                ((SLuint32) 0x800007BC)

#define XA_DATALOCATOR_ANDROIDFD_USE_FILE_SIZE ((SLAint64) 0xFFFFFFFFFFFFFFFFll)

/** File Descriptor-based data locator definition, locatorType must be XA_DATALOCATOR_ANDROIDFD */
typedef struct XADataLocator_AndroidFD_ {
    XAuint32        locatorType;
    XAint32         fd;
    XAAint64        offset;
    XAAint64        length;
} XADataLocator_AndroidFD;

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* OPENMAX_AL_ANDROID_H_ */
