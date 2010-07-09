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
    SL_ENTER_INTERFACE

    // validate input parameters
    C3DGroup *newGroup = (C3DGroup *) group;
    result = SL_RESULT_SUCCESS;
    if (NULL != newGroup) {
        if (SL_OBJECTID_3DGROUP != IObjectToObjectID(&newGroup->mObject))
            result = SL_RESULT_PARAMETER_INVALID;
        // FIXME race possible if new group unrealized immediately after check, and also
        //       missing locks later on during the updates to old and new group member masks
        else if (SL_OBJECT_STATE_REALIZED != newGroup->mObject.mState)
            result = SL_RESULT_PRECONDITIONS_VIOLATED;
    }
    if (SL_RESULT_SUCCESS == result) {
        I3DGrouping *this = (I3DGrouping *) self;
        unsigned mask = 1 << (InterfaceToIObject(this)->mInstanceID - 1);
        interface_lock_exclusive(this);
        C3DGroup *oldGroup = this->mGroup;
        if (newGroup != oldGroup) {
            // remove this object from the old group's set of objects
            if (NULL != oldGroup) {
                assert(oldGroup->mMemberMask & mask);
                oldGroup->mMemberMask &= ~mask;
            }
            // add this object to the new group's set of objects
            if (NULL != newGroup) {
                assert(!(newGroup->mMemberMask & mask));
                newGroup->mMemberMask |= mask;
            }
            this->mGroup = newGroup;
        }
        interface_unlock_exclusive(this);
    }

    SL_LEAVE_INTERFACE
}


static SLresult I3DGrouping_Get3DGroup(SL3DGroupingItf self, SLObjectItf *pGroup)
{
    SL_ENTER_INTERFACE

    if (NULL == pGroup) {
        result = SL_RESULT_PARAMETER_INVALID;
    } else {
        I3DGrouping *this = (I3DGrouping *) self;
        interface_lock_peek(this);
        C3DGroup *group = this->mGroup;
        *pGroup = (NULL != group) ? &group->mObject.mItf : NULL;
        interface_unlock_peek(this);
        result = SL_RESULT_SUCCESS;
    }

    SL_LEAVE_INTERFACE
}


static const struct SL3DGroupingItf_ I3DGrouping_Itf = {
    I3DGrouping_Set3DGroup,
    I3DGrouping_Get3DGroup
};

void I3DGrouping_init(void *self)
{
    I3DGrouping *this = (I3DGrouping *) self;
    this->mItf = &I3DGrouping_Itf;
    this->mGroup = NULL;
    // initialize the set here
}
