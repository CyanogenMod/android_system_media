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

typedef struct Closure_struct {
    void (*mHandler)(struct Closure_struct *);
    void *mContext;
} Closure;

#ifndef __cplusplus
typedef int bool;
#define false 0
#define true 1
#endif

typedef struct {
    unsigned mInitialized; // indicates which of the following 3 fields are initialized
    pthread_mutex_t mMutex;
    pthread_cond_t mCondNotFull;
    pthread_cond_t mCondNotEmpty;
    SLboolean mShutdown;   // whether shutdown of thread pool has been requested
    unsigned mWaitingNotFull;
    unsigned mWaitingNotEmpty;
    unsigned mMaxClosures;
    unsigned mMaxThreads;
    Closure **mClosureArray;
    Closure **mClosureFront, **mClosureRear;
    // saves a malloc in the typical case
#define CLOSURE_TYPICAL 15
    Closure *mClosureTypical[CLOSURE_TYPICAL+1];
    pthread_t *mThreadArray;
#define THREAD_TYPICAL 4
    pthread_t mThreadTypical[THREAD_TYPICAL];
} ThreadPool;

extern SLresult ThreadPool_init(ThreadPool *tp, unsigned maxClosures, unsigned maxThreads);
extern void ThreadPool_deinit(ThreadPool *tp);
extern bool ThreadPool_add(ThreadPool *tp, Closure *pClosure);
extern Closure *ThreadPool_remove(ThreadPool *tp);
