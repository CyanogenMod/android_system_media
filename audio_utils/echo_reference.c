/*
** Copyright 2011, The Android Open-Source Project
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
**     http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
*/

//#define LOG_NDEBUG 0
#define LOG_TAG "echo_reference"

#include <errno.h>
#include <stdlib.h>
#include <pthread.h>
#include <cutils/log.h>
#include <system/audio.h>
#include <audio_utils/resampler.h>
#include <audio_utils/echo_reference.h>

// echo reference state: bit field indicating if read, write or both are active.
enum state {
    ECHOREF_IDLE = 0x00,        // idle
    ECHOREF_READING = 0x01,     // reading is active
    ECHOREF_WRITING = 0x02      // writing is active
};

struct echo_reference {
    struct echo_reference_itfe itfe;
    int status;                     // init status
    uint32_t state;                 // active state: reading, writing or both
    audio_format_t rd_format;       // read sample format
    uint32_t rd_channel_count;      // read number of channels
    uint32_t rd_sampling_rate;      // read sampling rate in Hz
    size_t rd_frame_size;           // read frame size (bytes per sample)
    audio_format_t wr_format;       // write sample format
    uint32_t wr_channel_count;      // write number of channels
    uint32_t wr_sampling_rate;      // write sampling rate in Hz
    size_t wr_frame_size;           // write frame size (bytes per sample)
    void *buffer;                   // main buffer
    size_t buf_size;                // main buffer size in frames
    size_t frames_in;               // number of frames in main buffer
    void *wr_buf;                   // buffer for input conversions
    size_t wr_buf_size;             // size of conversion buffer in frames
    size_t wr_frames_in;            // number of frames in conversion buffer
    void *wr_src_buf;               // resampler input buf (either wr_buf or buffer used by write())
    struct timespec wr_render_time; // latest render time indicated by write()
                                    // default ALSA gettimeofday() format
    int32_t  playback_delay;        // playback buffer delay indicated by last write()
    pthread_mutex_t lock;                      // mutex protecting read/write concurrency
    pthread_cond_t cond;                       // condition signaled when data is ready to read
    struct resampler_itfe *down_sampler;       // input resampler
    struct resampler_buffer_provider provider; // resampler buffer provider
};


int echo_reference_get_next_buffer(struct resampler_buffer_provider *buffer_provider,
                                   struct resampler_buffer* buffer)
{
    struct echo_reference *er;

    if (buffer_provider == NULL) {
        return -EINVAL;
    }

    er = (struct echo_reference *)((char *)buffer_provider -
                                      offsetof(struct echo_reference, provider));

    if (er->wr_src_buf == NULL || er->wr_frames_in == 0) {
        buffer->raw = NULL;
        buffer->frame_count = 0;
        return -ENODATA;
    }

    buffer->frame_count = (buffer->frame_count > er->wr_frames_in) ? er->wr_frames_in : buffer->frame_count;
    // this is er->rd_channel_count here as we resample after stereo to mono conversion if any
    buffer->i16 = (int16_t *)er->wr_src_buf + (er->wr_buf_size - er->wr_frames_in) * er->rd_channel_count;

    return 0;
}

void echo_reference_release_buffer(struct resampler_buffer_provider *buffer_provider,
                                  struct resampler_buffer* buffer)
{
    struct echo_reference *er;

    if (buffer_provider == NULL) {
        return;
    }

    er = (struct echo_reference *)((char *)buffer_provider -
                                      offsetof(struct echo_reference, provider));

    er->wr_frames_in -= buffer->frame_count;
}

static void echo_reference_reset_l(struct echo_reference *er)
{
    LOGV("echo_reference_reset_l()");
    free(er->buffer);
    er->buffer = NULL;
    er->buf_size = 0;
    er->frames_in = 0;
    free(er->wr_buf);
    er->wr_buf = NULL;
    er->wr_buf_size = 0;
    er->wr_render_time.tv_sec = 0;
    er->wr_render_time.tv_nsec = 0;
}

