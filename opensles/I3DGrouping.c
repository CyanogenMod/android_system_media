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

/* 3DGrouping implementation */

#include "sles_allinclusive.h"

static SLresult I3DGrouping_Set3DGroup(SL3DGroupingItf self, SLObjectItf group)
{
    I3DGrouping *this = (I3DGrouping *) self;
    if (NULL == group)
        return SL_RESULT_PARAMETER_INVALID;
    IObject *thisGroup = (IObject *) group;
    if (&C3DGroup_class != thisGroup->mClass)
        return SL_RESULT_PARAMETER_INVALID;
    // FIXME race possible if group unrealized immediately after, should lock
    if (SL_OBJECT_STATE_REALIZED != thisGroup->mState)
        return SL_RESULT_PRECONDITIONS_VIOLATED;
    interface_lock_exclusive(this);
    this->mGroup = group;
    // FIXME add this object to the group's bag of objects
    interface_unlock_exclusive(this);
    return SL_RESULT_SUCCESS;
}

static SLresult I3DGrouping_Get3DGroup(SL3DGroupingItf self,
    SLObjectItf *pGroup)
{
    if (NULL == pGroup)
        return SL_RESULT_PARAMETER_INVALID;
    I3DGrouping *this = (I3DGrouping *) self;
    interface_lock_peek(this);
    *pGroup = this->mGroup;
    interface_unlock_peek(this);
    return SL_RESULT_SUCCESS;
}

static const struct SL3DGroupingItf_ I3DGrouping_Itf = {
    I3DGrouping_Set3DGroup,
    I3DGrouping_Get3DGroup
};

void I3DGrouping_init(void *self)
{
    I3DGrouping *this = (I3DGrouping *) self;
    this->mItf = &I3DGrouping_Itf;
#ifndef NDEBUG
    this->mGroup = NULL;
#endif
    // FIXME initialize the bag here
}
