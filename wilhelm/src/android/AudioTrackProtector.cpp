/*
 * Copyright (C) 2011 The Android Open Source Project
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

#include "AudioTrackProtector.h"

//--------------------------------------------------------------------------------------------------
namespace android {


AudioTrackProtector::AudioTrackProtector() : RefBase(),
        mSafeToEnterCb(true),
        mCbCount(0) {
}


AudioTrackProtector::~AudioTrackProtector() {

}


bool AudioTrackProtector::enterCb() {
    Mutex::Autolock _l(mLock);
    if (mSafeToEnterCb) {
        mCbCount++;
    }
    return mSafeToEnterCb;
}


void AudioTrackProtector::exitCb() {
    Mutex::Autolock _l(mLock);
    if (mCbCount > 0) {
        mCbCount--;
    }
    if (mCbCount == 0) {
        mCbExitedCondition.broadcast();
    }
}


void AudioTrackProtector::requestCbExitAndWait() {
    Mutex::Autolock _l(mLock);
    mSafeToEnterCb = false;
    while (mCbCount) {
        mCbExitedCondition.wait(mLock);
    }
}


} // namespace android