static int echo_reference_write(struct echo_reference_itfe *echo_reference,
                         struct echo_reference_buffer *buffer)
{
    struct echo_reference *er = (struct echo_reference *)echo_reference;
    int status = 0;

    if (er == NULL) {
        return -EINVAL;
    }

    pthread_mutex_lock(&er->lock);

    if (buffer == NULL) {
        LOGV("echo_reference_write() stop write");
        er->state &= ~ECHOREF_WRITING;
        echo_reference_reset_l(er);
        goto exit;
    }

    LOGV("echo_reference_write() START trying to write %d frames", buffer->frame_count);
    LOGV("echo_reference_write() playbackTimestamp:[%d].[%d], er->playback_delay:[%d]",
            (int)buffer->time_stamp.tv_sec,
            (int)buffer->time_stamp.tv_nsec, er->playback_delay);

    //LOGV("echo_reference_write() %d frames", buffer->frame_count);
    // discard writes until a valid time stamp is provided.

    if ((buffer->time_stamp.tv_sec == 0) && (buffer->time_stamp.tv_nsec == 0) &&
        (er->wr_render_time.tv_sec == 0) && (er->wr_render_time.tv_nsec == 0)) {
        goto exit;
    }

    if ((er->state & ECHOREF_WRITING) == 0) {
        LOGV("echo_reference_write() start write");
        if (er->down_sampler != NULL) {
            er->down_sampler->reset(er->down_sampler);
        }
        er->state |= ECHOREF_WRITING;
    }

    if ((er->state & ECHOREF_READING) == 0) {
        goto exit;
    }

    er->wr_render_time.tv_sec  = buffer->time_stamp.tv_sec;
    er->wr_render_time.tv_nsec = buffer->time_stamp.tv_nsec;

    er->playback_delay = buffer->delay_ns;

    void *srcBuf;
    size_t inFrames;
    // do stereo to mono and down sampling if necessary
    if (er->rd_channel_count != er->wr_channel_count ||
            er->rd_sampling_rate != er->wr_sampling_rate) {
        if (er->wr_buf_size < buffer->frame_count) {
            er->wr_buf_size = buffer->frame_count;
            //max buffer size is normally function of read sampling rate but as write sampling rate
            //is always more than read sampling rate this works
            er->wr_buf = realloc(er->wr_buf, er->wr_buf_size * er->rd_frame_size);
        }

        inFrames = buffer->frame_count;
        if (er->rd_channel_count != er->wr_channel_count) {
            // must be stereo to mono
            int16_t *src16 = (int16_t *)buffer->raw;
            int16_t *dst16 = (int16_t *)er->wr_buf;
            size_t frames = buffer->frame_count;
            while (frames--) {
                *dst16++ = (int16_t)(((int32_t)*src16 + (int32_t)*(src16 + 1)) >> 1);
                src16 += 2;
            }
        }
        if (er->wr_sampling_rate != er->rd_sampling_rate) {
            if (er->down_sampler == NULL) {
                int rc;
                LOGV("echo_reference_write() new ReSampler(%d, %d)",
                      er->wr_sampling_rate, er->rd_sampling_rate);
                er->provider.get_next_buffer = echo_reference_get_next_buffer;
                er->provider.release_buffer = echo_reference_release_buffer;
                rc = create_resampler(er->wr_sampling_rate,
                                 er->rd_sampling_rate,
                                 er->rd_channel_count,
                                 RESAMPLER_QUALITY_VOIP,
                                 &er->provider,
                                 &er->down_sampler);
                if (rc != 0) {
                    er->down_sampler = NULL;
                    LOGV("echo_reference_write() failure to create resampler %d", rc);
                    status = -ENODEV;
                    goto exit;
                }
            }
            // er->wr_src_buf and er->wr_frames_in are used by getNexBuffer() called by the resampler
            // to get new frames
            if (er->rd_channel_count != er->wr_channel_count) {
                er->wr_src_buf = er->wr_buf;
            } else {
                er->wr_src_buf = buffer->raw;
            }
            er->wr_frames_in = buffer->frame_count;
            // inFrames is always more than we need here to get frames remaining from previous runs
            // inFrames is updated by resample() with the number of frames produced
            LOGV("echo_reference_write() ReSampling(%d, %d)",
                  er->wr_sampling_rate, er->rd_sampling_rate);
            er->down_sampler->resample_from_provider(er->down_sampler,
                                                     (int16_t *)er->wr_buf, &inFrames);
            LOGV_IF(er->wr_frames_in != 0,
                    "echo_reference_write() er->wr_frames_in not 0 (%d) after resampler",
                    er->wr_frames_in);
        }
        srcBuf = er->wr_buf;
    } else {
        inFrames = buffer->frame_count;
        srcBuf = buffer->raw;
    }

