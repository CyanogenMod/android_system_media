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

#ifndef ANDROID_AUDIO_FORMAT_H
#define ANDROID_AUDIO_FORMAT_H

#include <stdint.h>
#include <sys/cdefs.h>
#include <system/audio.h>

__BEGIN_DECLS

/* Copy buffers with conversion between buffer sample formats.
 *
 *  dst        Destination buffer
 *  dst_format Destination buffer format
 *  src        Source buffer
 *  src_format Source buffer format
 *  count      Number of samples to copy
 *
 * Permitted format types for dst_format and src_format are as follows:
 * AUDIO_FORMAT_PCM_16_BIT
 * AUDIO_FORMAT_PCM_24_BIT_PACKED
 * AUDIO_FORMAT_PCM_FLOAT
 *
 * The destination and source buffers must be completely separate if the destination
 * format size is larger than the source format size. These routines call functions
 * in primitives.h, so descriptions of detailed behavior can be reviewed there.
 *
 * Logs a fatal error if dst or src format is not one of the permitted types.
 */
void memcpy_by_audio_format(void *dst, audio_format_t dst_format,
        void *src, audio_format_t src_format, size_t count);

__END_DECLS

#endif  // ANDROID_AUDIO_FORMAT_H
