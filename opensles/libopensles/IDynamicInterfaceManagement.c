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

/* DynamicInterfaceManagement implementation */

#include "sles_allinclusive.h"

static SLresult IDynamicInterfaceManagement_AddInterface(
    SLDynamicInterfaceManagementItf self, const SLInterfaceID iid,
    SLboolean async)
{
    if (NULL == iid)
        return SL_RESULT_PARAMETER_INVALID;
    IDynamicInterfaceManagement *this = (IDynamicInterfaceManagement *) self;
    IObject *thisObject = this->mThis;
    const ClassTable *class__ = thisObject->mClass;
    int MPH, index;
    if ((0 > (MPH = IID_to_MPH(iid))) || (0 > (index = class__->mMPH_to_index[MPH])))
        return SL_RESULT_FEATURE_UNSUPPORTED;
    // FIXME check that interface is dynamic?
    SLresult result;
    VoidHook init = MPH_init_table[MPH].mInit;
    const struct iid_vtable *x = &class__->mInterfaces[index];
    size_t offset = x->mOffset;
    void *thisItf = (char *) thisObject + offset;
    size_t size = ((SLuint32) (index + 1) == class__->mInterfaceCount ?
        class__->mSize : x[1].mOffset) - offset;
    unsigned mask = 1 << index;
    // Lock the object rather than the DIM interface, because
    // we modify both the object (exposed) and interface (added)
    object_lock_exclusive(thisObject);
    if (thisObject->mExposedMask & mask) {
        result = SL_RESULT_PRECONDITIONS_VIOLATED;
    } else {
        // FIXME Currently do initialization here, but might be asynchronous
        memset(thisItf, 0, size);
        ((void **) thisItf)[1] = thisObject;
        if (NULL != init)
            (*init)(thisItf);
        thisObject->mExposedMask |= mask;
        this->mAddedMask |= mask;
        result = SL_RESULT_SUCCESS;
        if (async && (NULL != this->mCallback)) {
            // FIXME Callback runs with mutex locked
            (*this->mCallback)(self, this->mContext,
                SL_DYNAMIC_ITF_EVENT_RESOURCES_AVAILABLE, result, iid);
        }
    }
    object_unlock_exclusive(thisObject);
    return result;
}

static SLresult IDynamicInterfaceManagement_RemoveInterface(
    SLDynamicInterfaceManagementItf self, const SLInterfaceID iid)
{
    if (NULL == iid)
        return SL_RESULT_PARAMETER_INVALID;
    IDynamicInterfaceManagement *this = (IDynamicInterfaceManagement *) self;
    IObject *thisObject = (IObject *) this->mThis;
    const ClassTable *class__ = thisObject->mClass;
    int MPH = IID_to_MPH(iid);
    if (0 > MPH)
        return SL_RESULT_PRECONDITIONS_VIOLATED;
    int index = class__->mMPH_to_index[MPH];
    if (0 > index)
        return SL_RESULT_PRECONDITIONS_VIOLATED;
    SLresult result = SL_RESULT_SUCCESS;
    VoidHook deinit = MPH_init_table[MPH].mDeinit;
    const struct iid_vtable *x = &class__->mInterfaces[index];
    size_t offset = x->mOffset;
    void *thisItf = (char *) thisObject + offset;
    size_t size = ((SLuint32) (index + 1) == class__->mInterfaceCount ?
        class__->mSize : x[1].mOffset) - offset;
    unsigned mask = 1 << index;
    // Lock the object rather than the DIM interface, because
    // we modify both the object (exposed) and interface (added)
    object_lock_exclusive(thisObject);
    // disallow removal of non-dynamic interfaces
    if (!(this->mAddedMask & mask)) {
        result = SL_RESULT_PRECONDITIONS_VIOLATED;
    } else {
        if (NULL != deinit)
            (*deinit)(thisItf);
#ifndef NDEBUG
        memset(thisItf, 0x55, size);
#endif
        thisObject->mExposedMask &= ~mask;
        this->mAddedMask &= ~mask;
    }
    object_unlock_exclusive(thisObject);
    return result;
}

static SLresult IDynamicInterfaceManagement_ResumeInterface(
    SLDynamicInterfaceManagementItf self,
    const SLInterfaceID iid, SLboolean async)
{
    if (NULL == iid)
        return SL_RESULT_PARAMETER_INVALID;
    IDynamicInterfaceManagement *this = (IDynamicInterfaceManagement *) self;
    IObject *thisObject = (IObject *) this->mThis;
    const ClassTable *class__ = thisObject->mClass;
    int MPH = IID_to_MPH(iid);
    if (0 > MPH)
        return SL_RESULT_PRECONDITIONS_VIOLATED;
    int index = class__->mMPH_to_index[MPH];
    if (0 > index)
        return SL_RESULT_PRECONDITIONS_VIOLATED;
    SLresult result = SL_RESULT_SUCCESS;
    unsigned mask = 1 << index;
    // FIXME Change to exclusive when resume hook implemented
    object_lock_shared(thisObject);
    if (!(this->mAddedMask & mask))
        result = SL_RESULT_PRECONDITIONS_VIOLATED;
    // FIXME Call a resume hook on the interface, if suspended
    object_unlock_shared(thisObject);
    return result;
}

static SLresult IDynamicInterfaceManagement_RegisterCallback(
    SLDynamicInterfaceManagementItf self,
    slDynamicInterfaceManagementCallback callback, void *pContext)
{
    IDynamicInterfaceManagement *this = (IDynamicInterfaceManagement *) self;
    IObject *thisObject = this->mThis;
    // FIXME This could be a poke lock, if we had atomic double-word load/store
    object_lock_exclusive(thisObject);
    this->mCallback = callback;
    this->mContext = pContext;
    object_unlock_exclusive(thisObject);
    return SL_RESULT_SUCCESS;
}

static const struct SLDynamicInterfaceManagementItf_ IDynamicInterfaceManagement_Itf = {
    IDynamicInterfaceManagement_AddInterface,
    IDynamicInterfaceManagement_RemoveInterface,
    IDynamicInterfaceManagement_ResumeInterface,
    IDynamicInterfaceManagement_RegisterCallback
};

void IDynamicInterfaceManagement_init(void *self)
{
    IDynamicInterfaceManagement *this = (IDynamicInterfaceManagement *) self;
    this->mItf = &IDynamicInterfaceManagement_Itf;
    this->mAddedMask = 0;
    this->mCallback = NULL;
    this->mContext = NULL;
}
