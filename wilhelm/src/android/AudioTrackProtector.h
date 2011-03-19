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

#include "utils/threads.h"

//--------------------------------------------------------------------------------------------------
namespace android {

class AudioTrackProtector : public RefBase {

public:
    AudioTrackProtector();
    virtual ~AudioTrackProtector();

    /**
     * Indicates whether it's safe to enter the AudioTrack callback. It would typically return
     * false if the AudioTrack is about to be destroyed
     */
    bool enterCb();

    /**
     * This method must be paired to each call to enterCb(), only if enterCb() returned that it is
     * safe enter the callback;
     */
    void exitCb();

    /**
     * Called to signal the track is about to be destroyed, so whenever an AudioTrack callback is
     * entered (see enterCb) it will be notified it is pointless to process the callback. This will
     * return immediately if there are no callbacks, and will block until current callbacks exit.
     */
    void requestCbExitAndWait();

protected:
    Mutex mLock;
    Condition mCbExitedCondition;

    bool mSafeToEnterCb;

    /** Counts the number of AudioTrack callbacks actively locking the associated AudioPlayer */
    unsigned int mCbCount;
};

} // namespace android
