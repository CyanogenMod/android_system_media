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

void object_unlock_exclusive_attributes(IObject *this, unsigned attributes)
{
    int ok;
    SLuint32 objectID = IObjectToObjectID(this);
    CAudioPlayer *ap;
    // Android likes to see certain updates synchronously
    if (attributes & ATTR_GAIN) {
        switch (objectID) {
        case SL_OBJECTID_AUDIOPLAYER:
            attributes &= ~ATTR_GAIN;   // no need to process asynchronously also
            ap = (CAudioPlayer *) this;
#ifdef ANDROID
            sles_to_android_audioPlayerVolumeUpdate(ap);
#else
            audioPlayerGainUpdate(ap);
#endif
            break;
        case SL_OBJECTID_OUTPUTMIX:
            // FIXME update gains on all players attached to this outputmix
            fprintf(stderr, "FIXME: gain update on an SL_OBJECTID_OUTPUTMIX to be implemented\n");
            break;
        // FIXME MIDI
        default:
            break;
        }
    }
    if (attributes & ATTR_TRANSPORT) {
        if (SL_OBJECTID_AUDIOPLAYER == objectID) {
            attributes &= ~ATTR_TRANSPORT;   // no need to process asynchronously also
            ap = (CAudioPlayer *) this;
#ifdef ANDROID
            // FIXME should only call when state changes
            sles_to_android_audioPlayerSetPlayState(ap);
            // FIXME ditto, but for either eventflags or marker position
            sles_to_android_audioPlayerUseEventMask(ap);
#else
            audioPlayerTransportUpdate(ap);
#endif
        }
    }
    if (attributes) {
        unsigned oldAttributesMask = this->mAttributesMask;
        this->mAttributesMask = oldAttributesMask | attributes;
        if (oldAttributesMask)
            attributes = ATTR_NONE;
    }
    ok = pthread_mutex_unlock(&this->mMutex);
    assert(0 == ok);
    if (attributes) {   // first update to this interface since previous sync
        IEngine *thisEngine = this->mEngine;
        interface_lock_exclusive(thisEngine);
        thisEngine->mChangedMask |= 1 << (this->mInstanceID - 1);
        interface_unlock_exclusive(thisEngine);
    }
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

void object_cond_broadcast(IObject *this)
{
    int ok;
    ok = pthread_cond_broadcast(&this->mCond);
    assert(0 == ok);
}
