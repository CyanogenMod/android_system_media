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

/* sync */

#include "sles_allinclusive.h"

// The sync thread runs periodically to synchronize audio state between
// the application and platform-specific device driver; for best results
// it should run about every graphics frame (e.g. 20 Hz to 50 Hz).

void *sync_start(void *arg)
{
    CEngine *this = (CEngine *) arg;
    for (;;) {
        usleep(20000*50);
        // Normally a shared lock would be needed for reading 2 fields,
        // but in this case their relationship is irrelevant.
        object_lock_peek(&this->mObject);
        SLboolean shutdown = this->mEngine.mShutdown;
        unsigned instanceMask = this->mEngine.mInstanceMask;
        object_unlock_peek(&this->mObject);
        if (shutdown)
            break;
        while (instanceMask) {
            unsigned i = ctz(instanceMask);
            assert(MAX_INSTANCE > i);
            instanceMask &= ~(1 << i);
            IObject *instance = (IObject *) this->mEngine.mInstances[i];
            // Could be NULL during construct or destroy
            if (NULL == instance)
                continue;
            // FIXME race condition here - object could be destroyed by now, better to do destroy here
            switch (IObjectToObjectID(instance)) {
            case SL_OBJECTID_AUDIOPLAYER:
                putchar('.');
                fflush(stdout);
                break;
            default:
                break;
            }
        }
    }
    return NULL;
}
