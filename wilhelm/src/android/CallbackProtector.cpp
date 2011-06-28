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

#include "CallbackProtector.h"

#include <media/stagefright/foundation/ADebug.h>

//--------------------------------------------------------------------------------------------------
namespace android {


CallbackProtector::CallbackProtector() : RefBase(),
        mSafeToEnterCb(true),
        mCbCount(0) {
}


CallbackProtector::~CallbackProtector() {

}


bool CallbackProtector::enterCbIfOk(const sp<CallbackProtector> &protector) {
    if (protector != 0) {
        return protector->enterCb();
    } else {
        return false;
    }
}


bool CallbackProtector::enterCb() {
    Mutex::Autolock _l(mLock);
    if (mSafeToEnterCb) {
        mCbCount++;
    }
    return mSafeToEnterCb;
}


void CallbackProtector::exitCb() {
    Mutex::Autolock _l(mLock);

    CHECK(mCbCount > 0);
    mCbCount--;

    if (mCbCount == 0) {
        mCbExitedCondition.broadcast();
    }
}


void CallbackProtector::requestCbExitAndWait() {
    Mutex::Autolock _l(mLock);
    mSafeToEnterCb = false;
    while (mCbCount) {
        mCbExitedCondition.wait(mLock);
    }
}


} // namespace android
