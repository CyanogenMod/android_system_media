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

/* MetadataTraversal implementation */

#include "sles_allinclusive.h"

static SLresult IMetadataTraversal_SetMode(SLMetadataTraversalItf self, SLuint32 mode)
{
    switch (mode) {
    case SL_METADATATRAVERSALMODE_ALL:
    case SL_METADATATRAVERSALMODE_NODE:
        break;
    default:
        return SL_RESULT_PARAMETER_INVALID;
    }
    IMetadataTraversal *this = (IMetadataTraversal *) self;
    interface_lock_poke(this);
    this->mMode = mode;
    interface_unlock_poke(this);
    return SL_RESULT_SUCCESS;
}

static SLresult IMetadataTraversal_GetChildCount(SLMetadataTraversalItf self, SLuint32 *pCount)
{
    if (NULL == pCount)
        return SL_RESULT_PARAMETER_INVALID;
    IMetadataTraversal *this = (IMetadataTraversal *) self;
    interface_lock_peek(this);
    SLuint32 count = this->mCount;
    interface_unlock_peek(this);
    *pCount = count;
    return SL_RESULT_SUCCESS;
}

static SLresult IMetadataTraversal_GetChildMIMETypeSize(
    SLMetadataTraversalItf self, SLuint32 index, SLuint32 *pSize)
{
    if (NULL == pSize)
        return SL_RESULT_PARAMETER_INVALID;
    IMetadataTraversal *this = (IMetadataTraversal *) self;
    interface_lock_peek(this);
    SLuint32 size = this->mSize;
    interface_unlock_peek(this);
    *pSize = size;
    return SL_RESULT_SUCCESS;
}

static SLresult IMetadataTraversal_GetChildInfo(SLMetadataTraversalItf self, SLuint32 index,
    SLint32 *pNodeID, SLuint32 *pType, SLuint32 size, SLchar *pMimeType)
{
    //IMetadataTraversal *this = (IMetadataTraversal *) self;
    // FIXME not implemented
    return SL_RESULT_FEATURE_UNSUPPORTED;
}

static SLresult IMetadataTraversal_SetActiveNode(SLMetadataTraversalItf self, SLuint32 index)
{
    if (SL_NODE_PARENT == index) {
        ;
    }
    IMetadataTraversal *this = (IMetadataTraversal *) self;
    this->mIndex = index;
    // FIXME not implemented
    return SL_RESULT_FEATURE_UNSUPPORTED;
}

static const struct SLMetadataTraversalItf_ IMetadataTraversal_Itf = {
    IMetadataTraversal_SetMode,
    IMetadataTraversal_GetChildCount,
    IMetadataTraversal_GetChildMIMETypeSize,
    IMetadataTraversal_GetChildInfo,
    IMetadataTraversal_SetActiveNode
};

void IMetadataTraversal_init(void *self)
{
    IMetadataTraversal *this = (IMetadataTraversal *) self;
    this->mItf = &IMetadataTraversal_Itf;
    this->mIndex = 0;
    this->mMode = SL_METADATATRAVERSALMODE_NODE;
    this->mCount = 0;
    this->mSize = 0;
}
