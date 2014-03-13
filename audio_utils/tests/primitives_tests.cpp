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

//#define LOG_NDEBUG 0
#define LOG_TAG "audio_utils_primitives_tests"

#include <vector>
#include <cutils/log.h>
#include <gtest/gtest.h>
#include <audio_utils/primitives.h>
#include <audio_utils/format.h>

#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))

static const int32_t lim16pos = (1 << 15) - 1;
static const int32_t lim16neg = -(1 << 15);
static const int32_t lim24pos = (1 << 23) - 1;
static const int32_t lim24neg = -(1 << 23);

inline void testClamp16(float f)
{
    int16_t ival = clamp16FromFloat(f / (1 << 15));

    // test clamping
    ALOGV("clamp16(%f) = %d\n", f, ival);
    if (f > lim16pos) {
        EXPECT_EQ(ival, lim16pos);
    } else if (f < lim16neg) {
        EXPECT_EQ(ival, lim16neg);
    }

    // if in range, make sure round trip clamp and conversion is correct.
    if (f < lim16pos - 1. && f > lim16neg + 1.) {
        int ival2 = clamp16FromFloat(float_from_i16(ival));
        int diff = abs(ival - ival2);
        EXPECT_LE(diff, 1);
    }
}

inline void testClamp24(float f)
{
    int32_t ival = clamp24_from_float(f / (1 << 23));

    // test clamping
    ALOGV("clamp24(%f) = %d\n", f, ival);
    if (f > lim24pos) {
        EXPECT_EQ(ival, lim24pos);
    } else if (f < lim24neg) {
        EXPECT_EQ(ival, lim24neg);
    }

    // if in range, make sure round trip clamp and conversion is correct.
    if (f < lim24pos - 1. && f > lim24neg + 1.) {
        int ival2 = clamp24_from_float(float_from_q8_23(ival));
        int diff = abs(ival - ival2);
        EXPECT_LE(diff, 1);
    }
}

TEST(audio_utils_primitives, clamp) {
    static const float testArray[] = {
            -NAN, -INFINITY,
            -1.e20, -32768., 63.9,
            -3.5, -3.4, -2.5, 2.4, -1.5, -1.4, -0.5, -0.2, 0., 0.2, 0.5, 0.8,
            1.4, 1.5, 1.8, 2.4, 2.5, 2.6, 3.4, 3.5,
            32767., 32768., 1.e20,
            INFINITY, NAN };

    for (size_t i = 0; i < ARRAY_SIZE(testArray); ++i) {
        testClamp16(testArray[i]);
    }
    for (size_t i = 0; i < ARRAY_SIZE(testArray); ++i) {
        testClamp24(testArray[i]);
    }
}

TEST(audio_utils_primitives, memcpy) {
    // test round-trip.
    int16_t *i16ary = new int16_t[65536];
    int32_t *i32ary = new int32_t[65536];
    float *fary = new float[65536];
    uint8_t *pary = new uint8_t[65536*3];

    for (size_t i = 0; i < 65536; ++i) {
        i16ary[i] = i; // (caution) overflow to neg.
    }

    // do round-trip testing i16 and float
    memcpy_to_float_from_i16(fary, i16ary, 65536);
    memset(i16ary, 0, 65536 * sizeof(i16ary[0]));

    memcpy_to_i16_from_float(i16ary, fary, 65536);
    memset(fary, 0, 65536 * sizeof(fary[0]));

    for (size_t i = 0; i < 65536; ++i) {
        EXPECT_EQ(((unsigned)i16ary[i] & 0xffff), (unsigned)i);
    }

    // TODO make a template case for the following?

    // do round-trip testing p24 to i16 and float
    memcpy_to_p24_from_i16(pary, i16ary, 65536);
    memset(i16ary, 0, 65536 * sizeof(i16ary[0]));

    // check an intermediate format at a position(???)
#if 0
    printf("pary[%d].0 = %u  pary[%d].1 = %u  pary[%d].2 = %u\n",
            1025, (unsigned) pary[1025*3],
            1025, (unsigned) pary[1025*3+1],
            1025, (unsigned) pary[1025*3+2]
            );
#endif

    memcpy_to_float_from_p24(fary, pary, 65536);
    memset(pary, 0, 65536 * 3 * sizeof(pary[0]));

    memcpy_to_p24_from_float(pary, fary, 65536);
    memset(fary, 0, 65536 * sizeof(fary[0]));

    memcpy_to_i16_from_p24(i16ary, pary, 65536);
    memset(pary, 0, 65536 * 3 * sizeof(pary[0]));

    for (size_t i = 0; i < 65536; ++i) {
        EXPECT_EQ(((unsigned)i16ary[i] & 0xffff), (unsigned)i);
    }

    // do round-trip testing q8_23 to i16 and float
    memcpy_to_q8_23_from_i16(i32ary, i16ary, 65536);
    memset(i16ary, 0, 65536 * sizeof(i16ary[0]));

    memcpy_to_float_from_q8_23(fary, i32ary, 65536);
    memset(i32ary, 0, 65536 * sizeof(i32ary[0]));

    memcpy_to_q8_23_from_float_with_clamp(i32ary, fary, 65536);
    memset(fary, 0, 65536 * sizeof(fary[0]));

    memcpy_to_i16_from_q8_23(i16ary, i32ary, 65536);
    memset(i32ary, 0, 65536 * sizeof(i32ary[0]));

    for (size_t i = 0; i < 65536; ++i) {
        EXPECT_EQ(((unsigned)i16ary[i] & 0xffff), (unsigned)i);
    }

    // do round-trip testing i32 to i16 and float
    memcpy_to_i32_from_i16(i32ary, i16ary, 65536);
    memset(i16ary, 0, 65536 * sizeof(i16ary[0]));

    memcpy_to_float_from_i32(fary, i32ary, 65536);
    memset(i32ary, 0, 65536 * sizeof(i32ary[0]));

    memcpy_to_i32_from_float(i32ary, fary, 65536);
    memset(fary, 0, 65536 * sizeof(fary[0]));

    memcpy_to_i16_from_i32(i16ary, i32ary, 65536);
    memset(i32ary, 0, 65536 * sizeof(i32ary[0]));

    for (size_t i = 0; i < 65536; ++i) {
        EXPECT_EQ(((unsigned)i16ary[i] & 0xffff), (unsigned)i);
    }

    delete[] i16ary;
    delete[] i32ary;
    delete[] fary;
    delete[] pary;
}
