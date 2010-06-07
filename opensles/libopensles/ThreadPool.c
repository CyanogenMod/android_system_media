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

/* ThreadPool */

#include "sles_allinclusive.h"

// Entry point for each worker thread

static void *ThreadPool_start(void *context)
{
    ThreadPool *tp = (ThreadPool *) context;
    assert(NULL != tp);
    for (;;) {
        Closure *pClosure = ThreadPool_remove(tp);
        // closure is NULL when engine is being destroyed
        if (NULL == pClosure)
            break;
        void (*handler)(Closure *);
        handler = pClosure->mHandler;
        assert(NULL != handler);
        (*handler)(pClosure);
    }
    return NULL;
}

#define INITIALIZED_NONE         0
#define INITIALIZED_MUTEX        1
#define INITIALIZED_CONDNOTFULL  2
#define INITIALIZED_CONDNOTEMPTY 4
#define INITIALIZED_ALL          7

static void ThreadPool_deinit_internal(ThreadPool *tp, unsigned initialized, unsigned nThreads);

// Initialize a ThreadPool
// maxClosures defaults to CLOSURE_TYPICAL if 0
// maxThreads defaults to THREAD_TYPICAL if 0

SLresult ThreadPool_init(ThreadPool *tp, unsigned maxClosures, unsigned maxThreads)
{
    assert(NULL != tp);
    memset(tp, 0, sizeof(ThreadPool));
    tp->mShutdown = SL_BOOLEAN_FALSE;
    unsigned initialized = INITIALIZED_NONE;
    unsigned nThreads = 0;
    SLresult result;
    result = err_to_result(pthread_mutex_init(&tp->mMutex, (const pthread_mutexattr_t *) NULL));
    if (SL_RESULT_SUCCESS != result)
        goto fail;
    initialized |= INITIALIZED_MUTEX;
    result = err_to_result(pthread_cond_init(&tp->mCondNotFull, (const pthread_condattr_t *) NULL));
    if (SL_RESULT_SUCCESS != result)
        goto fail;
    initialized |= INITIALIZED_CONDNOTFULL;
    result = err_to_result(pthread_cond_init(&tp->mCondNotEmpty, (const pthread_condattr_t *) NULL));
    if (SL_RESULT_SUCCESS != result)
        goto fail;
    initialized |= INITIALIZED_CONDNOTEMPTY;
    tp->mWaitingNotFull = 0;
    tp->mWaitingNotEmpty = 0;
    if (0 == maxClosures)
        maxClosures = CLOSURE_TYPICAL;
    tp->mMaxClosures = maxClosures;
    if (0 == maxThreads)
        maxThreads = THREAD_TYPICAL;
    tp->mMaxThreads = maxThreads;
    if (CLOSURE_TYPICAL >= maxClosures) {
        tp->mClosureArray = tp->mClosureTypical;
    } else {
        tp->mClosureArray = (Closure **) malloc((maxClosures + 1) * sizeof(Closure *));
        if (NULL == tp->mClosureArray) {
            result = SL_RESULT_RESOURCE_ERROR;
            goto fail;
        }
    }
    tp->mClosureFront = tp->mClosureArray;
    tp->mClosureRear = tp->mClosureArray;
    if (THREAD_TYPICAL >= maxThreads) {
        tp->mThreadArray = tp->mThreadTypical;
    } else {
        tp->mThreadArray = (pthread_t *) malloc(maxThreads * sizeof(pthread_t));
        if (NULL == tp->mThreadArray) {
            result = SL_RESULT_RESOURCE_ERROR;
            goto fail;
        }
    }
    unsigned i;
    for (i = 0; i < maxThreads; ++i) {
        result = err_to_result(pthread_create(&tp->mThreadArray[i], (const pthread_attr_t *) NULL, ThreadPool_start, tp));
        if (SL_RESULT_SUCCESS != result)
            goto fail;
        ++nThreads;
    }
    tp->mInitialized = initialized;
    return SL_RESULT_SUCCESS;
fail:
    ThreadPool_deinit_internal(tp, initialized, nThreads);
    return result;
}

