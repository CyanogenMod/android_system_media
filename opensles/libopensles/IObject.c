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

/* Object implementation */

#include "sles_allinclusive.h"

static SLresult IObject_Realize(SLObjectItf self, SLboolean async)
{
    IObject *this = (IObject *) self;
    const ClassTable *class__ = this->mClass;
    AsyncHook realize = class__->mRealize;
    SLresult result;
    slObjectCallback callback = NULL;
    void *context = NULL;
    SLuint32 state = 0;
    object_lock_exclusive(this);
    if (SL_OBJECT_STATE_UNREALIZED != this->mState) {
        result = SL_RESULT_PRECONDITIONS_VIOLATED;
    } else {
        // FIXME The realize hook and callback should be asynchronous if requested
        // FIXME For asynchronous, mark operation pending to prevent duplication
        result = NULL != realize ? (*realize)(this, async) : SL_RESULT_SUCCESS;
        if (SL_RESULT_SUCCESS == result)
            this->mState = SL_OBJECT_STATE_REALIZED;
        // Make a copy of these, so we can call the callback with mutex unlocked
        if (async) {
            callback = this->mCallback;
            context = this->mContext;
            state = this->mState;
        }
    }
    object_unlock_exclusive(this);
    if (NULL != callback)
        (*callback)(self, context, SL_OBJECT_EVENT_ASYNC_TERMINATION, result, state, NULL);
    return result;
}

static SLresult IObject_Resume(SLObjectItf self, SLboolean async)
{
    IObject *this = (IObject *) self;
    const ClassTable *class__ = this->mClass;
    StatusHook resume = class__->mResume;
    SLresult result;
    slObjectCallback callback = NULL;
    void *context = NULL;
    SLuint32 state = 0;
    object_lock_exclusive(this);
    if (SL_OBJECT_STATE_SUSPENDED != this->mState) {
        result = SL_RESULT_PRECONDITIONS_VIOLATED;
    } else {
        // FIXME The resume hook and callback should be asynchronous if requested
        // FIXME For asynchronous, mark operation pending to prevent duplication
        result = NULL != resume ? (*resume)(this) : SL_RESULT_SUCCESS;
        if (SL_RESULT_SUCCESS == result)
            this->mState = SL_OBJECT_STATE_REALIZED;
        // Make a copy of these, so we can call the callback with mutex unlocked
        if (async) {
            callback = this->mCallback;
            context = this->mContext;
            state = this->mState;
        }
    }
    object_unlock_exclusive(this);
    if (NULL != callback)
        (*callback)(self, context, SL_OBJECT_EVENT_ASYNC_TERMINATION, result, state, NULL);
    return result;
}

static SLresult IObject_GetState(SLObjectItf self, SLuint32 *pState)
{
    if (NULL == pState)
        return SL_RESULT_PARAMETER_INVALID;
    IObject *this = (IObject *) self;
    // Note that the state is immediately obsolete, so a peek lock is safe
    object_lock_peek(this);
    SLuint32 state = this->mState;
    object_unlock_peek(this);
    *pState = state;
    return SL_RESULT_SUCCESS;
}

static SLresult IObject_GetInterface(SLObjectItf self, const SLInterfaceID iid, void *pInterface)
{
    if (NULL == pInterface)
        return SL_RESULT_PARAMETER_INVALID;
    SLresult result;
    void *interface = NULL;
    if (NULL == iid)
        result = SL_RESULT_PARAMETER_INVALID;
    else {
        IObject *this = (IObject *) self;
        const ClassTable *class__ = this->mClass;
        int MPH, index;
        if ((0 > (MPH = IID_to_MPH(iid))) ||
            (0 > (index = class__->mMPH_to_index[MPH])))
            result = SL_RESULT_FEATURE_UNSUPPORTED;
        else {
            unsigned mask = 1 << index;
            object_lock_shared(this);
            if (SL_OBJECT_STATE_REALIZED != this->mState)
                result = SL_RESULT_PRECONDITIONS_VIOLATED;
            else if (!(this->mExposedMask & mask))
                result = SL_RESULT_FEATURE_UNSUPPORTED;
            else {
                // FIXME Should note that interface has been gotten,
                // so as to detect use of ill-gotten interfaces; be sure
                // to change the lock to exclusive if that is done
                interface = (char *) this + class__->mInterfaces[index].mOffset;
                result = SL_RESULT_SUCCESS;
            }
            object_unlock_shared(this);
        }
    }
    *(void **)pInterface = interface;
    return SL_RESULT_SUCCESS;
}

static SLresult IObject_RegisterCallback(SLObjectItf self,
    slObjectCallback callback, void *pContext)
{
    IObject *this = (IObject *) self;
    object_lock_exclusive(this);
    this->mCallback = callback;
    this->mContext = pContext;
    object_unlock_exclusive(this);
    return SL_RESULT_SUCCESS;
}

static void IObject_AbortAsyncOperation(SLObjectItf self)
{
    // FIXME Asynchronous operations are not yet implemented
}

