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


// Called by a worker thread to handle an asynchronous AddInterface.
// Parameter self is the DynamicInterface, and MPH specifies which interface to add.

static void HandleAdd(void *self, int MPH)
{

    // validate input parameters
    IDynamicInterfaceManagement *this = (IDynamicInterfaceManagement *) self;
    assert(NULL != this);
    IObject *thisObject = InterfaceToIObject(this);
    assert(NULL != thisObject);
    assert(0 <= MPH && MPH < MPH_MAX);
    const ClassTable *class__ = thisObject->mClass;
    assert(NULL != class__);
    int index = class__->mMPH_to_index[MPH];
    assert(0 <= index && index < (int) class__->mInterfaceCount);
    SLuint8 *interfaceStateP = &thisObject->mInterfaceStates[index];
    SLresult result;

    // check interface state
    object_lock_exclusive(thisObject);
    SLuint8 state = *interfaceStateP;
    switch (state) {

    case INTERFACE_ADDING_1:    // normal case
        {
        // change state to indicate we are now adding the interface
        *interfaceStateP = INTERFACE_ADDING_2;
        object_unlock_exclusive(thisObject);

        // this section runs with mutex unlocked
        const struct iid_vtable *x = &class__->mInterfaces[index];
        size_t offset = x->mOffset;
        void *thisItf = (char *) thisObject + offset;
        size_t size = ((SLuint32) (index + 1) == class__->mInterfaceCount ?
            class__->mSize : x[1].mOffset) - offset;
        memset(thisItf, 0, size);
        // Will never add IObject, so [1] is always defined
        ((void **) thisItf)[1] = thisObject;
        VoidHook init = MPH_init_table[MPH].mInit;
        if (NULL != init)
            (*init)(thisItf);
        result = SL_RESULT_SUCCESS;

        // re-lock mutex to update state
        object_lock_exclusive(thisObject);
        assert(INTERFACE_ADDING_2 == *interfaceStateP);
        state = INTERFACE_ADDED;
        }
        break;

    case INTERFACE_ADDING_1A:   // operation was aborted while on work queue
        result = SL_RESULT_OPERATION_ABORTED;
        state = INTERFACE_UNINITIALIZED;
        break;

    default:                    // impossible
        assert(SL_BOOLEAN_FALSE);
        result = SL_RESULT_INTERNAL_ERROR;
        break;

    }

    // mutex is locked, update state
    *interfaceStateP = state;

    // Make a copy of these, so we can call the callback with mutex unlocked
    slDynamicInterfaceManagementCallback callback = this->mCallback;
    void *context = this->mContext;
    object_unlock_exclusive(thisObject);

    // Note that the mutex is unlocked during the callback
    if (NULL != callback) {
        const SLInterfaceID iid = &SL_IID_array[MPH]; // equal but not == to the original IID
        (*callback)(&this->mItf, context, SL_DYNAMIC_ITF_EVENT_ASYNC_TERMINATION, result, iid);
    }

}


static SLresult IDynamicInterfaceManagement_AddInterface(SLDynamicInterfaceManagementItf self,
    const SLInterfaceID iid, SLboolean async)
{
    SL_ENTER_INTERFACE

    // validate input parameters
    if (NULL == iid) {
        result = SL_RESULT_PARAMETER_INVALID;
    } else {
        IDynamicInterfaceManagement *this = (IDynamicInterfaceManagement *) self;
        IObject *thisObject = InterfaceToIObject(this);
        const ClassTable *class__ = thisObject->mClass;
        int MPH, index;
        if ((0 > (MPH = IID_to_MPH(iid))) || (0 > (index = class__->mMPH_to_index[MPH]))) {
            result = SL_RESULT_FEATURE_UNSUPPORTED;
        } else {
            assert(index < (int) class__->mInterfaceCount);
            SLuint8 *interfaceStateP = &thisObject->mInterfaceStates[index];

            // check interface state
            object_lock_exclusive(thisObject);
            switch (*interfaceStateP) {

            case INTERFACE_UNINITIALIZED:   // normal case
                if (async) {
                    // Asynchronous: mark operation pending and cancellable
                    *interfaceStateP = INTERFACE_ADDING_1;
                    object_unlock_exclusive(thisObject);

                    // this section runs with mutex unlocked
                    result = ThreadPool_add(&thisObject->mEngine->mThreadPool, HandleAdd, this,
                        MPH);
                    if (SL_RESULT_SUCCESS != result) {
                        // Engine was destroyed during add, or insufficient memory,
                        // so restore mInterfaceStates state to prior value
                        object_lock_exclusive(thisObject);
                        switch (*interfaceStateP) {
                        case INTERFACE_ADDING_1:    // normal
                        case INTERFACE_ADDING_1A:   // operation aborted while mutex unlocked
                            *interfaceStateP = INTERFACE_UNINITIALIZED;
                            break;
                        default:                    // unexpected
                            // leave state alone
                            break;
                        }
                    }

                } else {
                    // Synchronous: mark operation pending to prevent duplication
                    *interfaceStateP = INTERFACE_ADDING_2;
                    object_unlock_exclusive(thisObject);

                    // this section runs with mutex unlocked
                    const struct iid_vtable *x = &class__->mInterfaces[index];
                    size_t offset = x->mOffset;
                    size_t size = ((SLuint32) (index + 1) == class__->mInterfaceCount ?
                        class__->mSize : x[1].mOffset) - offset;
                    void *thisItf = (char *) thisObject + offset;
                    memset(thisItf, 0, size);
                    // Will never add IObject, so [1] is always defined
                    ((void **) thisItf)[1] = thisObject;
                    VoidHook init = MPH_init_table[MPH].mInit;
                    if (NULL != init)
                        (*init)(thisItf);
                    result = SL_RESULT_SUCCESS;

                    // re-lock mutex to update state
                    object_lock_exclusive(thisObject);
                    assert(INTERFACE_ADDING_2 == *interfaceStateP);
                    *interfaceStateP = INTERFACE_ADDED;
                }

                // mutex is still locked
                break;

            default:    // disallow adding of (partially) initialized interfaces
                result = SL_RESULT_PRECONDITIONS_VIOLATED;
                break;

            }

            object_unlock_exclusive(thisObject);

        }
    }

    SL_LEAVE_INTERFACE
}


