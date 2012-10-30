/*
 * Copyright (C) 2012 The Android Open Source Project
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

#include <audio_utils/sndfile.h>
#include <stdio.h>
#include <string.h>

struct SNDFILE_ {
    FILE *stream;
    size_t bytesPerFrame;
    size_t remaining;
    SF_INFO info;
};

static unsigned little2u(unsigned char *ptr)
{
    return (ptr[1] << 8) + ptr[0];
}

static unsigned little4u(unsigned char *ptr)
{
    return (ptr[3] << 24) + (ptr[2] << 16) + (ptr[1] << 8) + ptr[0];
}

static int isLittleEndian(void)
{
    static const short one = 1;
    return *((const char *) &one) == 1;
}

static void swab(short *ptr, size_t numToSwap)
{
    while (numToSwap > 0) {
        *ptr = little2u((unsigned char *) ptr);
        --numToSwap;
        ++ptr;
    }
}

SNDFILE *sf_open(const char *path, int mode, SF_INFO *info)
{
    if (path == NULL || mode != SFM_READ || info == NULL)
        return NULL;
    FILE *stream = fopen(path, "rb");
    if (stream == NULL)
        return NULL;
    // don't attempt to parse all valid forms, just the most common one
    unsigned char wav[44];
    size_t actual;
    actual = fread(wav, sizeof(char), sizeof(wav), stream);
    if (actual != sizeof(wav))
        return NULL;
    for (;;) {
        if (memcmp(wav, "RIFF", 4))
            break;
        unsigned riffSize = little4u(&wav[4]);
        if (riffSize < 44)
            break;
        if (memcmp(&wav[8], "WAVEfmt ", 8))
            break;
        unsigned fmtsize = little4u(&wav[16]);
        if (fmtsize != 16)
            break;
        unsigned format = little2u(&wav[20]);
        if (format != 1)    // PCM
            break;
        unsigned channels = little2u(&wav[22]);
        if (channels != 1 && channels != 2)
            break;
        unsigned samplerate = little4u(&wav[24]);
        if (samplerate == 0)
            break;
        // ignore byte rate
        // ignore block alignment
        unsigned bitsPerSample = little2u(&wav[34]);
        if (/*bitsPerSample != 8 &&*/ bitsPerSample != 16)
            break;
        unsigned bytesPerFrame = (bitsPerSample >> 3) * channels;
        if (memcmp(&wav[36], "data", 4))
            break;
        unsigned dataSize = little4u(&wav[40]);
        SNDFILE *handle = (SNDFILE *) malloc(sizeof(SNDFILE));
        handle->stream = stream;
        handle->bytesPerFrame = bytesPerFrame;
        handle->remaining = dataSize / bytesPerFrame;
        handle->info.samplerate = samplerate;
        handle->info.channels = channels;
        handle->info.format = SF_FORMAT_WAV | SF_FORMAT_PCM_16;
        *info = handle->info;
        return handle;
    }
    return NULL;
}

void sf_close(SNDFILE *handle)
{
    if (handle == NULL)
        return;
    (void) fclose(handle->stream);
    handle->stream = NULL;
    handle->remaining = 0;
}

ssize_t sf_readf_short(SNDFILE *handle, short *ptr, size_t desiredFrames)
{
    if (handle == NULL || ptr == NULL || !handle->remaining)
        return 0;
    if (handle->remaining < desiredFrames)
        desiredFrames = handle->remaining;
    size_t desiredBytes = desiredFrames * handle->bytesPerFrame;
    // does not check for numeric overflow
    size_t actualBytes = fread(ptr, sizeof(char), desiredBytes, handle->stream);
    size_t actualFrames = actualBytes / handle->bytesPerFrame;
    handle->remaining -= actualFrames;
    if (!isLittleEndian())
        swab(ptr, actualFrames * handle->info.channels);
    return actualFrames;
}
