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

/** \file ThreadPool.h ThreadPool interface */

/** Kind of closure */

typedef enum {
    CLOSURE_KIND_PPI,   // void *, void *, int
    CLOSURE_KIND_PPII   // void *, void *, int, int
} ClosureKind;

/** Closure handlers */

typedef void (*ClosureHandler_ppi)(void *context1, void *context2, int parameter1);
typedef void (*ClosureHandler_ppii)(void *context1, void *context2, int parameter1, int parameter2);

/** \brief Closure represents a deferred computation */

typedef struct {
    union {
        ClosureHandler_ppi mHandler_ppi;
        ClosureHandler_ppii mHandler_ppii;
    } mHandler;
    ClosureKind mKind;
    void *mContext1;
    void *mContext2;
    int mParameter1;
    int mParameter2;
} Closure;

/** \brief ThreadPool manages a pool of worker threads that execute Closures */

typedef struct {
    unsigned mInitialized; ///< Indicates which of the following 3 fields are initialized
    pthread_mutex_t mMutex;
    pthread_cond_t mCondNotFull;    ///< Signalled when a client thread could be unblocked
    pthread_cond_t mCondNotEmpty;   ///< Signalled when a worker thread could be unblocked
    SLboolean mShutdown;   ///< Whether shutdown of thread pool has been requested
    unsigned mWaitingNotFull;   ///< Number of client threads waiting to enqueue
    unsigned mWaitingNotEmpty;  ///< Number of worker threads waiting to dequeue
    unsigned mMaxClosures;  ///< Number of slots in circular buffer for closures, not counting spare
    unsigned mMaxThreads;   ///< Number of worker threads
    Closure **mClosureArray;    ///< The circular buffer of closures
    Closure **mClosureFront, **mClosureRear;
    /// Saves a malloc in the typical case
#define CLOSURE_TYPICAL 15
    Closure *mClosureTypical[CLOSURE_TYPICAL+1];
    pthread_t *mThreadArray;    ///< The worker threads
#ifdef ANDROID
#define THREAD_TYPICAL 2
#else
#define THREAD_TYPICAL 4
#endif
    pthread_t mThreadTypical[THREAD_TYPICAL];
} ThreadPool;

extern SLresult ThreadPool_init(ThreadPool *tp, unsigned maxClosures, unsigned maxThreads);
extern void ThreadPool_deinit(ThreadPool *tp);
extern SLresult ThreadPool_add(ThreadPool *tp, ClosureKind kind,
    void (*handler)(void *, void *, int, int), void *context1,
    void *context2, int parameter1, int parameter2);
extern Closure *ThreadPool_remove(ThreadPool *tp);
extern SLresult ThreadPool_add_ppi(ThreadPool *tp, ClosureHandler_ppi handler,
    void *context1, void *context2, int parameter1);
extern SLresult ThreadPool_add_ppii(ThreadPool *tp, ClosureHandler_ppii handler,
    void *context1, void *context2, int parameter1, int parameter2);