static SLresult IDynamicInterfaceManagement_RemoveInterface(
    SLDynamicInterfaceManagementItf self, const SLInterfaceID iid)
{
    SL_ENTER_INTERFACE

    // validate input parameters
    if (NULL == iid) {
        result = SL_RESULT_PARAMETER_INVALID;
    } else {
        IDynamicInterfaceManagement *this = (IDynamicInterfaceManagement *) self;
        IObject *thisObject = InterfaceToIObject(this);
        const ClassTable *class__ = thisObject->mClass;
        int MPH, index;
        if ((0 > (MPH = IID_to_MPH(iid))) || (0 > (index = class__->mMPH_to_index[MPH]))) {
            result = SL_RESULT_PRECONDITIONS_VIOLATED;
        } else {
            SLuint8 *interfaceStateP = &thisObject->mInterfaceStates[index];

            // check interface state
            object_lock_exclusive(thisObject);
            switch (*interfaceStateP) {

            case INTERFACE_ADDED:       // normal cases
            case INTERFACE_SUSPENDED:
                {
                // Mark operation pending to prevent duplication
                *interfaceStateP = INTERFACE_REMOVING;
                thisObject->mGottenMask &= ~(1 << index);
                object_unlock_exclusive(thisObject);

                // The deinitialization is done with mutex unlocked
                const struct iid_vtable *x = &class__->mInterfaces[index];
                size_t offset = x->mOffset;
                void *thisItf = (char *) thisObject + offset;
                VoidHook deinit = MPH_init_table[MPH].mDeinit;
                if (NULL != deinit)
                    (*deinit)(thisItf);
#ifndef NDEBUG
                size_t size = ((SLuint32) (index + 1) == class__->mInterfaceCount ?
                    class__->mSize : x[1].mOffset) - offset;
                memset(thisItf, 0x55, size);
#endif
                result = SL_RESULT_SUCCESS;

                // Note that this was previously locked, but then unlocked for the deinit hook
                object_lock_exclusive(thisObject);
                assert(INTERFACE_REMOVING == *interfaceStateP);
                *interfaceStateP = INTERFACE_UNINITIALIZED;
                }

                // mutex is still locked
                break;

            default:
                // disallow removal of non-dynamic interfaces, or interfaces which are
                // currently being resumed (will not auto-cancel an asynchronous resume)
                result = SL_RESULT_PRECONDITIONS_VIOLATED;
                break;

            }

            object_unlock_exclusive(thisObject);
        }
    }

    SL_LEAVE_INTERFACE
}


// Called by a worker thread to handle an asynchronous ResumeInterface.
// Parameter self is the DynamicInterface, and MPH specifies which interface to resume.

