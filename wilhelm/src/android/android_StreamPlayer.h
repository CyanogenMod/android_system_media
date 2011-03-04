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

#include <binder/IServiceManager.h>


//--------------------------------------------------------------------------------------------------
namespace android {

typedef void (*cb_buffAvailable_t)(const void* user, bool userIsAudioPlayer,
        size_t bufferId, void* bufferLoc, size_t buffSize);

//--------------------------------------------------------------------------------------------------
class StreamSourceAppProxy : public BnStreamSource {
public:
    StreamSourceAppProxy(slAndroidBufferQueueCallback callback,
            cb_buffAvailable_t notify,
            const void* user, bool userIsAudioPlayer,
            void *appContext,
            const void *caller);
    virtual ~StreamSourceAppProxy();

    // IStreamSource implementation
    virtual void setListener(const sp<IStreamListener> &listener);
    virtual void setBuffers(const Vector<sp<IMemory> > &buffers);
    virtual void onBufferAvailable(size_t index);

    void receivedFromAppCommand(IStreamListener::Command cmd);
    void receivedFromAppBuffer(size_t buffIndex, size_t buffLength);

private:
    Mutex mListenerLock;
    sp<IStreamListener> mListener;
    Vector<sp<IMemory> > mBuffers;

    slAndroidBufferQueueCallback mCallback; // FIXME remove
    cb_buffAvailable_t mCbNotifyBufferAvailable;
    const void* mUser;
    bool mUserIsAudioPlayer;

    void *mAppContext;
    const void *mCaller;

    DISALLOW_EVIL_CONSTRUCTORS(StreamSourceAppProxy);
};


//--------------------------------------------------------------------------------------------------
class StreamPlayer : public GenericMediaPlayer
{
public:
    StreamPlayer(AudioPlayback_Parameters* params, bool hasVideo);
    virtual ~StreamPlayer();

    void registerQueueCallback(slAndroidBufferQueueCallback callback,
            cb_buffAvailable_t notify,
            const void* user, bool userIsAudioPlayer,
            void *context,
            const void *caller);
    void appEnqueue(SLuint32 bufferId, SLuint32 length, SLuint32 event, void *pData);
    void appClear();

protected:
    sp<StreamSourceAppProxy> mAppProxy; // application proxy for the android buffer queue source

    // overridden from GenericMediaPlayer
    virtual void onPrepare();

    Mutex mAppProxyLock;

private:
    DISALLOW_EVIL_CONSTRUCTORS(StreamPlayer);
};

} // namespace android


//--------------------------------------------------------------------------------------------------
/*
 * xxx_l functions are called with a lock on the CAudioPlayer mObject
 */
extern void android_StreamPlayer_realize_l(CAudioPlayer *ap, const notif_cbf_t cbf,
        void* notifUser);
extern void android_StreamPlayer_enqueue_l(CAudioPlayer *ap,
        SLuint32 bufferId, SLuint32 length, SLuint32 event, void *pData);
extern void android_StreamPlayer_clear_l(CAudioPlayer *ap);