static void IObject_Destroy(SLObjectItf self)
{
    // FIXME Check for dependencies, e.g. destroying an output mix with attached players,
    //       destroying an engine with extant objects, etc.
    // FIXME The abort should be atomic w.r.t. destroy, so another async can't be started in window
    // FIXME Destroy may need to be made asynchronous to permit safe cleanup of resources
    // FIXME For asynchronous, mark operation pending to prevent duplication
    IObject_AbortAsyncOperation(self);
    IObject *this = (IObject *) self;
    const ClassTable *class__ = this->mClass;
    VoidHook destroy = class__->mDestroy;
    const struct iid_vtable *x = class__->mInterfaces;
    // const, no lock needed
    IEngine *thisEngine = this->mEngine;
    unsigned i = this->mInstanceID;
    assert(0 < i && i <= INSTANCE_MAX);
    --i;
    // remove object from exposure to sync thread
    interface_lock_exclusive(thisEngine);
    assert(0 < thisEngine->mInstanceCount);
    --thisEngine->mInstanceCount;
    assert(0 != thisEngine->mInstanceMask);
    thisEngine->mInstanceMask &= ~(1 << i);
    assert(thisEngine->mInstances[i] == this);
    thisEngine->mInstances[i] = NULL;
    interface_unlock_exclusive(thisEngine);
    object_lock_exclusive(this);
    // Call the deinitializer for each currently exposed interface,
    // whether it is implicit, explicit, optional, or dynamically added.
    unsigned exposedMask = this->mExposedMask;
    for ( ; exposedMask; exposedMask >>= 1, ++x) {
        if (exposedMask & 1) {
            VoidHook deinit = MPH_init_table[x->mMPH].mDeinit;
            if (NULL != deinit)
                (*deinit)((char *) this + x->mOffset);
        }
    }
    if (NULL != destroy)
        (*destroy)(this);
    // redundant: this->mState = SL_OBJECT_STATE_UNREALIZED;
    object_unlock_exclusive(this);
#ifndef NDEBUG
    memset(this, 0x55, class__->mSize);
#endif
    free(this);
}

static SLresult IObject_SetPriority(SLObjectItf self, SLint32 priority, SLboolean preemptable)
{
    IObject *this = (IObject *) self;
    object_lock_exclusive(this);
    this->mPriority = priority;
    this->mPreemptable = preemptable;
    object_unlock_exclusive(this);
    return SL_RESULT_SUCCESS;
}

static SLresult IObject_GetPriority(SLObjectItf self, SLint32 *pPriority, SLboolean *pPreemptable)
{
    if (NULL == pPriority || NULL == pPreemptable)
        return SL_RESULT_PARAMETER_INVALID;
    IObject *this = (IObject *) self;
    object_lock_shared(this);
    SLint32 priority = this->mPriority;
    SLboolean preemptable = this->mPreemptable;
    object_unlock_shared(this);
    *pPriority = priority;
    *pPreemptable = preemptable;
    return SL_RESULT_SUCCESS;
}

static SLresult IObject_SetLossOfControlInterfaces(SLObjectItf self,
    SLint16 numInterfaces, SLInterfaceID *pInterfaceIDs, SLboolean enabled)
{
    if (0 < numInterfaces) {
        SLuint32 i;
        if (NULL == pInterfaceIDs)
            return SL_RESULT_PARAMETER_INVALID;
        IObject *this = (IObject *) self;
        const ClassTable *class__ = this->mClass;
        unsigned lossOfControlMask = 0;
        // FIXME The cast is due to a typo in the spec
        for (i = 0; i < (SLuint32) numInterfaces; ++i) {
            SLInterfaceID iid = pInterfaceIDs[i];
            if (NULL == iid)
                return SL_RESULT_PARAMETER_INVALID;
            int MPH, index;
            if (0 <= (MPH = IID_to_MPH(iid)) &&
                (0 <= (index = class__->mMPH_to_index[MPH])))
                lossOfControlMask |= (1 << index);
        }
        object_lock_exclusive(this);
        if (enabled)
            this->mLossOfControlMask |= lossOfControlMask;
        else
            this->mLossOfControlMask &= ~lossOfControlMask;
        object_unlock_exclusive(this);
    }
    return SL_RESULT_SUCCESS;
}

static const struct SLObjectItf_ IObject_Itf = {
    IObject_Realize,
    IObject_Resume,
    IObject_GetState,
    IObject_GetInterface,
    IObject_RegisterCallback,
    IObject_AbortAsyncOperation,
    IObject_Destroy,
    IObject_SetPriority,
    IObject_GetPriority,
    IObject_SetLossOfControlInterfaces,
};

void IObject_init(void *self)
{
    IObject *this = (IObject *) self;
    this->mItf = &IObject_Itf;
    // initialized in construct:
    // mClass
    // mInstanceID
    // mExposedMask
    // mLossOfControlMask
    // mEngine
    this->mState = SL_OBJECT_STATE_UNREALIZED;
    this->mCallback = NULL;
    this->mContext = NULL;
    this->mPriority = 0;
    this->mPreemptable = SL_BOOLEAN_FALSE;
    int ok;
    ok = pthread_mutex_init(&this->mMutex, (const pthread_mutexattr_t *) NULL);
    assert(0 == ok);
    ok = pthread_cond_init(&this->mCond, (const pthread_condattr_t *) NULL);
    assert(0 == ok);
}
