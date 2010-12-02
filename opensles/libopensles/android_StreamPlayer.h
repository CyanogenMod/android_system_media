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
// FIXME move to mediaplayer.h
enum media_player_stream_origin {
    MEDIA_PLAYER_STREAM_ORIGIN_INVALID           = 0,
    MEDIA_PLAYER_STREAM_ORIGIN_FILE              = 1 << 0,
    MEDIA_PLAYER_STREAM_ORIGIN_TRANSPORT_STREAM  = 1 << 1
};

typedef struct StreamPlayback_Parameters_struct {
    int streamType;
    int sessionId;
} StreamPlayback_Parameters;

//--------------------------------------------------------------------------------------------------
/*
 * xxx_l functions are called with a lock on the CAudioPlayer mObject
 */
extern void android_StreamPlayer_realize_l(CAudioPlayer *ap);
extern void android_StreamPlayer_destroy(CAudioPlayer *ap);
extern void android_StreamPlayer_setPlayState(CAudioPlayer *ap, SLuint32 playState,
        AndroidObject_state objState);
extern void android_StreamPlayer_registerCallback_l(CAudioPlayer *ap);
extern void android_StreamPlayer_enqueue_l(CAudioPlayer *ap,
        SLuint32 bufferId, SLuint32 length, SLAbufferQueueEvent event, void *pData);
extern void android_StreamPlayer_clear_l(CAudioPlayer *ap);

//--------------------------------------------------------------------------------------------------
namespace android {

class StreamMediaPlayerClient : public BnMediaPlayerClient {
public:
    StreamMediaPlayerClient();
    virtual ~StreamMediaPlayerClient();

    // IMediaPlayerClient implementation
    virtual void notify(int msg, int ext1, int ext2);

};

//--------------------------------------------------------------------------------------------------

class StreamSourceAppProxy : public BnStreamSource {
public:
    StreamSourceAppProxy(slAndroidBufferQueueCallback callback, void *appContext, const void *caller);
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
class StreamPlayer : public AHandler
{
public:
    StreamPlayer(StreamPlayback_Parameters* params);
    virtual ~StreamPlayer();

    void init();

    void appRegisterCallback(slAndroidBufferQueueCallback callback, void *context, const void *caller);
    void appEnqueue(SLuint32 bufferId, SLuint32 length, SLAbufferQueueEvent event, void *pData);
    void appClear();

    void prepare();
    void stop();
    void pause();
    void play();

protected:
    // AHandler implementation
    virtual void onMessageReceived(const sp<AMessage> &msg);

    sp<ALooper> mLooper;

    StreamPlayback_Parameters mPlaybackParams;
    sp<StreamSourceAppProxy> mAppProxy; // application proxy for the stream source
    sp<StreamMediaPlayerClient> mMPClient;
    sp<IMediaPlayer> mPlayer;

    sp<IServiceManager> mServiceManager;
    sp<IBinder> mBinder;
    sp<IMediaPlayerService> mMediaPlayerService;

private:
    Mutex mLock;

    enum {
        kWhatPrepare    = 'prep',
        kWhatNotif      = 'noti',
        kWhatPlay       = 'play',
        kWhatPause      = 'paus',
    };

    // Event handlers
    void onPrepare();
    void onNotif(const sp<AMessage> &msg);
    void onPlay();
    void onPause();
};

} // namespace android
