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

/* Engine class */

#include "sles_allinclusive.h"

// IObject hooks

SLresult CEngine_Realize(void *self, SLboolean async)
{
    CEngine *this = (CEngine *) self;
    SLresult result;
    result = err_to_result(pthread_create(&this->mSyncThread, (const pthread_attr_t *) NULL, sync_start, this));
    if (SL_RESULT_SUCCESS != result)
        return result;
    // FIXME Use platform thread pool if available (e.g. on Android)
    // FIXME Use platform-specific engine configuration options here.
    result = ThreadPool_init(&this->mEngine.mThreadPool, 0, 0);
    if (SL_RESULT_SUCCESS != result) {
        // FIXME Wrong
        this->mEngine.mShutdown = SL_BOOLEAN_TRUE;
        (void) pthread_join(this->mSyncThread, (void **) NULL);
        return result;
    }
#ifdef USE_SDL
    SDL_start(&this->mEngine);
#endif
    return SL_RESULT_SUCCESS;
}

void CEngine_Destroy(void *self)
{
    CEngine *this = (CEngine *) self;
    // FIXME Cancel pending operations
    // FIXME Verify no extant objects
    // FIXME This is the wrong way to kill the sync thread
    this->mEngine.mShutdown = SL_BOOLEAN_TRUE;
    (void) pthread_join(this->mSyncThread, (void **) NULL);
    ThreadPool_deinit(&this->mEngine.mThreadPool);
}
