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

// Called by a worker thread to handle an asynchronous Object.Realize.
// Parameter self is the Object.

static void HandleRealize(void *self, int unused)
{

    // validate input parameters
    IObject *this = (IObject *) self;
    assert(NULL != this);
    const ClassTable *class__ = this->mClass;
    assert(NULL != class__);
    AsyncHook realize = class__->mRealize;
    SLresult result;
    SLuint32 state;

    // check object state
    object_lock_exclusive(this);
    state = this->mState;
    switch (state) {

    case SL_OBJECT_STATE_REALIZING_1:   // normal case
        if (NULL != realize) {
            this->mState = SL_OBJECT_STATE_REALIZING_2;
            object_unlock_exclusive(this);
            // Note that the mutex is unlocked during the realize hook
            result = (*realize)(this, SL_BOOLEAN_TRUE);
            object_lock_exclusive(this);
            assert(SL_OBJECT_STATE_REALIZING_2 == this->mState);
            state = SL_RESULT_SUCCESS == result ? SL_OBJECT_STATE_REALIZED :
                SL_OBJECT_STATE_UNREALIZED;
        } else {
            result = SL_RESULT_SUCCESS;
            state = SL_OBJECT_STATE_REALIZED;
        }
        break;

    case SL_OBJECT_STATE_REALIZING_1A:  // operation was aborted while on work queue
        result = SL_RESULT_OPERATION_ABORTED;
        state = SL_OBJECT_STATE_UNREALIZED;
        break;

    default:                            // impossible
        assert(SL_BOOLEAN_FALSE);
        result = SL_RESULT_INTERNAL_ERROR;
        break;

    }

    // mutex is locked, update state
    this->mState = state;

    // Make a copy of these, so we can call the callback with mutex unlocked
    slObjectCallback callback = this->mCallback;
    void *context = this->mContext;
    object_unlock_exclusive(this);

    // Note that the mutex is unlocked during the callback
    if (NULL != callback)
        (*callback)(&this->mItf, context, SL_OBJECT_EVENT_ASYNC_TERMINATION, result, state, NULL);
}

static SLresult IObject_Realize(SLObjectItf self, SLboolean async)
{
    IObject *this = (IObject *) self;
    SLuint32 state;
    const ClassTable *class__ = this->mClass;
    object_lock_exclusive(this);
    state = this->mState;
    // Reject redundant calls to Realize
    if (SL_OBJECT_STATE_UNREALIZED != state) {
        object_unlock_exclusive(this);
        return SL_RESULT_PRECONDITIONS_VIOLATED;
    }
    // Asynchronous: mark operation pending and cancellable
    if (async && (SL_OBJECTID_ENGINE != class__->mObjectID)) {
        state = SL_OBJECT_STATE_REALIZING_1;
    // Synchronous: mark operation pending and non-cancellable
    } else {
        state = SL_OBJECT_STATE_REALIZING_2;
    }
    this->mState = state;
    object_unlock_exclusive(this);
    SLresult result;
    switch (state) {
    case SL_OBJECT_STATE_REALIZING_1: // asynchronous on non-Engine
        assert(async);
        result = ThreadPool_add(&this->mEngine->mThreadPool, HandleRealize, this, 0);
        if (SL_RESULT_SUCCESS != result) {
            // Engine was destroyed during realize, or insufficient memory
            object_lock_exclusive(this);
            this->mState = SL_OBJECT_STATE_UNREALIZED;
            object_unlock_exclusive(this);
        }
        break;
    case SL_OBJECT_STATE_REALIZING_2: // synchronous, or asynchronous on Engine
        {
        AsyncHook realize = class__->mRealize;
        // Note that the mutex is unlocked during the realize hook
        result = (NULL != realize) ? (*realize)(this, async) : SL_RESULT_SUCCESS;
        object_lock_exclusive(this);
        assert(SL_OBJECT_STATE_REALIZING_2 == this->mState);
        state = (SL_RESULT_SUCCESS == result) ? SL_OBJECT_STATE_REALIZED :
            SL_OBJECT_STATE_UNREALIZED;
        this->mState = state;
        slObjectCallback callback = this->mCallback;
        void *context = this->mContext;
        object_unlock_exclusive(this);
        // asynchronous Realize on an Engine is actually done synchronously, but still has callback
        // This is because there is no thread pool yet to do it asynchronously.
        if (async && (NULL != callback))
            (*callback)(&this->mItf, context, SL_OBJECT_EVENT_ASYNC_TERMINATION, result, state,
                NULL);
        }
        break;
    default:                          // impossible
        assert(SL_BOOLEAN_FALSE);
        break;
    }
    return result;
}

