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

/* Visualization implementation */

#include "sles_allinclusive.h"

static SLresult IVisualization_RegisterVisualizationCallback(SLVisualizationItf self,
    slVisualizationCallback callback, void *pContext, SLmilliHertz rate)
{
    if (!(0 < rate && rate <= 20000))
        return SL_RESULT_PARAMETER_INVALID;
    IVisualization *this = (IVisualization *) self;
    interface_lock_exclusive(this);
    this->mCallback = callback;
    this->mContext = pContext;
    this->mRate = rate;
    interface_unlock_exclusive(this);
    return SL_RESULT_SUCCESS;
}

static SLresult IVisualization_GetMaxRate(SLVisualizationItf self,
    SLmilliHertz *pRate)
{
    if (NULL == pRate)
        return SL_RESULT_PARAMETER_INVALID;
    *pRate = 20000;
    return SL_RESULT_SUCCESS;
}

static const struct SLVisualizationItf_ IVisualization_Itf = {
    IVisualization_RegisterVisualizationCallback,
    IVisualization_GetMaxRate
};

void IVisualization_init(void *self)
{
    IVisualization *this = (IVisualization *) self;
    this->mItf = &IVisualization_Itf;
    this->mCallback = NULL;
    this->mContext = NULL;
    this->mRate = 20000;
}
