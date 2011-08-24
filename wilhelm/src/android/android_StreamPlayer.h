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

#include <media/IStreamSource.h>
#include <binder/IServiceManager.h>
#include "android/android_GenericMediaPlayer.h"

// number of SLuint32 fields to store a buffer event message in an item, by mapping each
//   to the item key (SLuint32), the item size (SLuint32), and the item data (mask on SLuint32)
#define NB_BUFFEREVENT_ITEM_FIELDS 3

//--------------------------------------------------------------------------------------------------
namespace android {

//--------------------------------------------------------------------------------------------------
class StreamSourceAppProxy : public BnStreamSource {
public:
    StreamSourceAppProxy(
            const void* user, bool userIsAudioPlayer,
            void *appContext,
            const void *caller,
            const sp<CallbackProtector> &callbackProtector);
    virtual ~StreamSourceAppProxy();

    // store an item structure to indicate a processed buffer
    static const SLuint32 kItemProcessed[NB_BUFFEREVENT_ITEM_FIELDS];

    // IStreamSource implementation
    virtual void setListener(const sp<IStreamListener> &listener);
    virtual void setBuffers(const Vector<sp<IMemory> > &buffers);
    virtual void onBufferAvailable(size_t index);

    // Consumption from ABQ
    void pullFromBuffQueue();

    void receivedCmd_l(IStreamListener::Command cmd, const sp<AMessage> &msg = NULL);
    void receivedBuffer_l(size_t buffIndex, size_t buffLength);

private:
    // for mListener and mAvailableBuffers
    Mutex mLock;
    sp<IStreamListener> mListener;
    // array of shared memory buffers
    Vector<sp<IMemory> > mBuffers;
    // list of available buffers in shared memory, identified by their index
    List<size_t> mAvailableBuffers;

    const void* mUser;
    bool mUserIsAudioPlayer;
    // the Android Buffer Queue from which data is consumed and written to shared memory
    IAndroidBufferQueue* mAndroidBufferQueue;

    void *mAppContext;
    const void *mCaller;

    sp<CallbackProtector> mCallbackProtector;

    DISALLOW_EVIL_CONSTRUCTORS(StreamSourceAppProxy);
};


//--------------------------------------------------------------------------------------------------
class StreamPlayer : public GenericMediaPlayer
{
public:
    StreamPlayer(AudioPlayback_Parameters* params, bool hasVideo);
    virtual ~StreamPlayer();

    // overridden from GenericPlayer
    virtual void onMessageReceived(const sp<AMessage> &msg);

    void registerQueueCallback(
            const void* user, bool userIsAudioPlayer,
            void *context,
            const void *caller);
    void queueRefilled_l();
    void appClear_l();

protected:

    enum {
        // message to asynchronously notify mAppProxy the Android Buffer Queue was refilled
        kWhatQueueRefilled = 'qrfi'
    };

    sp<StreamSourceAppProxy> mAppProxy; // application proxy for the android buffer queue source

    // overridden from GenericMediaPlayer
    virtual void onPrepare();

    void onQueueRefilled();

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