static void ThreadPool_deinit_internal(ThreadPool *tp, unsigned initialized, unsigned nThreads)
{
    assert(NULL != tp);
    // FIXME Cancel all pending operations
    // Destroy all threads
    if (0 < nThreads) {
        int ok;
        assert(INITIALIZED_ALL == initialized);
        ok = pthread_mutex_lock(&tp->mMutex);
        assert(0 == ok);
        tp->mShutdown = SL_BOOLEAN_TRUE;
        ok = pthread_cond_broadcast(&tp->mCondNotEmpty);
        assert(0 == ok);
        ok = pthread_cond_broadcast(&tp->mCondNotFull);
        assert(0 == ok);
        ok = pthread_mutex_unlock(&tp->mMutex);
        assert(0 == ok);
        unsigned i;
        for (i = 0; i < nThreads; ++i) {
            ok = pthread_join(tp->mThreadArray[i], (void **) NULL);
            assert(ok == 0);
        }
        ok = pthread_mutex_lock(&tp->mMutex);
        assert(0 == ok);
        assert(0 == tp->mWaitingNotEmpty);
        ok = pthread_mutex_unlock(&tp->mMutex);
        assert(0 == ok);
        // Note that we can't be sure when mWaitingNotFull will drop to zero
    }
    if (initialized & INITIALIZED_CONDNOTEMPTY)
        (void) pthread_cond_destroy(&tp->mCondNotEmpty);
    if (initialized & INITIALIZED_CONDNOTFULL)
        (void) pthread_cond_destroy(&tp->mCondNotFull);
    if (initialized & INITIALIZED_MUTEX)
        (void) pthread_mutex_destroy(&tp->mMutex);
    tp->mInitialized = INITIALIZED_NONE;
    if (tp->mClosureTypical != tp->mClosureArray && NULL != tp->mClosureArray) {
        free(tp->mClosureArray);
        tp->mClosureArray = NULL;
    }
    if (tp->mThreadTypical != tp->mThreadArray && NULL != tp->mThreadArray) {
        free(tp->mThreadArray);
        tp->mThreadArray = NULL;
    }
}

void ThreadPool_deinit(ThreadPool *tp)
{
    ThreadPool_deinit_internal(tp, tp->mInitialized, tp->mMaxThreads);
}

bool ThreadPool_add(ThreadPool *tp, Closure *closure)
{
    assert(NULL != tp);
    assert(NULL != closure);
    int ok;
    ok = pthread_mutex_lock(&tp->mMutex);
    assert(0 == ok);
    for (;;) {
        Closure **oldRear = tp->mClosureRear;
        Closure **newRear = oldRear;
        if (++newRear == &tp->mClosureArray[tp->mMaxClosures + 1])
            newRear = tp->mClosureArray;
        if (newRear == tp->mClosureFront) {
            ++tp->mWaitingNotFull;
            ok = pthread_cond_wait(&tp->mCondNotFull, &tp->mMutex);
            assert(0 == ok);
            if (tp->mShutdown) {
                assert(0 < tp->mWaitingNotFull);
                --tp->mWaitingNotFull;
                ok = pthread_mutex_unlock(&tp->mMutex);
                assert(0 == ok);
                return false;
            }
            continue;
        }
        assert(NULL == *oldRear);
        *oldRear = closure;
        if (0 < tp->mWaitingNotEmpty) {
            --tp->mWaitingNotEmpty;
            ok = pthread_cond_signal(&tp->mCondNotEmpty);
            assert(0 == ok);
        }
        break;
    }
    ok = pthread_mutex_unlock(&tp->mMutex);
    assert(0 == ok);
    return true;
}

Closure *ThreadPool_remove(ThreadPool *tp)
{
    Closure *pClosure;
    int ok;
    ok = pthread_mutex_lock(&tp->mMutex);
    assert(0 == ok);
    for (;;) {
        Closure **oldFront = tp->mClosureFront;
        if (oldFront == tp->mClosureRear) {
            ++tp->mWaitingNotEmpty;
            ok = pthread_cond_wait(&tp->mCondNotEmpty, &tp->mMutex);
            assert(0 == ok);
            if (tp->mShutdown) {
                assert(0 < tp->mWaitingNotEmpty);
                --tp->mWaitingNotEmpty;
                pClosure = NULL;
                break;
            }
            continue;
        }
        Closure **newFront = oldFront;
        if (++newFront == &tp->mClosureArray[tp->mMaxClosures + 1])
            newFront = tp->mClosureArray;
        pClosure = *oldFront;
        tp->mClosureFront = newFront;
        if (0 < tp->mWaitingNotFull) {
            --tp->mWaitingNotFull;
            ok = pthread_cond_signal(&tp->mCondNotFull);
            assert(0 == ok);
        }
        break;
    }
    ok = pthread_mutex_unlock(&tp->mMutex);
    assert(0 == ok);
    return pClosure;
}
