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

// sp<> capable proxy for AudioTrack

#include <media/AudioTrack.h>

namespace android {

class AudioTrackProxy : public RefBase {

public:

    AudioTrackProxy(AudioTrack *raw) : mRaw(raw) { assert(raw != NULL); }

    // don't define all methods, just the ones needed

    void setVolume(float left, float right)
            { mRaw->setVolume(left, right); }
    void stop()
            { mRaw->stop(); }
    void start()
            { mRaw->start(); }
    status_t initCheck()
            { return mRaw->initCheck(); }
    status_t setSampleRate(int sampleRate)
            { return mRaw->setSampleRate(sampleRate); }
    void pause()
            { mRaw->pause(); }
    void getPosition(uint32_t *p)
            { mRaw->getPosition(p); }
    void mute(bool muted)
            { mRaw->mute(muted); }
    void flush()
            { mRaw->flush(); }
    void setMarkerPosition(uint32_t marker)
            { mRaw->setMarkerPosition(marker); }
    void setPositionUpdatePeriod(uint32_t updatePeriod)
            { mRaw->setPositionUpdatePeriod(updatePeriod); }
    status_t attachAuxEffect(int effectId)
            { return mRaw->attachAuxEffect(effectId); }
    status_t setAuxEffectSendLevel(float level)
            { return mRaw->setAuxEffectSendLevel(level); }

protected:

    virtual ~AudioTrackProxy()
            { }
    virtual void onLastStrongRef(const void* id)
            {
                assert(mRaw != NULL);
                delete mRaw;
                mRaw = NULL;
            }

private:
    android::AudioTrack *mRaw;

    AudioTrackProxy(const AudioTrackProxy &);
    AudioTrackProxy &operator=(const AudioTrackProxy &);
};

}
