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


//--------------------------------------------------------------------------------------------------
namespace android {

class AudioSfDecoder : public GenericPlayer
{
public:

    AudioSfDecoder(const AudioPlayback_Parameters* params);
    virtual ~AudioSfDecoder();

    // overridden from GenericPlayer
    virtual void play();

    void startPrefetch_async();

protected:

    enum CacheStatus {
            kStatusUnknown = -1,
            kStatusEmpty = 0,
            kStatusLow,
            kStatusIntermediate,
            kStatusEnough,
            kStatusHigh
        };

    enum {
        kEventPrefetchStatusChange    = 'prsc',
        kEventPrefetchFillLevelUpdate = 'pflu',
        kEventEndOfStream             = 'eos',
    };

    enum {
        kWhatDecode     = 'deco',
        kWhatRender     = 'rend',
        kWhatCheckCache = 'cach',
    };

    // Async event handlers (called from the AudioSfDecoder's event loop)
    void onDecode();
    void onCheckCache(const sp<AMessage> &msg);
    virtual void onRender();

    // Async event handlers (called from GenericPlayer's event loop)
    virtual void onPrepare();
    virtual void onPlay();
    virtual void onPause();
    virtual void onSeek(const sp<AMessage> &msg);
    virtual void onLoop(const sp<AMessage> &msg);

    // overridden from GenericPlayer
    virtual void onNotify(const sp<AMessage> &msg);
    virtual void onMessageReceived(const sp<AMessage> &msg);

    // to be implemented by subclasses of AudioSfDecoder to do something with the audio samples
    virtual void createAudioSink() = 0;
    virtual void updateAudioSink() = 0;
    virtual void startAudioSink() = 0;
    virtual void pauseAudioSink() = 0;

    sp<DataSource> mDataSource;
    sp<MediaSource> mAudioSource;

    // negative values indicate invalid value
    int64_t mBitrate;  // in bits/sec
    int32_t mNumChannels;
    int32_t mSampleRateHz;
    int64_t mDurationUsec;

    // buffer passed from decoder to renderer
    MediaBuffer *mDecodeBuffer;
    // mutex used to protect the decode buffer
    Mutex       mDecodeBufferLock;


private:

    void notifyStatus();
    void notifyCacheFill();
    void notifyPrepared(status_t prepareRes);

    CacheStatus mCacheStatus;
    int16_t mCacheFill; // cache fill level in permille, signed per OpenSL ES
    int16_t mLastNotifiedCacheFill; // last cache fill level communicated to the listener
    int16_t mCacheFillNotifThreshold; // threshold in cache fill level for cache fill to be reported

    int64_t mTimeDelta;
    int64_t mSeekTimeMsec;
    int64_t mLastDecodedPositionUs;

    // mutex used for seek flag and seek time read/write
    Mutex mSeekLock;

/*
    sp<ALooper> mRenderLooper;
*/

    bool wantPrefetch();
    CacheStatus getCacheRemaining(bool *eos);
    int64_t getPositionUsec();

private:
    DISALLOW_EVIL_CONSTRUCTORS(AudioSfDecoder);

};

} // namespace android
