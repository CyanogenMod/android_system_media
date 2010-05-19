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

#include "sles_allinclusive.h"

void object_lock_exclusive(IObject *this)
{
    int ok;
    ok = pthread_mutex_lock(&this->mMutex);
    assert(0 == ok);
}

void object_unlock_exclusive(IObject *this)
{
    int ok;
    ok = pthread_mutex_unlock(&this->mMutex);
    assert(0 == ok);
}

void object_cond_wait(IObject *this)
{
    int ok;
    ok = pthread_cond_wait(&this->mCond, &this->mMutex);
    assert(0 == ok);
}

void object_cond_signal(IObject *this)
{
    int ok;
    ok = pthread_cond_signal(&this->mCond);
    assert(0 == ok);
}
