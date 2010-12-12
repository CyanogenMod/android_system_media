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


typedef struct StreamPlayback_Parameters_struct {
    int streamType;
    int sessionId;
} StreamPlayback_Parameters;


//--------------------------------------------------------------------------------------------------
namespace android {


class StreamSourceAppProxy : public BnStreamSource {
public:
    StreamSourceAppProxy(slAndroidBufferQueueCallback callback, void *appContext,
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

    slAndroidBufferQueueCallback mCallback;
    void *mAppContext;
    const void *mCaller;

    DISALLOW_EVIL_CONSTRUCTORS(StreamSourceAppProxy);
};


//--------------------------------------------------------------------------------------------------
class StreamPlayer : public AVPlayer
{
public:
    StreamPlayer(AudioPlayback_Parameters* params);
    virtual ~StreamPlayer();

    // overridden from AVPlayer
    virtual void init();


    void registerQueueCallback(slAndroidBufferQueueCallback callback, void *context,
            const void *caller);
    void appEnqueue(SLuint32 bufferId, SLuint32 length, SLAbufferQueueEvent event, void *pData);
    void appClear();

protected:
    sp<StreamSourceAppProxy> mAppProxy; // application proxy for the android buffer queue source

    // overridden from AVPlayer
    virtual void onPrepare();

private:
    DISALLOW_EVIL_CONSTRUCTORS(StreamPlayer);
};

} // namespace android


//--------------------------------------------------------------------------------------------------
/*
 * xxx_l functions are called with a lock on the CAudioPlayer mObject
 */
extern void android_StreamPlayer_realize_l(CAudioPlayer *ap);
extern void android_StreamPlayer_destroy(CAudioPlayer *ap);
extern void android_StreamPlayer_androidBufferQueue_registerCallback(
        android::StreamPlayer *splr,
        slAndroidBufferQueueCallback callback, void* context, const void* callerItf);
extern void android_StreamPlayer_enqueue_l(CAudioPlayer *ap,
        SLuint32 bufferId, SLuint32 length, SLAbufferQueueEvent event, void *pData);
extern void android_StreamPlayer_clear_l(CAudioPlayer *ap);