    if (er->frames_in + inFrames > er->buf_size) {
        LOGV("echo_reference_write() increasing buffer size from %d to %d",
                er->buf_size, er->frames_in + inFrames);
                er->buf_size = er->frames_in + inFrames;
                er->buffer = realloc(er->buffer, er->buf_size * er->rd_frame_size);
    }
    memcpy((char *)er->buffer + er->frames_in * er->rd_frame_size,
           srcBuf,
           inFrames * er->rd_frame_size);
    er->frames_in += inFrames;

    LOGV("EchoReference::write_log() inFrames:[%d], mFramesInOld:[%d], "\
         "mFramesInNew:[%d], er->buf_size:[%d], er->wr_render_time:[%d].[%d],"
         "er->playback_delay:[%d]",
         inFrames, er->frames_in - inFrames, er->frames_in, er->buf_size,
         (int)er->wr_render_time.tv_sec,
         (int)er->wr_render_time.tv_nsec, er->playback_delay);

    pthread_cond_signal(&er->cond);
exit:
    pthread_mutex_unlock(&er->lock);
    LOGV("echo_reference_write() END");
    return status;
}

#define MIN_DELAY_UPDATE_NS 62500 // delay jump threshold to update ref buffer
                                  // 0.5 samples at 8kHz in nsecs


static int echo_reference_read(struct echo_reference_itfe *echo_reference,
                         struct echo_reference_buffer *buffer)
{
    struct echo_reference *er = (struct echo_reference *)echo_reference;

    if (er == NULL) {
        return -EINVAL;
    }

    pthread_mutex_lock(&er->lock);

    if (buffer == NULL) {
        LOGV("EchoReference::read() stop read");
        er->state &= ~ECHOREF_READING;
        goto exit;
    }

    LOGV("EchoReference::read() START, delayCapture:[%d],er->frames_in:[%d],buffer->frame_count:[%d]",
    buffer->delay_ns, er->frames_in, buffer->frame_count);

    if ((er->state & ECHOREF_READING) == 0) {
        LOGV("EchoReference::read() start read");
        echo_reference_reset_l(er);
        er->state |= ECHOREF_READING;
    }

    if ((er->state & ECHOREF_WRITING) == 0) {
        memset(buffer->raw, 0, er->rd_frame_size * buffer->frame_count);
        buffer->delay_ns = 0;
        goto exit;
    }

//    LOGV("EchoReference::read() %d frames", buffer->frame_count);

    // allow some time for new frames to arrive if not enough frames are ready for read
    if (er->frames_in < buffer->frame_count) {
        uint32_t timeoutMs = (uint32_t)((1000 * buffer->frame_count) / er->rd_sampling_rate / 2);
        struct timespec ts;

        ts.tv_sec  = timeoutMs/1000;
        ts.tv_nsec = timeoutMs%1000;
        pthread_cond_timedwait_relative_np(&er->cond, &er->lock, &ts);

        if (er->frames_in < buffer->frame_count) {
            LOGV("EchoReference::read() waited %d ms but still not enough frames"\
                 " er->frames_in: %d, buffer->frame_count = %d",
                timeoutMs, er->frames_in, buffer->frame_count);
            buffer->frame_count = er->frames_in;
        }
    }

