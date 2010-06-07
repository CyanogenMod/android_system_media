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

static SLresult IDynamicInterfaceManagement_AddInterface(SLDynamicInterfaceManagementItf self,
    const SLInterfaceID iid, SLboolean async)
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
    slDynamicInterfaceManagementCallback callback = NULL;
    void *context = NULL;
    // Lock the object rather than the DIM interface, because
    // we modify both the object (exposed) and interface (added)
    object_lock_exclusive(thisObject);
    if ((thisObject->mExposedMask | this->mAddingMask) & mask) {
        result = SL_RESULT_PRECONDITIONS_VIOLATED;
    } else {
        // Mark operation pending to prevent duplication
        this->mAddingMask |= mask;
        object_unlock_exclusive(thisObject);
        // FIXME Currently all initialization is done here, even if requested to be asynchronous
        memset(thisItf, 0, size);
        ((void **) thisItf)[1] = thisObject;
        if (NULL != init)
            (*init)(thisItf);
        // Note that this was previously locked, but then unlocked for the init hook
        object_lock_exclusive(thisObject);
        assert(this->mAddingMask & mask);
        this->mAddingMask &= ~mask;
        assert(!(thisObject->mExposedMask & mask));
        thisObject->mExposedMask |= mask;
        assert(!(this->mAddedMask & mask));
        this->mAddedMask |= mask;
        assert(!(this->mSuspendedMask & mask));
        result = SL_RESULT_SUCCESS;
        // Make a copy of these, so we can call the callback with mutex unlocked
        if (async) {
            callback = this->mCallback;
            context = this->mContext;
        }
    }
    object_unlock_exclusive(thisObject);
    if (NULL != callback)
        (*callback)(self, context, SL_DYNAMIC_ITF_EVENT_ASYNC_TERMINATION, result, iid);
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
    if (!(this->mAddedMask & mask) || (this->mRemovingMask & mask)) {
        result = SL_RESULT_PRECONDITIONS_VIOLATED;
    } else {
        this->mRemovingMask |= mask;
        object_unlock_exclusive(thisObject);
        // FIXME When async resume is implemented, a pending async resume should be cancelled
        // The deinitialization is done with mutex unlocked
        if (NULL != deinit)
            (*deinit)(thisItf);
#ifndef NDEBUG
        memset(thisItf, 0x55, size);
#endif
        // Note that this was previously locked, but then unlocked for the deinit hook
        object_lock_exclusive(thisObject);
        assert(this->mRemovingMask & mask);
        this->mRemovingMask &= ~mask;
        assert(thisObject->mExposedMask & mask);
        thisObject->mExposedMask &= ~mask;
        assert(this->mAddedMask & mask);
        this->mAddedMask &= ~mask;
        this->mSuspendedMask &= ~mask;
    }
    object_unlock_exclusive(thisObject);
    return result;
}

static SLresult IDynamicInterfaceManagement_ResumeInterface(SLDynamicInterfaceManagementItf self,
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
    SLresult result;
    unsigned mask = 1 << index;
    slDynamicInterfaceManagementCallback callback = NULL;
    void *context = NULL;
    // FIXME Change to exclusive when resume hook implemented
    object_lock_shared(thisObject);
    if (!(this->mSuspendedMask & mask))
        result = SL_RESULT_PRECONDITIONS_VIOLATED;
    else {
        assert(this->mAddedMask & mask);
        assert(thisObject->mExposedMask & mask);
        // FIXME Currently the resume is done here, even if requested to be asynchronous
        // FIXME For asynchronous, mark operation pending to prevent duplication
        this->mSuspendedMask &= ~mask;
        result = SL_RESULT_SUCCESS;
        // Make a copy of these, so we can call the callback with mutex unlocked
        if (async) {
            callback = this->mCallback;
            context = this->mContext;
        }
    }
    // FIXME Call a resume hook on the interface, if suspended
    object_unlock_shared(thisObject);
    if (NULL != callback)
        (*callback)(self, context, SL_DYNAMIC_ITF_EVENT_ASYNC_TERMINATION, result, iid);
    return result;
}

static SLresult IDynamicInterfaceManagement_RegisterCallback(SLDynamicInterfaceManagementItf self,
    slDynamicInterfaceManagementCallback callback, void *pContext)
{
    IDynamicInterfaceManagement *this = (IDynamicInterfaceManagement *) self;
    IObject *thisObject = this->mThis;
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
    this->mAddingMask = 0;
    this->mRemovingMask = 0;
    this->mSuspendedMask = 0;
    this->mCallback = NULL;
    this->mContext = NULL;
}
