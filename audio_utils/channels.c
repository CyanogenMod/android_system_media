/*
 * Copyright (C) 2014 The Android Open Source Project
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

#include <string.h>
#include <audio_utils/channels.h>
#include "private/private.h"

/* Channel expands (adds zeroes to audio frame end) from an input buffer to an output buffer.
 * See expand_channels() function below for parameter definitions.
 *
 * Move from back to front so that the conversion can be done in-place
 * i.e. in_buff == out_buff
 * NOTE: num_in_bytes must be a multiple of in_buff_channels * in_buff_sample_size.
 */
#define EXPAND_CHANNELS(in_buff, in_buff_chans, out_buff, out_buff_chans, num_in_bytes, zero) \
{ \
    size_t num_in_samples = num_in_bytes / sizeof(*in_buff); \
    size_t num_out_samples = (num_in_samples * out_buff_chans) / in_buff_chans; \
    typeof(out_buff) dst_ptr = out_buff + num_out_samples - 1; \
    size_t src_index; \
    typeof(in_buff) src_ptr = in_buff + num_in_samples - 1; \
    size_t num_zero_chans = out_buff_chans - in_buff_chans; \
    for (src_index = 0; src_index < num_in_samples; src_index += in_buff_chans) { \
        size_t dst_offset; \
        for (dst_offset = 0; dst_offset < num_zero_chans; dst_offset++) { \
            *dst_ptr-- = zero; \
        } \
        for (; dst_offset < out_buff_chans; dst_offset++) { \
            *dst_ptr-- = *src_ptr--; \
        } \
    } \
    /* return number of *bytes* generated */ \
    return num_out_samples * sizeof(*out_buff); \
}

/* Channel contracts (removes from audio frame end) from an input buffer to an output buffer.
 * See contract_channels() function below for parameter definitions.
 *
 * Move from front to back so that the conversion can be done in-place
 * i.e. in_buff == out_buff
 * NOTE: num_in_bytes must be a multiple of in_buff_channels * in_buff_sample_size.
 */
#define CONTRACT_CHANNELS(in_buff, in_buff_chans, out_buff, out_buff_chans, num_in_bytes) \
{ \
    size_t num_in_samples = num_in_bytes / sizeof(*in_buff); \
    size_t num_out_samples = (num_in_samples * out_buff_chans) / in_buff_chans; \
    size_t num_skip_samples = in_buff_chans - out_buff_chans; \
    typeof(out_buff) dst_ptr = out_buff; \
    typeof(in_buff) src_ptr = in_buff; \
    size_t src_index; \
    for (src_index = 0; src_index < num_in_samples; src_index += in_buff_chans) { \
        size_t dst_offset; \
        for (dst_offset = 0; dst_offset < out_buff_chans; dst_offset++) { \
            *dst_ptr++ = *src_ptr++; \
        } \
        src_ptr += num_skip_samples; \
    } \
    /* return number of *bytes* generated */ \
    return num_out_samples * sizeof(*out_buff); \
}

/*
 * Convert a buffer of N-channel, interleaved samples to M-channel
 * (where N > M).
 *   in_buff points to the buffer of samples
 *   in_buff_channels Specifies the number of channels in the input buffer.
 *   out_buff points to the buffer to receive converted samples.
 *   out_buff_channels Specifies the number of channels in the output buffer.
 *   sample_size_in_bytes Specifies the number of bytes per sample.
 *   num_in_bytes size of input buffer in BYTES
 * returns
 *   the number of BYTES of output data.
 * NOTE
 *   channels > M are thrown away.
 *   The out and sums buffers must either be completely separate (non-overlapping), or
 *   they must both start at the same address. Partially overlapping buffers are not supported.
 */
static size_t contract_channels(const void* in_buff, size_t in_buff_chans,
                                void* out_buff, size_t out_buff_chans,
                                unsigned sample_size_in_bytes, size_t num_in_bytes)
{
    switch (sample_size_in_bytes) {
    case 1:
        CONTRACT_CHANNELS((const uint8_t*)in_buff, in_buff_chans,
                          (uint8_t*)out_buff, out_buff_chans,
                          num_in_bytes);
        // returns in macro
    case 2:
        CONTRACT_CHANNELS((const int16_t*)in_buff, in_buff_chans,
                          (int16_t*)out_buff, out_buff_chans,
                          num_in_bytes);
        // returns in macro
    case 3:
        CONTRACT_CHANNELS((const uint8x3_t*)in_buff, in_buff_chans,
                          (uint8x3_t*)out_buff, out_buff_chans,
                          num_in_bytes);
        // returns in macro
    case 4:
        CONTRACT_CHANNELS((const int32_t*)in_buff, in_buff_chans,
                          (int32_t*)out_buff, out_buff_chans,
                          num_in_bytes);
        // returns in macro
    default:
        return 0;
    }
}

/*
 * Convert a buffer of N-channel, interleaved samples to M-channel
 * (where N < M).
 *   in_buff points to the buffer of samples
 *   in_buff_channels Specifies the number of channels in the input buffer.
 *   out_buff points to the buffer to receive converted samples.
 *   out_buff_channels Specifies the number of channels in the output buffer.
 *   sample_size_in_bytes Specifies the number of bytes per sample.
 *   num_in_bytes size of input buffer in BYTES
 * returns
 *   the number of BYTES of output data.
 * NOTE
 *   channels > N are filled with silence.
 *   The out and sums buffers must either be completely separate (non-overlapping), or
 *   they must both start at the same address. Partially overlapping buffers are not supported.
 */
static size_t expand_channels(const void* in_buff, size_t in_buff_chans,
                              void* out_buff, size_t out_buff_chans,
                              unsigned sample_size_in_bytes, size_t num_in_bytes)
{
    static const uint8x3_t packed24_zero; /* zero 24 bit sample */

    switch (sample_size_in_bytes) {
    case 1:
        EXPAND_CHANNELS((const uint8_t*)in_buff, in_buff_chans,
                        (uint8_t*)out_buff, out_buff_chans,
                        num_in_bytes, 0);
        // returns in macro
    case 2:
        EXPAND_CHANNELS((const int16_t*)in_buff, in_buff_chans,
                        (int16_t*)out_buff, out_buff_chans,
                        num_in_bytes, 0);
        // returns in macro
    case 3:
        EXPAND_CHANNELS((const uint8x3_t*)in_buff, in_buff_chans,
                        (uint8x3_t*)out_buff, out_buff_chans,
                        num_in_bytes, packed24_zero);
        // returns in macro
    case 4:
        EXPAND_CHANNELS((const int32_t*)in_buff, in_buff_chans,
                        (int32_t*)out_buff, out_buff_chans,
                        num_in_bytes, 0);
        // returns in macro
    default:
        return 0;
    }
}

size_t adjust_channels(const void* in_buff, size_t in_buff_chans,
                       void* out_buff, size_t out_buff_chans,
                       unsigned sample_size_in_bytes, size_t num_in_bytes)
{
    if (out_buff_chans > in_buff_chans) {
        return expand_channels(in_buff, in_buff_chans, out_buff,  out_buff_chans,
                               sample_size_in_bytes, num_in_bytes);
    } else if (out_buff_chans < in_buff_chans) {
        return contract_channels(in_buff, in_buff_chans, out_buff,  out_buff_chans,
                                 sample_size_in_bytes, num_in_bytes);
    } else if (in_buff != out_buff) {
        memcpy(out_buff, in_buff, num_in_bytes);
    }

    return num_in_bytes;
}