    int64_t timeDiff;
    struct timespec tmp;

    if ((er->wr_render_time.tv_sec == 0 && er->wr_render_time.tv_nsec == 0) ||
        (buffer->time_stamp.tv_sec == 0 && buffer->time_stamp.tv_nsec == 0)) {
        LOGV("read: NEW:timestamp is zero---------setting timeDiff = 0, "\
             "not updating delay this time");
        timeDiff = 0;
    } else {
        if (buffer->time_stamp.tv_nsec < er->wr_render_time.tv_nsec) {
            tmp.tv_sec = buffer->time_stamp.tv_sec - er->wr_render_time.tv_sec - 1;
            tmp.tv_nsec = 1000000000 + buffer->time_stamp.tv_nsec - er->wr_render_time.tv_nsec;
        } else {
            tmp.tv_sec = buffer->time_stamp.tv_sec - er->wr_render_time.tv_sec;
            tmp.tv_nsec = buffer->time_stamp.tv_nsec - er->wr_render_time.tv_nsec;
        }
        timeDiff = (((int64_t)tmp.tv_sec * 1000000000 + tmp.tv_nsec));

        int64_t expectedDelayNs =  er->playback_delay + buffer->delay_ns - timeDiff;

        LOGV("expectedDelayNs[%lld] =  er->playback_delay[%d] + delayCapture[%d] - timeDiff[%lld]",
        expectedDelayNs, er->playback_delay, buffer->delay_ns, timeDiff);

        if (expectedDelayNs > 0) {
            int64_t delayNs = ((int64_t)er->frames_in * 1000000000) / er->rd_sampling_rate;

            delayNs -= expectedDelayNs;

            if (abs(delayNs) >= MIN_DELAY_UPDATE_NS) {
                if (delayNs < 0) {
                    size_t previousFrameIn = er->frames_in;
                    er->frames_in = (expectedDelayNs * er->rd_sampling_rate)/1000000000;
                    int    offset = er->frames_in - previousFrameIn;
                    LOGV("EchoReference::readlog: delayNs = NEGATIVE and ENOUGH : "\
                         "setting %d frames to zero er->frames_in: %d, previousFrameIn = %d",
                         offset, er->frames_in, previousFrameIn);

                    if (er->frames_in > er->buf_size) {
                        er->buf_size = er->frames_in;
                        er->buffer  = realloc(er->buffer, er->frames_in * er->rd_frame_size);
                        LOGV("EchoReference::read: increasing buffer size to %d", er->buf_size);
                    }

                    if (offset > 0)
                        memset((char *)er->buffer + previousFrameIn * er->rd_frame_size,
                               0, offset * er->rd_frame_size);
                } else {
                    size_t  previousFrameIn = er->frames_in;
                    int     framesInInt = (int)(((int64_t)expectedDelayNs *
                                           (int64_t)er->rd_sampling_rate)/1000000000);
                    int     offset = previousFrameIn - framesInInt;

                    LOGV("EchoReference::readlog: delayNs = POSITIVE/ENOUGH :previousFrameIn: %d,"\
                         "framesInInt: [%d], offset:[%d], buffer->frame_count:[%d]",
                         previousFrameIn, framesInInt, offset, buffer->frame_count);

                    if (framesInInt < (int)buffer->frame_count) {
                        if (framesInInt > 0) {
                            memset((char *)er->buffer + framesInInt * er->rd_frame_size,
                                   0, (buffer->frame_count-framesInInt) * er->rd_frame_size);
                            LOGV("EchoReference::read: pushing [%d] zeros into ref buffer",
                                 (buffer->frame_count-framesInInt));
                        } else {
                            LOGV("framesInInt = %d", framesInInt);
                        }
                        framesInInt = buffer->frame_count;
                    } else {
                        if (offset > 0) {
                            memcpy(er->buffer, (char *)er->buffer + (offset * er->rd_frame_size),
                                   framesInInt * er->rd_frame_size);
                            LOGV("EchoReference::read: shifting ref buffer by [%d]",framesInInt);
                        }
                    }
                    er->frames_in = (size_t)framesInInt;
                }
            } else {
                LOGV("EchoReference::read: NOT ENOUGH samples to update %lld", delayNs);
            }
        } else {
            LOGV("NEGATIVE expectedDelayNs[%lld] =  "\
                 "er->playback_delay[%d] + delayCapture[%d] - timeDiff[%lld]",
                 expectedDelayNs, er->playback_delay, buffer->delay_ns, timeDiff);
        }
    }

