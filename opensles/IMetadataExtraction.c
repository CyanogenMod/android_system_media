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

/* MetadataExtraction implementation */

#include "sles_allinclusive.h"

static SLresult IMetadataExtraction_GetItemCount(SLMetadataExtractionItf self,
    SLuint32 *pItemCount)
{
    //IMetadataExtraction *this = (IMetadataExtraction *) self;
    if (NULL == pItemCount)
        return SL_RESULT_PARAMETER_INVALID;
    *pItemCount = 0;
    return SL_RESULT_SUCCESS;
}

static SLresult IMetadataExtraction_GetKeySize(SLMetadataExtractionItf self,
    SLuint32 index, SLuint32 *pKeySize)
{
    //IMetadataExtraction *this = (IMetadataExtraction *) self;
    if (NULL == pKeySize)
        return SL_RESULT_PARAMETER_INVALID;
    *pKeySize = 0;
    return SL_RESULT_SUCCESS;
}

static SLresult IMetadataExtraction_GetKey(SLMetadataExtractionItf self,
    SLuint32 index, SLuint32 keySize, SLMetadataInfo *pKey)
{
    //IMetadataExtraction *this = (IMetadataExtraction *) self;
    if (NULL == pKey)
        return SL_RESULT_PARAMETER_INVALID;
    SLMetadataInfo key;
    key.size = 1;
    key.encoding = SL_CHARACTERENCODING_UTF8;
    strcpy((char *) key.langCountry, "en");
    key.data[0] = 0;
    *pKey = key;
    return SL_RESULT_SUCCESS;
}

static SLresult IMetadataExtraction_GetValueSize(SLMetadataExtractionItf self,
    SLuint32 index, SLuint32 *pValueSize)
{
    //IMetadataExtraction *this = (IMetadataExtraction *) self;
    if (NULL == pValueSize)
        return SL_RESULT_PARAMETER_INVALID;
    *pValueSize = 0;
    return SL_RESULT_SUCCESS;
}

static SLresult IMetadataExtraction_GetValue(SLMetadataExtractionItf self,
    SLuint32 index, SLuint32 valueSize, SLMetadataInfo *pValue)
{
    //IMetadataExtraction *this = (IMetadataExtraction *) self;
    if (NULL == pValue)
        return SL_RESULT_PARAMETER_INVALID;
    SLMetadataInfo value;
    value.size = 1;
    value.encoding = SL_CHARACTERENCODING_UTF8;
    strcpy((char *) value.langCountry, "en");
    value.data[0] = 0;
    *pValue = value;;
    return SL_RESULT_SUCCESS;
}

static SLresult IMetadataExtraction_AddKeyFilter(SLMetadataExtractionItf self,
    SLuint32 keySize, const void *pKey, SLuint32 keyEncoding,
    const SLchar *pValueLangCountry, SLuint32 valueEncoding, SLuint8 filterMask)
{
    if (NULL == pKey || NULL == pValueLangCountry)
        return SL_RESULT_PARAMETER_INVALID;
    if (filterMask & ~(SL_METADATA_FILTER_KEY | SL_METADATA_FILTER_KEY | SL_METADATA_FILTER_KEY))
        return SL_RESULT_PARAMETER_INVALID;
    IMetadataExtraction *this = (IMetadataExtraction *) self;
    interface_lock_exclusive(this);
    this->mKeySize = keySize;
    this->mKey = pKey;
    this->mKeyEncoding = keyEncoding;
    this->mValueLangCountry = pValueLangCountry; // FIXME local copy?
    this->mValueEncoding = valueEncoding;
    this->mFilterMask = filterMask;
    interface_unlock_exclusive(this);
    return SL_RESULT_SUCCESS;
}

static SLresult IMetadataExtraction_ClearKeyFilter(SLMetadataExtractionItf self)
{
    IMetadataExtraction *this = (IMetadataExtraction *) self;
    this->mKeyFilter = 0;
    return SL_RESULT_SUCCESS;
}

static const struct SLMetadataExtractionItf_ IMetadataExtraction_Itf = {
    IMetadataExtraction_GetItemCount,
    IMetadataExtraction_GetKeySize,
    IMetadataExtraction_GetKey,
    IMetadataExtraction_GetValueSize,
    IMetadataExtraction_GetValue,
    IMetadataExtraction_AddKeyFilter,
    IMetadataExtraction_ClearKeyFilter
};

void IMetadataExtraction_init(void *self)
{
    IMetadataExtraction *this = (IMetadataExtraction *) self;
    this->mItf = &IMetadataExtraction_Itf;
    this->mKeySize = 0;
    this->mKey = NULL;
    this->mKeyEncoding = 0 /*TBD*/;
    this->mValueLangCountry = 0 /*TBD*/;
    this->mValueEncoding = 0 /*TBD*/;
    this->mFilterMask = 0 /*TBD*/;
}