// Called by a worker thread to handle an asynchronous Object.Resume.
// Parameter self is the Object.

static void HandleResume(void *self, int unused)
{

    // valid input parameters
    IObject *this = (IObject *) self;
    assert(NULL != this);
    const ClassTable *class__ = this->mClass;
    assert(NULL != class__);
    AsyncHook resume = class__->mResume;
    SLresult result;
    SLuint32 state;

    // check object state
    object_lock_exclusive(this);
    state = this->mState;
    switch (state) {

    case SL_OBJECT_STATE_RESUMING_1:    // normal case
        if (NULL != resume) {
            this->mState = SL_OBJECT_STATE_RESUMING_2;
            object_unlock_exclusive(this);
            // Note that the mutex is unlocked during the resume hook
            result = (*resume)(this, SL_BOOLEAN_TRUE);
            object_lock_exclusive(this);
            assert(SL_OBJECT_STATE_RESUMING_2 == this->mState);
            state = SL_RESULT_SUCCESS == result ? SL_OBJECT_STATE_REALIZED :
                SL_OBJECT_STATE_SUSPENDED;
        } else {
            result = SL_RESULT_SUCCESS;
            state = SL_OBJECT_STATE_REALIZED;
        }
        break;

    case SL_OBJECT_STATE_RESUMING_1A:   // operation was aborted while on work queue
        result = SL_RESULT_OPERATION_ABORTED;
        state = SL_OBJECT_STATE_SUSPENDED;
        break;

    default:                            // impossible
        assert(SL_BOOLEAN_FALSE);
        result = SL_RESULT_INTERNAL_ERROR;
        break;

    }

    // mutex is unlocked, update state
    this->mState = state;

    // Make a copy of these, so we can call the callback with mutex unlocked
    slObjectCallback callback = this->mCallback;
    void *context = this->mContext;
    object_unlock_exclusive(this);

    // Note that the mutex is unlocked during the callback
    if (NULL != callback)
        (*callback)(&this->mItf, context, SL_OBJECT_EVENT_ASYNC_TERMINATION, result, state, NULL);
}