static void HandleResume(void *self, int MPH)
{

    // validate input parameters
    IDynamicInterfaceManagement *this = (IDynamicInterfaceManagement *) self;
    assert(NULL != this);
    IObject *thisObject = InterfaceToIObject(this);
    assert(NULL != thisObject);
    assert(0 <= MPH && MPH < MPH_MAX);
    const ClassTable *class__ = thisObject->mClass;
    assert(NULL != class__);
    int index = class__->mMPH_to_index[MPH];
    assert(0 <= index && index < (int) class__->mInterfaceCount);
    SLuint8 *interfaceStateP = &thisObject->mInterfaceStates[index];
    SLresult result;

    // check interface state
    object_lock_exclusive(thisObject);
    SLuint8 state = *interfaceStateP;
    switch (state) {

    case INTERFACE_RESUMING_1:      // normal case
        {
        // change state to indicate we are now resuming the interface
        *interfaceStateP = INTERFACE_RESUMING_2;
        object_unlock_exclusive(thisObject);

        // this section runs with mutex unlocked
        const struct iid_vtable *x = &class__->mInterfaces[index];
        size_t offset = x->mOffset;
        void *thisItf = (char *) thisObject + offset;
        VoidHook resume = MPH_init_table[MPH].mResume;
        if (NULL != resume)
            (*resume)(thisItf);
        result = SL_RESULT_SUCCESS;

        // re-lock mutex to update state
        object_lock_exclusive(thisObject);
        assert(INTERFACE_RESUMING_2 == *interfaceStateP);
        state = INTERFACE_ADDED;
        }
        break;

    case INTERFACE_RESUMING_1A:     // operation was aborted while on work queue
        result = SL_RESULT_OPERATION_ABORTED;
        state = INTERFACE_SUSPENDED;
        break;

    default:                        // impossible
        assert(SL_BOOLEAN_FALSE);
        result = SL_RESULT_INTERNAL_ERROR;
        break;

    }

    // mutex is locked, update state
    *interfaceStateP = state;

    // Make a copy of these, so we can call the callback with mutex unlocked
    slDynamicInterfaceManagementCallback callback = this->mCallback;
    void *context = this->mContext;
    object_unlock_exclusive(thisObject);

    // Note that the mutex is unlocked during the callback
    if (NULL != callback) {
        const SLInterfaceID iid = &SL_IID_array[MPH]; // equal but not == to the original IID
        (*callback)(&this->mItf, context, SL_DYNAMIC_ITF_EVENT_ASYNC_TERMINATION, result, iid);
    }
}


static SLresult IDynamicInterfaceManagement_ResumeInterface(SLDynamicInterfaceManagementItf self,
    const SLInterfaceID iid, SLboolean async)
{
    SL_ENTER_INTERFACE

    // validate input parameters
    if (NULL == iid) {
        result = SL_RESULT_PARAMETER_INVALID;
    } else {
        IDynamicInterfaceManagement *this = (IDynamicInterfaceManagement *) self;
        IObject *thisObject = InterfaceToIObject(this);
        const ClassTable *class__ = thisObject->mClass;
        int MPH, index;
        if ((0 > (MPH = IID_to_MPH(iid))) || (0 > (index = class__->mMPH_to_index[MPH]))) {
            result = SL_RESULT_PRECONDITIONS_VIOLATED;
        } else {
            assert(index < (int) class__->mInterfaceCount);
            SLuint8 *interfaceStateP = &thisObject->mInterfaceStates[index];

            // check interface state
            object_lock_exclusive(thisObject);
            switch (*interfaceStateP) {

            case INTERFACE_SUSPENDED:   // normal case
                if (async) {
                    // Asynchronous: mark operation pending and cancellable
                    *interfaceStateP = INTERFACE_RESUMING_1;
                    object_unlock_exclusive(thisObject);

                    // this section runs with mutex unlocked
                    result = ThreadPool_add(&thisObject->mEngine->mThreadPool, HandleResume, this,
                        MPH);
                    if (SL_RESULT_SUCCESS != result) {
                        // Engine was destroyed during resume, or insufficient memory,
                        // so restore mInterfaceStates state to prior value
                        object_lock_exclusive(thisObject);
                        switch (*interfaceStateP) {
                        case INTERFACE_RESUMING_1:  // normal
                        case INTERFACE_RESUMING_1A: // operation aborted while mutex unlocked
                            *interfaceStateP = INTERFACE_SUSPENDED;
                            break;
                        default:                    // unexpected
                            // leave state alone
                            break;
                        }
                    }

                } else {
                    // Synchronous: mark operation pending to prevent duplication
                    *interfaceStateP = INTERFACE_RESUMING_2;
                    object_unlock_exclusive(thisObject);

                    // this section runs with mutex unlocked
                    const struct iid_vtable *x = &class__->mInterfaces[index];
                    size_t offset = x->mOffset;
                    void *thisItf = (char *) this + offset;
                    VoidHook resume = MPH_init_table[MPH].mResume;
                    if (NULL != resume)
                        (*resume)(thisItf);
                    result = SL_RESULT_SUCCESS;

                    // re-lock mutex to update state
                    object_lock_exclusive(thisObject);
                    assert(INTERFACE_RESUMING_2 == *interfaceStateP);
                    *interfaceStateP = INTERFACE_ADDED;
                }

                // mutex is now unlocked
                break;

            default:    // disallow resumption of non-suspended interfaces
                object_unlock_exclusive(thisObject);
                result = SL_RESULT_PRECONDITIONS_VIOLATED;
                break;
            }

            object_unlock_exclusive(thisObject);
        }
    }

    SL_LEAVE_INTERFACE
}


static SLresult IDynamicInterfaceManagement_RegisterCallback(SLDynamicInterfaceManagementItf self,
    slDynamicInterfaceManagementCallback callback, void *pContext)
{
    SL_ENTER_INTERFACE

    IDynamicInterfaceManagement *this = (IDynamicInterfaceManagement *) self;
    IObject *thisObject = InterfaceToIObject(this);
    object_lock_exclusive(thisObject);
    this->mCallback = callback;
    this->mContext = pContext;
    object_unlock_exclusive(thisObject);
    result = SL_RESULT_SUCCESS;

    SL_LEAVE_INTERFACE
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
    this->mCallback = NULL;
    this->mContext = NULL;
}