    memcpy(buffer->raw,
           (char *)er->buffer,
           buffer->frame_count * er->rd_frame_size);

    er->frames_in -= buffer->frame_count;
    memcpy(er->buffer,
           (char *)er->buffer + buffer->frame_count * er->rd_frame_size,
           er->frames_in * er->rd_frame_size);

    // As the reference buffer is now time aligned to the microphone signal there is a zero delay
    buffer->delay_ns = 0;

    LOGV("EchoReference::read() END %d frames, total frames in %d",
          buffer->frame_count, er->frames_in);

    pthread_cond_signal(&er->cond);

exit:
    pthread_mutex_unlock(&er->lock);
    return 0;
}


int create_echo_reference(audio_format_t rdFormat,
                            uint32_t rdChannelCount,
                            uint32_t rdSamplingRate,
                            audio_format_t wrFormat,
                            uint32_t wrChannelCount,
                            uint32_t wrSamplingRate,
                            struct echo_reference_itfe **echo_reference)
{
    struct echo_reference *er;

    LOGV("create_echo_reference()");

    if (echo_reference == NULL) {
        return -EINVAL;
    }

    *echo_reference = NULL;

    if (rdFormat != AUDIO_FORMAT_PCM_16_BIT ||
            rdFormat != wrFormat) {
        LOGW("create_echo_reference bad format rd %d, wr %d", rdFormat, wrFormat);
        return -EINVAL;
    }
    if ((rdChannelCount != 1 && rdChannelCount != 2) ||
            wrChannelCount != 2) {
        LOGW("create_echo_reference bad channel count rd %d, wr %d", rdChannelCount, wrChannelCount);
        return -EINVAL;
    }

    if (wrSamplingRate < rdSamplingRate) {
        LOGW("create_echo_reference bad smp rate rd %d, wr %d", rdSamplingRate, wrSamplingRate);
        return -EINVAL;
    }

    er = (struct echo_reference *)calloc(1, sizeof(struct echo_reference));

    er->itfe.read = echo_reference_read;
    er->itfe.write = echo_reference_write;

    er->state = ECHOREF_IDLE;
    er->rd_format = rdFormat;
    er->rd_channel_count = rdChannelCount;
    er->rd_sampling_rate = rdSamplingRate;
    er->wr_format = wrFormat;
    er->wr_channel_count = wrChannelCount;
    er->wr_sampling_rate = wrSamplingRate;
    er->rd_frame_size = audio_bytes_per_sample(rdFormat) * rdChannelCount;
    er->wr_frame_size = audio_bytes_per_sample(wrFormat) * wrChannelCount;
    *echo_reference = &er->itfe;
    return 0;
}

void release_echo_reference(struct echo_reference_itfe *echo_reference) {
    struct echo_reference *er = (struct echo_reference *)echo_reference;

    if (er == NULL) {
        return;
    }

    LOGV("EchoReference dstor");
    echo_reference_reset_l(er);
    if (er->down_sampler != NULL) {
        release_resampler(er->down_sampler);
    }
    free(er);
}