static SLresult IObject_Resume(SLObjectItf self, SLboolean async)
{
    IObject *this = (IObject *) self;
    const ClassTable *class__ = this->mClass;
    SLresult result;
    SLuint32 state;
    object_lock_exclusive(this);
    state = this->mState;
    // Reject redundant calls to Resume
    if (SL_OBJECT_STATE_SUSPENDED != state) {
        object_unlock_exclusive(this);
        return SL_RESULT_PRECONDITIONS_VIOLATED;
    }
    // Asynchronous: mark operation pending and cancellable
    if (async) {
        state = SL_OBJECT_STATE_RESUMING_1;
    // Synchronous: mark operatio pending and non-cancellable
    } else {
        state = SL_OBJECT_STATE_RESUMING_2;
    }
    this->mState = state;
    object_unlock_exclusive(this);
    switch (state) {
    case SL_OBJECT_STATE_RESUMING_1: // asynchronous
        assert(async);
        result = ThreadPool_add(&this->mEngine->mThreadPool, HandleResume, this, 0);
        if (SL_RESULT_SUCCESS != result) {
            // Engine was destroyed during resume, or insufficient memory
            object_lock_exclusive(this);
            this->mState = SL_OBJECT_STATE_SUSPENDED;
            object_unlock_exclusive(this);
        }
        break;
    case SL_OBJECT_STATE_RESUMING_2: // synchronous
        {
        AsyncHook resume = class__->mResume;
        // Note that the mutex is unlocked during the resume hook
        result = (NULL != resume) ? (*resume)(this, SL_BOOLEAN_FALSE) : SL_RESULT_SUCCESS;
        object_lock_exclusive(this);
        assert(SL_OBJECT_STATE_RESUMING_2 == this->mState);
        this->mState = (SL_RESULT_SUCCESS == result) ? SL_OBJECT_STATE_REALIZED :
            SL_OBJECT_STATE_SUSPENDED;
        object_unlock_exclusive(this);
        }
        break;
    default:                        // impossible
        assert(SL_BOOLEAN_FALSE);
        break;
    }
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
    // Re-map the realizing and suspended states
    switch (state) {
    case SL_OBJECT_STATE_REALIZING_1:
    case SL_OBJECT_STATE_REALIZING_1A:
    case SL_OBJECT_STATE_REALIZING_2:
        state = SL_OBJECT_STATE_UNREALIZED;
        break;
    case SL_OBJECT_STATE_RESUMING_1:
    case SL_OBJECT_STATE_RESUMING_1A:
    case SL_OBJECT_STATE_RESUMING_2:
    case SL_OBJECT_STATE_SUSPENDING:
        state = SL_OBJECT_STATE_SUSPENDED;
        break;
    case SL_OBJECT_STATE_UNREALIZED:
    case SL_OBJECT_STATE_REALIZED:
    case SL_OBJECT_STATE_SUSPENDED:
        // These are the "official" object states, return them as is
        break;
    default:
        assert(SL_BOOLEAN_FALSE);
        break;
    }
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
        if ((0 > (MPH = IID_to_MPH(iid))) || (0 > (index = class__->mMPH_to_index[MPH]))) {
            result = SL_RESULT_FEATURE_UNSUPPORTED;
        } else {
            unsigned mask = 1 << index;
            object_lock_exclusive(this);
            // Can't get interface on a suspended/suspending/resuming object
            if (SL_OBJECT_STATE_REALIZED != this->mState) {
                result = SL_RESULT_PRECONDITIONS_VIOLATED;
            } else {
                switch (this->mInterfaceStates[index]) {
                case INTERFACE_EXPOSED:
                case INTERFACE_ADDED:
                    // Note that interface has been gotten,
                    // for debugger and to detect incorrect use of interfaces
                    this->mGottenMask |= mask;
                    interface = (char *) this + class__->mInterfaces[index].mOffset;
                    result = SL_RESULT_SUCCESS;
                    break;
                // Can't get interface if uninitialized/suspend(ed,ing)/resuming/adding/removing
                default:
                    result = SL_RESULT_FEATURE_UNSUPPORTED;
                    break;
                }
            }
            object_unlock_exclusive(this);
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

// internal common code for Abort and Destroy

static void Abort_internal(IObject *this, SLboolean shutdown)
{
    // FIXME Aborting asynchronous operations during phase 2 is not yet implemented
    const ClassTable *class__ = this->mClass;
    object_lock_exclusive(this);
    // Abort asynchronous operations on the object
    switch (this->mState) {
    case SL_OBJECT_STATE_REALIZING_1:   // Realize
        this->mState = SL_OBJECT_STATE_REALIZING_1A;
        break;
    case SL_OBJECT_STATE_RESUMING_1:    // Resume
        this->mState = SL_OBJECT_STATE_RESUMING_1A;
        break;
    default:
        break;
    }
    // Abort asynchronous operations on interfaces
    // FIXME O(n), could be O(1) using a generation count
    SLuint8 *interfaceStateP = this->mInterfaceStates;
    unsigned index;
    for (index = 0; index < class__->mInterfaceCount; ++index, ++interfaceStateP) {
        switch (*interfaceStateP) {
        case INTERFACE_ADDING_1:    // AddInterface
            *interfaceStateP = INTERFACE_ADDING_1A;
            break;
        case INTERFACE_RESUMING_1:  // ResumeInterface
            *interfaceStateP = INTERFACE_RESUMING_1A;
            break;
        default:
            break;
        }
    }
#if 0
    // FIXME Not fully implemented, the intention is to advise other threads to exit from sync ops
    // FIXME Also need to wait for async ops to recognize the shutdown
    if (shutdown)
        this->mShutdown = SL_BOOLEAN_TRUE;
#endif
    object_unlock_exclusive(this);
}

static void IObject_AbortAsyncOperation(SLObjectItf self)
{
    IObject *this = (IObject *) self;
    Abort_internal(this, SL_BOOLEAN_FALSE);
}

static void IObject_Destroy(SLObjectItf self)
{
    // FIXME Check for dependencies, e.g. destroying an output mix with attached players,
    //       destroying an engine with extant objects, etc.
    // FIXME The abort should be atomic w.r.t. destroy, so another async can't be started in window
    // FIXME Destroy may need to be made asynchronous to permit safe cleanup of resources
    IObject *this = (IObject *) self;
    Abort_internal(this, SL_BOOLEAN_TRUE);
    const ClassTable *class__ = this->mClass;
    VoidHook destroy = class__->mDestroy;
    const struct iid_vtable *x = class__->mInterfaces;
    // const, no lock needed
    IEngine *thisEngine = this->mEngine;
    unsigned i = this->mInstanceID;
    assert(0 < i && i <= MAX_INSTANCE);
    --i;
    // remove object from exposure to sync thread and debugger
    interface_lock_exclusive(thisEngine);
    assert(0 < thisEngine->mInstanceCount);
    --thisEngine->mInstanceCount;
    assert(0 != thisEngine->mInstanceMask);
    thisEngine->mInstanceMask &= ~(1 << i);
    assert(thisEngine->mInstances[i] == this);
    thisEngine->mInstances[i] = NULL;
#ifdef USE_SDL
    if (SL_OBJECTID_OUTPUTMIX == class__->mObjectID && (COutputMix *) this == thisEngine->mOutputMix) {
        SDL_PauseAudio(1);
        thisEngine->mOutputMix = NULL;
        // Note we don't attempt to connect another output mix to SDL
    }
#endif
    interface_unlock_exclusive(thisEngine);
    object_lock_exclusive(this);
    if (NULL != destroy)
        (*destroy)(this);
    // Call the deinitializer for each currently exposed interface,
    // whether it is implicit, explicit, optional, or dynamically added.
    unsigned incorrect = 0;
    SLuint8 *interfaceStateP = this->mInterfaceStates;
    unsigned index;
    for (index = 0; index < class__->mInterfaceCount; ++index, ++x, ++interfaceStateP) {
        SLuint32 state = *interfaceStateP;
        switch (state) {
        case INTERFACE_UNINITIALIZED:
            break;
        case INTERFACE_EXPOSED:     // quiescent states
        case INTERFACE_ADDED:
        case INTERFACE_SUSPENDED:
            {
            VoidHook deinit = MPH_init_table[x->mMPH].mDeinit;
            if (NULL != deinit)
                (*deinit)((char *) this + x->mOffset);
            }
            break;
        case INTERFACE_ADDING_1:    // active states indicate incorrect use of API
        case INTERFACE_ADDING_1A:
        case INTERFACE_ADDING_2:
        case INTERFACE_RESUMING_1:
        case INTERFACE_RESUMING_1A:
        case INTERFACE_RESUMING_2:
        case INTERFACE_REMOVING:
        case INTERFACE_SUSPENDING:
            ++incorrect;
            break;
        default:
            assert(SL_BOOLEAN_FALSE);
            break;
        }
    }
    // redundant: this->mState = SL_OBJECT_STATE_UNREALIZED;
    object_unlock_exclusive(this);
#ifndef NDEBUG
    memset(this, 0x55, class__->mSize);
#endif
    free(this);
    // one or more interfaces was actively changing at time of Destroy
    assert(incorrect == 0);
    // FIXME assert might be the wrong thing to do; instead block until active ops complete?
}

static SLresult IObject_SetPriority(SLObjectItf self, SLint32 priority, SLboolean preemptable)
{
    IObject *this = (IObject *) self;
    object_lock_exclusive(this);
    this->mPriority = priority;
    this->mPreemptable = SL_BOOLEAN_FALSE != preemptable; // normalize
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
            if ((0 <= (MPH = IID_to_MPH(iid))) && (0 <= (index = class__->mMPH_to_index[MPH])))
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
    // mLossOfControlMask
    // mEngine
    // mInstanceStates
    this->mState = SL_OBJECT_STATE_UNREALIZED;
    this->mGottenMask = 1;  // IObject
    this->mCallback = NULL;
    this->mContext = NULL;
    this->mPriority = SL_PRIORITY_NORMAL;
    this->mPreemptable = SL_BOOLEAN_FALSE;
    int ok;
    ok = pthread_mutex_init(&this->mMutex, (const pthread_mutexattr_t *) NULL);
    assert(0 == ok);
    ok = pthread_cond_init(&this->mCond, (const pthread_condattr_t *) NULL);
    assert(0 == ok);
}
