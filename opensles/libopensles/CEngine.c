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

/** \file CEngine.c Engine class */

#include "sles_allinclusive.h"


/** \brief Hook called by Object::Realize when an engine is realized */

SLresult CEngine_Realize(void *self, SLboolean async)
{
    CEngine *this = (CEngine *) self;
    int err = pthread_create(&this->mSyncThread, (const pthread_attr_t *) NULL, sync_start, this);
    SLresult result = err_to_result(err);
    if (SL_RESULT_SUCCESS != result)
        return result;
    result = ThreadPool_init(&this->mEngine.mThreadPool, 0, 0);
    if (SL_RESULT_SUCCESS != result) {
        this->mEngine.mShutdown = SL_BOOLEAN_TRUE;
        (void) pthread_join(this->mSyncThread, (void **) NULL);
        return result;
    }
#ifdef USE_SDL
    SDL_start(&this->mEngine);
#endif
    return SL_RESULT_SUCCESS;
}


/** \brief Hook called by Object::Destroy when an engine is destroyed */

void CEngine_Destroy(void *self)
{
    CEngine *this = (CEngine *) self;
    this->mEngine.mShutdown = SL_BOOLEAN_TRUE;
#ifdef ANDROID
    // free effect data
    //   free EQ data
    if ((0 < this->mEngine.mEqNumPresets) && (NULL != this->mEngine.mEqPresetNames)) {
        for(uint32_t i = 0 ; i < this->mEngine.mEqNumPresets ; i++) {
            if (NULL != this->mEngine.mEqPresetNames[i]) {
                delete [] this->mEngine.mEqPresetNames[i];
            }
        }
        delete [] this->mEngine.mEqPresetNames;
    }
#endif
    while (!this->mEngine.mShutdownAck)
        pthread_cond_wait(&this->mEngine.mShutdownCond, &this->mObject.mMutex);
    object_unlock_exclusive(&this->mObject);
    (void) pthread_join(this->mSyncThread, (void **) NULL);
    ThreadPool_deinit(&this->mEngine.mThreadPool);

}
