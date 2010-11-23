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
 * Called with a lock on the CAudioPlayer mObject
 */
extern void android_StreamPlayer_realize_lApObj(CAudioPlayer *ap);

extern void android_StreamPlayer_destroy(CAudioPlayer *ap);

extern void android_StreamPlayer_setPlayState(CAudioPlayer *ap, SLuint32 playState,
        AndroidObject_state objState);

/*
 * Called with a lock on the CAudioPlayer mObject
 */
extern void android_StreamPlayer_registerCallback_lApObj(CAudioPlayer *ap);

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

class StreamSourceAppProxy : public BnStreamSource/*, public BnMediaPlayerClient*/ {
public:
    StreamSourceAppProxy(slAndroidStreamSourceCallback callback, void *appContext);
    virtual ~StreamSourceAppProxy();

    // IStreamSource implementation
    virtual void setListener(const sp<IStreamListener> &listener);
    virtual void setBuffers(const Vector<sp<IMemory> > &buffers);
    virtual void onBufferAvailable(size_t index);

private:
    sp<IStreamListener> mListener;
    Vector<sp<IMemory> > mBuffers;

    slAndroidStreamSourceCallback mCallback;
    void * mAppContext;

    DISALLOW_EVIL_CONSTRUCTORS(StreamSourceAppProxy);
};


//--------------------------------------------------------------------------------------------------
class StreamPlayer : public RefBase
{
public:
    StreamPlayer(StreamPlayback_Parameters* params);
    virtual ~StreamPlayer();

    void useCallbackToApp(slAndroidStreamSourceCallback callback, void *context);

    void stop();
    void pause();
    void play();

protected:
    StreamPlayback_Parameters mPlaybackParams;
    sp<StreamSourceAppProxy> mAppProxy; // application proxy for the stream source
    sp<StreamMediaPlayerClient> mMPClient;
    sp<IMediaPlayer> mPlayer;

    sp<IServiceManager> mServiceManager;
    sp<IBinder> mBinder;
    sp<IMediaPlayerService> mMediaPlayerService;

private:
    Mutex mLock;
};

} // namespace android
