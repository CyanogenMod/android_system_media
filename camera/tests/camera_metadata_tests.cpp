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

#include <errno.h>

#include "gtest/gtest.h"
#include "system/camera_metadata.h"

#include "camera_metadata_tests_fake_vendor.h"

#define EXPECT_NULL(x)     EXPECT_EQ((void*)0, x)
#define EXPECT_NOT_NULL(x) EXPECT_NE((void*)0, x)

#define OK    0
#define ERROR 1
#define NOT_FOUND (-ENOENT)

TEST(camera_metadata, allocate_normal) {
    camera_metadata_t *m = NULL;
    const size_t entry_capacity = 5;
    const size_t data_capacity = 32;

    m = allocate_camera_metadata(entry_capacity, data_capacity);

    EXPECT_NOT_NULL(m);
    EXPECT_EQ((size_t)0, get_camera_metadata_entry_count(m));
    EXPECT_EQ(entry_capacity, get_camera_metadata_entry_capacity(m));
    EXPECT_EQ((size_t)0, get_camera_metadata_data_count(m));
    EXPECT_EQ(data_capacity, get_camera_metadata_data_capacity(m));

    free_camera_metadata(m);
}

TEST(camera_metadata, allocate_nodata) {
    camera_metadata_t *m = NULL;

    m = allocate_camera_metadata(1, 0);

    EXPECT_NOT_NULL(m);
    EXPECT_EQ((size_t)0, get_camera_metadata_entry_count(m));
    EXPECT_EQ((size_t)1, get_camera_metadata_entry_capacity(m));
    EXPECT_EQ((size_t)0, get_camera_metadata_data_count(m));
    EXPECT_EQ((size_t)0, get_camera_metadata_data_capacity(m));

    free_camera_metadata(m);
}

TEST(camera_metadata, allocate_nothing) {
    camera_metadata_t *m = NULL;

    m = allocate_camera_metadata(0, 0);

    EXPECT_NULL(m);
}

TEST(camera_metadata, place_normal) {
    camera_metadata_t *m = NULL;
    void *buf = NULL;

    const size_t entry_capacity = 5;
    const size_t data_capacity = 32;

    size_t buf_size = calculate_camera_metadata_size(entry_capacity,
            data_capacity);

    EXPECT_TRUE(buf_size > 0);

    buf = malloc(buf_size);

    EXPECT_NOT_NULL(buf);

    m = place_camera_metadata(buf, buf_size, entry_capacity, data_capacity);

    EXPECT_EQ(buf, (uint8_t*)m);
    EXPECT_EQ((size_t)0, get_camera_metadata_entry_count(m));
    EXPECT_EQ(entry_capacity, get_camera_metadata_entry_capacity(m));
    EXPECT_EQ((size_t)0, get_camera_metadata_data_count(m));
    EXPECT_EQ(data_capacity, get_camera_metadata_data_capacity(m));

    free(buf);
}

TEST(camera_metadata, place_nospace) {
    camera_metadata_t *m = NULL;
    void *buf = NULL;

    const size_t entry_capacity = 5;
    const size_t data_capacity = 32;

    size_t buf_size = calculate_camera_metadata_size(entry_capacity,
            data_capacity);

    EXPECT_GT(buf_size, (size_t)0);

    buf_size--;

    buf = malloc(buf_size);

    EXPECT_NOT_NULL(buf);

    m = place_camera_metadata(buf, buf_size, entry_capacity, data_capacity);

    EXPECT_NULL(m);

    free(buf);
}

TEST(camera_metadata, place_extraspace) {
    camera_metadata_t *m = NULL;
    uint8_t *buf = NULL;

    const size_t entry_capacity = 5;
    const size_t data_capacity = 32;
    const size_t extra_space = 10;

    size_t buf_size = calculate_camera_metadata_size(entry_capacity,
            data_capacity);

    EXPECT_GT(buf_size, (size_t)0);

    buf_size += extra_space;

    buf = (uint8_t*)malloc(buf_size);

    EXPECT_NOT_NULL(buf);

    m = place_camera_metadata(buf, buf_size, entry_capacity, data_capacity);

    EXPECT_EQ((uint8_t*)m, buf);
    EXPECT_EQ((size_t)0, get_camera_metadata_entry_count(m));
    EXPECT_EQ(entry_capacity, get_camera_metadata_entry_capacity(m));
    EXPECT_EQ((size_t)0, get_camera_metadata_data_count(m));
    EXPECT_EQ(data_capacity, get_camera_metadata_data_capacity(m));
    EXPECT_EQ(buf + buf_size - extra_space, (uint8_t*)m + get_camera_metadata_size(m));

    free(buf);
}

TEST(camera_metadata, get_size) {
    camera_metadata_t *m = NULL;
    const size_t entry_capacity = 5;
    const size_t data_capacity = 32;

    m = allocate_camera_metadata(entry_capacity, data_capacity);

    EXPECT_EQ(calculate_camera_metadata_size(entry_capacity, data_capacity),
            get_camera_metadata_size(m) );

    EXPECT_EQ(calculate_camera_metadata_size(0,0),
            get_camera_metadata_compact_size(m) );

    free_camera_metadata(m);
}

TEST(camera_metadata, add_get_normal) {
    camera_metadata_t *m = NULL;
    const size_t entry_capacity = 5;
    const size_t data_capacity = 80;

    m = allocate_camera_metadata(entry_capacity, data_capacity);

    int result;
    size_t data_used = 0;
    size_t entries_used = 0;

    // INT64

    int64_t exposure_time = 1000000000;
    result = add_camera_metadata_entry(m,
            ANDROID_SENSOR_EXPOSURE_TIME,
            &exposure_time, 1);
    EXPECT_EQ(OK, result);
    data_used += calculate_camera_metadata_entry_data_size(
            get_camera_metadata_tag_type(ANDROID_SENSOR_EXPOSURE_TIME), 1);
    entries_used++;

    // INT32

    int32_t sensitivity = 800;
    result = add_camera_metadata_entry(m,
            ANDROID_SENSOR_SENSITIVITY,
            &sensitivity, 1);
    EXPECT_EQ(OK, result);
    data_used += calculate_camera_metadata_entry_data_size(
            get_camera_metadata_tag_type(ANDROID_SENSOR_SENSITIVITY), 1);
    entries_used++;

    // FLOAT

    float focusDistance = 0.5f;
    result = add_camera_metadata_entry(m,
            ANDROID_LENS_FOCUS_DISTANCE,
            &focusDistance, 1);
    EXPECT_EQ(OK, result);
    data_used += calculate_camera_metadata_entry_data_size(
            get_camera_metadata_tag_type(ANDROID_LENS_FOCUS_DISTANCE), 1);
    entries_used++;

    // Array of FLOAT

    float colorTransform[9] = {
        0.9f, 0.0f, 0.0f,
        0.2f, 0.5f, 0.0f,
        0.0f, 0.1f, 0.7f
    };
    result = add_camera_metadata_entry(m,
            ANDROID_COLOR_TRANSFORM,
            colorTransform, 9);
    EXPECT_EQ(OK, result);
    data_used += calculate_camera_metadata_entry_data_size(
            get_camera_metadata_tag_type(ANDROID_COLOR_TRANSFORM), 9);
    entries_used++;

    // Check added entries

    uint32_t tag = 0;
    uint8_t type = 0;
    int32_t *data_int32;
    int64_t *data_int64;
    float *data_float;
    size_t data_count = 0;

    result = get_camera_metadata_entry(m,
            0, &tag, &type, (void**)&data_int64, &data_count);
    EXPECT_EQ(OK, result);
    EXPECT_EQ(ANDROID_SENSOR_EXPOSURE_TIME, tag);
    EXPECT_EQ(TYPE_INT64, type);
    EXPECT_EQ((size_t)1, data_count);
    EXPECT_EQ(exposure_time, *data_int64);

    result = get_camera_metadata_entry(m,
            1, &tag, &type, (void**)&data_int32, &data_count);
    EXPECT_EQ(OK, result);
    EXPECT_EQ(ANDROID_SENSOR_SENSITIVITY, tag);
    EXPECT_EQ(TYPE_INT32, type);
    EXPECT_EQ((size_t)1, data_count);
    EXPECT_EQ(sensitivity, *data_int32);

    result = get_camera_metadata_entry(m,
            2, &tag, &type, (void**)&data_float, &data_count);
    EXPECT_EQ(OK, result);
    EXPECT_EQ(ANDROID_LENS_FOCUS_DISTANCE, tag);
    EXPECT_EQ(TYPE_FLOAT, type);
    EXPECT_EQ((size_t)1, data_count);
    EXPECT_EQ(focusDistance, *data_float);

    result = get_camera_metadata_entry(m,
            3, &tag, &type, (void**)&data_float, &data_count);
    EXPECT_EQ(OK, result);
    EXPECT_EQ(ANDROID_COLOR_TRANSFORM, tag);
    EXPECT_EQ(TYPE_FLOAT, type);
    EXPECT_EQ((size_t)9, data_count);
    for (unsigned int i=0; i < data_count; i++) {
        EXPECT_EQ(colorTransform[i], data_float[i] );
    }

    EXPECT_EQ(calculate_camera_metadata_size(entry_capacity, data_capacity),
            get_camera_metadata_size(m) );

    EXPECT_EQ(calculate_camera_metadata_size(entries_used, data_used),
            get_camera_metadata_compact_size(m) );

    dump_camera_metadata(m, 0, 2);

    free_camera_metadata(m);
}

void add_test_metadata(camera_metadata_t *m, int entry_count) {

    EXPECT_NOT_NULL(m);

    int result;
    size_t data_used = 0;
    size_t entries_used = 0;
    int64_t exposure_time;
    for (int i=0; i < entry_count; i++ ) {
        exposure_time = 100 + i * 100;
        result = add_camera_metadata_entry(m,
                ANDROID_SENSOR_EXPOSURE_TIME,
                &exposure_time, 1);
        EXPECT_EQ(OK, result);
        data_used += calculate_camera_metadata_entry_data_size(
                get_camera_metadata_tag_type(ANDROID_SENSOR_EXPOSURE_TIME), 1);
        entries_used++;
    }
    EXPECT_EQ(data_used, get_camera_metadata_data_count(m));
    EXPECT_EQ(entries_used, get_camera_metadata_entry_count(m));
    EXPECT_GT(get_camera_metadata_data_capacity(m),
            get_camera_metadata_data_count(m));
}

TEST(camera_metadata, add_get_toomany) {
    camera_metadata_t *m = NULL;
    const size_t entry_capacity = 5;
    const size_t data_capacity = 50;
    int result;

    m = allocate_camera_metadata(entry_capacity, data_capacity);

    add_test_metadata(m, entry_capacity);

    int32_t sensitivity = 100;
    result = add_camera_metadata_entry(m,
            ANDROID_SENSOR_SENSITIVITY,
            &sensitivity, 1);

    EXPECT_EQ(ERROR, result);

    uint32_t tag = 0;
    uint8_t type = 0;
    int32_t *data_int32;
    size_t data_count = 0;
    for (unsigned int i=0; i < entry_capacity; i++) {
        int64_t exposure_time = 100 + i * 100;
        result = get_camera_metadata_entry(m,
                i, &tag, &type, (void**)&data_int32, &data_count);
        EXPECT_EQ(OK, result);
        EXPECT_EQ(ANDROID_SENSOR_EXPOSURE_TIME, tag);
        EXPECT_EQ(TYPE_INT64, type);
        EXPECT_EQ((size_t)1, data_count);
        EXPECT_EQ(exposure_time, *data_int32);
    }
    tag = 0;
    type = 0;
    data_int32 = NULL;
    data_count = 0;
    result = get_camera_metadata_entry(m,
            entry_capacity, &tag, &type, (void**)&data_int32, &data_count);
    EXPECT_EQ(ERROR, result);
    EXPECT_EQ((uint32_t)0, tag);
    EXPECT_EQ((uint8_t)0, type);
    EXPECT_EQ((size_t)0, data_count);
    EXPECT_EQ(NULL, data_int32);

    dump_camera_metadata(m, 0, 2);

    free_camera_metadata(m);
}

TEST(camera_metadata, copy_metadata) {
    camera_metadata_t *m = NULL;
    const size_t entry_capacity = 50;
    const size_t data_capacity = 450;

    int result;

    m = allocate_camera_metadata(entry_capacity, data_capacity);

    add_test_metadata(m, entry_capacity);

    size_t buf_size = get_camera_metadata_compact_size(m);
    EXPECT_LT((size_t)0, buf_size);

    uint8_t *buf = (uint8_t*)malloc(buf_size);
    EXPECT_NOT_NULL(buf);

    camera_metadata_t *m2 = copy_camera_metadata(buf, buf_size, m);
    EXPECT_NOT_NULL(m2);
    EXPECT_EQ(buf, (uint8_t*)m2);
    EXPECT_EQ(get_camera_metadata_entry_count(m),
            get_camera_metadata_entry_count(m2));
    EXPECT_EQ(get_camera_metadata_data_count(m),
            get_camera_metadata_data_count(m2));
    EXPECT_EQ(get_camera_metadata_entry_capacity(m2),
            get_camera_metadata_entry_count(m2));
    EXPECT_EQ(get_camera_metadata_data_capacity(m2),
            get_camera_metadata_data_count(m2));

    for (unsigned int i=0; i < get_camera_metadata_entry_count(m); i++) {
        uint32_t tag, tag2;
        uint8_t type, type2;
        uint8_t *data, *data2;
        size_t   data_count, data_count2;

        int result;
        result = get_camera_metadata_entry(m,
                i, &tag, &type, (void**)&data, &data_count);
        EXPECT_EQ(OK, result);
        result = get_camera_metadata_entry(m2,
                i, &tag2, &type2, (void**)&data2, &data_count2);
        EXPECT_EQ(OK, result);
        EXPECT_EQ(tag, tag2);
        EXPECT_EQ(type, type2);
        EXPECT_EQ(data_count, data_count2);
        for (unsigned int j=0; j < data_count; j++) {
            EXPECT_EQ(data[j], data2[j]);
        }
    }

    free(buf);

    free_camera_metadata(m);
}

TEST(camera_metadata, copy_metadata_extraspace) {
    camera_metadata_t *m = NULL;
    const size_t entry_capacity = 12;
    const size_t data_capacity = 100;

    const size_t extra_space = 10;

    int result;

    m = allocate_camera_metadata(entry_capacity, data_capacity);

    add_test_metadata(m, entry_capacity);

    size_t buf_size = get_camera_metadata_compact_size(m);
    EXPECT_LT((size_t)0, buf_size);
    buf_size += extra_space;

    uint8_t *buf = (uint8_t*)malloc(buf_size);
    EXPECT_NOT_NULL(buf);

    camera_metadata_t *m2 = copy_camera_metadata(buf, buf_size, m);
    EXPECT_NOT_NULL(m2);
    EXPECT_EQ(buf, (uint8_t*)m2);
    EXPECT_EQ(get_camera_metadata_entry_count(m),
            get_camera_metadata_entry_count(m2));
    EXPECT_EQ(get_camera_metadata_data_count(m),
            get_camera_metadata_data_count(m2));
    EXPECT_EQ(get_camera_metadata_entry_capacity(m2),
            get_camera_metadata_entry_count(m2));
    EXPECT_EQ(get_camera_metadata_data_capacity(m2),
            get_camera_metadata_data_count(m2));
    EXPECT_EQ(buf + buf_size - extra_space,
            (uint8_t*)m2 + get_camera_metadata_size(m2) );

    for (unsigned int i=0; i < get_camera_metadata_entry_count(m); i++) {
        uint32_t tag, tag2;
        uint8_t type, type2;
        uint8_t *data, *data2;
        size_t   data_count, data_count2;

        int result;
        result = get_camera_metadata_entry(m,
                i, &tag, &type, (void**)&data, &data_count);
        EXPECT_EQ(OK, result);
        result = get_camera_metadata_entry(m2,
                i, &tag2, &type2, (void**)&data2, &data_count2);
        EXPECT_EQ(OK, result);
        EXPECT_EQ(tag, tag2);
        EXPECT_EQ(type, type2);
        EXPECT_EQ(data_count, data_count2);
        for (unsigned int j=0; j < data_count; j++) {
            EXPECT_EQ(data[j], data2[j]);
        }
    }

    free(buf);

    free_camera_metadata(m);
}

TEST(camera_metadata, copy_metadata_nospace) {
    camera_metadata_t *m = NULL;
    const size_t entry_capacity = 5;
    const size_t data_capacity = 50;

    int result;

    m = allocate_camera_metadata(entry_capacity, data_capacity);

    add_test_metadata(m, entry_capacity);

    size_t buf_size = get_camera_metadata_compact_size(m);
    EXPECT_LT((size_t)0, buf_size);

    buf_size--;

    uint8_t *buf = (uint8_t*)malloc(buf_size);
    EXPECT_NOT_NULL(buf);

    camera_metadata_t *m2 = copy_camera_metadata(buf, buf_size, m);
    EXPECT_NULL(m2);

    free(buf);

    free_camera_metadata(m);
}

TEST(camera_metadata, append_metadata) {
    camera_metadata_t *m = NULL;
    const size_t entry_capacity = 5;
    const size_t data_capacity = 50;

    int result;

    m = allocate_camera_metadata(entry_capacity, data_capacity);

    add_test_metadata(m, entry_capacity);

    camera_metadata_t *m2 = NULL;

    m2 = allocate_camera_metadata(entry_capacity*2, data_capacity*2);
    EXPECT_NOT_NULL(m2);

    result = append_camera_metadata(m2, m);

    EXPECT_EQ(OK, result);

    EXPECT_EQ(get_camera_metadata_entry_count(m), get_camera_metadata_entry_count(m2));
    EXPECT_EQ(get_camera_metadata_data_count(m),  get_camera_metadata_data_count(m2));
    EXPECT_EQ(entry_capacity*2, get_camera_metadata_entry_capacity(m2));
    EXPECT_EQ(data_capacity*2,  get_camera_metadata_data_capacity(m2));

    for (unsigned int i=0; i < get_camera_metadata_entry_count(m); i++) {
        uint32_t tag, tag2;
        uint8_t type, type2;
        uint8_t *data, *data2;
        size_t   data_count, data_count2;

        int result;
        result = get_camera_metadata_entry(m,
                i, &tag, &type, (void**)&data, &data_count);
        EXPECT_EQ(OK, result);
        result = get_camera_metadata_entry(m2,
                i, &tag2, &type2, (void**)&data2, &data_count2);
        EXPECT_EQ(OK, result);
        EXPECT_EQ(tag, tag2);
        EXPECT_EQ(type, type2);
        EXPECT_EQ(data_count, data_count2);
        for (unsigned int j=0; j < data_count; j++) {
            EXPECT_EQ(data[j], data2[j]);
        }
    }

    result = append_camera_metadata(m2, m);

    EXPECT_EQ(OK, result);

    EXPECT_EQ(get_camera_metadata_entry_count(m)*2, get_camera_metadata_entry_count(m2));
    EXPECT_EQ(get_camera_metadata_data_count(m)*2,  get_camera_metadata_data_count(m2));
    EXPECT_EQ(entry_capacity*2, get_camera_metadata_entry_capacity(m2));
    EXPECT_EQ(data_capacity*2,  get_camera_metadata_data_capacity(m2));

    for (unsigned int i=0; i < get_camera_metadata_entry_count(m2); i++) {
        uint32_t tag, tag2;
        uint8_t type, type2;
        uint8_t *data, *data2;
        size_t   data_count, data_count2;

        int result;
        result = get_camera_metadata_entry(m,
                i % entry_capacity, &tag, &type, (void**)&data, &data_count);
        EXPECT_EQ(OK, result);
        result = get_camera_metadata_entry(m2,
                i, &tag2, &type2, (void**)&data2, &data_count2);
        EXPECT_EQ(OK, result);
        EXPECT_EQ(tag, tag2);
        EXPECT_EQ(type, type2);
        EXPECT_EQ(data_count, data_count2);
        for (unsigned int j=0; j < data_count; j++) {
            EXPECT_EQ(data[j], data2[j]);
        }
    }

    free_camera_metadata(m);
    free_camera_metadata(m2);
}

TEST(camera_metadata, append_metadata_nospace) {
    camera_metadata_t *m = NULL;
    const size_t entry_capacity = 5;
    const size_t data_capacity = 50;

    int result;

    m = allocate_camera_metadata(entry_capacity, data_capacity);

    add_test_metadata(m, entry_capacity);

    camera_metadata_t *m2 = NULL;

    m2 = allocate_camera_metadata(entry_capacity-1, data_capacity);
    EXPECT_NOT_NULL(m2);

    result = append_camera_metadata(m2, m);

    EXPECT_EQ(ERROR, result);
    EXPECT_EQ((size_t)0, get_camera_metadata_entry_count(m2));
    EXPECT_EQ((size_t)0, get_camera_metadata_data_count(m2));

    free_camera_metadata(m);
    free_camera_metadata(m2);
}

TEST(camera_metadata, append_metadata_onespace) {
    camera_metadata_t *m = NULL;
    const size_t entry_capacity = 5;
    const size_t data_capacity = 50;
    const size_t entry_capacity2 = entry_capacity * 2 - 2;
    const size_t data_capacity2 = data_capacity * 2;
    int result;

    m = allocate_camera_metadata(entry_capacity, data_capacity);

    add_test_metadata(m, entry_capacity);

    camera_metadata_t *m2 = NULL;

    m2 = allocate_camera_metadata(entry_capacity2, data_capacity2);
    EXPECT_NOT_NULL(m2);

    result = append_camera_metadata(m2, m);

    EXPECT_EQ(OK, result);

    EXPECT_EQ(get_camera_metadata_entry_count(m), get_camera_metadata_entry_count(m2));
    EXPECT_EQ(get_camera_metadata_data_count(m),  get_camera_metadata_data_count(m2));
    EXPECT_EQ(entry_capacity2, get_camera_metadata_entry_capacity(m2));
    EXPECT_EQ(data_capacity2,  get_camera_metadata_data_capacity(m2));

    for (unsigned int i=0; i < get_camera_metadata_entry_count(m); i++) {
        uint32_t tag, tag2;
        uint8_t type, type2;
        uint8_t *data, *data2;
        size_t   data_count, data_count2;

        int result;
        result = get_camera_metadata_entry(m,
                i, &tag, &type, (void**)&data, &data_count);
        EXPECT_EQ(OK, result);
        result = get_camera_metadata_entry(m2,
                i, &tag2, &type2, (void**)&data2, &data_count2);
        EXPECT_EQ(OK, result);
        EXPECT_EQ(tag, tag2);
        EXPECT_EQ(type, type2);
        EXPECT_EQ(data_count, data_count2);
        for (unsigned int j=0; j < data_count; j++) {
            EXPECT_EQ(data[j], data2[j]);
        }
    }

    result = append_camera_metadata(m2, m);

    EXPECT_EQ(ERROR, result);
    EXPECT_EQ(entry_capacity, get_camera_metadata_entry_count(m2));
    EXPECT_EQ(get_camera_metadata_data_count(m),
            get_camera_metadata_data_count(m2));
    EXPECT_EQ(entry_capacity2, get_camera_metadata_entry_capacity(m2));
    EXPECT_EQ(data_capacity2,  get_camera_metadata_data_capacity(m2));

    for (unsigned int i=0; i < get_camera_metadata_entry_count(m2); i++) {
        uint32_t tag, tag2;
        uint8_t type, type2;
        uint8_t *data, *data2;
        size_t   data_count, data_count2;

        int result;
        result = get_camera_metadata_entry(m,
                i % entry_capacity, &tag, &type, (void**)&data, &data_count);
        EXPECT_EQ(OK, result);
        result = get_camera_metadata_entry(m2,
                i, &tag2, &type2, (void**)&data2, &data_count2);
        EXPECT_EQ(OK, result);
        EXPECT_EQ(tag, tag2);
        EXPECT_EQ(type, type2);
        EXPECT_EQ(data_count, data_count2);
        for (unsigned int j=0; j < data_count; j++) {
            EXPECT_EQ(data[j], data2[j]);
        }
    }

    free_camera_metadata(m);
    free_camera_metadata(m2);
}

TEST(camera_metadata, vendor_tags) {
    camera_metadata_t *m = NULL;
    const size_t entry_capacity = 5;
    const size_t data_capacity = 50;
    int result;

    m = allocate_camera_metadata(entry_capacity, data_capacity);

    uint8_t superMode = 5;
    result = add_camera_metadata_entry(m,
            FAKEVENDOR_SENSOR_SUPERMODE,
            &superMode, 1);
    EXPECT_EQ(ERROR, result);

    result = add_camera_metadata_entry(m,
            ANDROID_REQUEST_METADATA_MODE,
            &superMode, 1);
    EXPECT_EQ(OK, result);

    EXPECT_NULL(get_camera_metadata_section_name(FAKEVENDOR_SENSOR_SUPERMODE));
    EXPECT_NULL(get_camera_metadata_tag_name(FAKEVENDOR_SENSOR_SUPERMODE));
    EXPECT_EQ(-1, get_camera_metadata_tag_type(FAKEVENDOR_SENSOR_SUPERMODE));

    set_camera_metadata_vendor_tag_ops(&fakevendor_query_ops);

    result = add_camera_metadata_entry(m,
            FAKEVENDOR_SENSOR_SUPERMODE,
            &superMode, 1);
    EXPECT_EQ(OK, result);

    result = add_camera_metadata_entry(m,
            ANDROID_REQUEST_METADATA_MODE,
            &superMode, 1);
    EXPECT_EQ(OK, result);

    result = add_camera_metadata_entry(m,
            FAKEVENDOR_SCALER_END,
            &superMode, 1);
    EXPECT_EQ(ERROR, result);

    EXPECT_STREQ("com.fakevendor.sensor",
            get_camera_metadata_section_name(FAKEVENDOR_SENSOR_SUPERMODE));
    EXPECT_STREQ("superMode",
            get_camera_metadata_tag_name(FAKEVENDOR_SENSOR_SUPERMODE));
    EXPECT_EQ(TYPE_BYTE,
            get_camera_metadata_tag_type(FAKEVENDOR_SENSOR_SUPERMODE));

    EXPECT_STREQ("com.fakevendor.scaler",
            get_camera_metadata_section_name(FAKEVENDOR_SCALER_END));
    EXPECT_NULL(get_camera_metadata_tag_name(FAKEVENDOR_SCALER_END));
    EXPECT_EQ(-1, get_camera_metadata_tag_type(FAKEVENDOR_SCALER_END));

    set_camera_metadata_vendor_tag_ops(NULL);

    result = add_camera_metadata_entry(m,
            FAKEVENDOR_SENSOR_SUPERMODE,
            &superMode, 1);
    EXPECT_EQ(ERROR, result);

    result = add_camera_metadata_entry(m,
            ANDROID_REQUEST_METADATA_MODE,
            &superMode, 1);
    EXPECT_EQ(OK, result);

    EXPECT_NULL(get_camera_metadata_section_name(FAKEVENDOR_SENSOR_SUPERMODE));
    EXPECT_NULL(get_camera_metadata_tag_name(FAKEVENDOR_SENSOR_SUPERMODE));
    EXPECT_EQ(-1, get_camera_metadata_tag_type(FAKEVENDOR_SENSOR_SUPERMODE));

    free_camera_metadata(m);
}

TEST(camera_metadata, add_all_tags) {
    int total_tag_count = 0;
    for (int i = 0; i < ANDROID_SECTION_COUNT; i++) {
        total_tag_count += camera_metadata_section_bounds[i][1] -
                camera_metadata_section_bounds[i][0];
    }
    int entry_data_count = 3;
    int conservative_data_space = total_tag_count * entry_data_count * 8;
    uint8_t data[entry_data_count * 8];
    int32_t *data_int32 = (int32_t *)data;
    float *data_float   = (float *)data;
    int64_t *data_int64 = (int64_t *)data;
    double *data_double = (double *)data;
    camera_metadata_rational_t *data_rational = (camera_metadata_rational_t *)data;

    camera_metadata_t *m = allocate_camera_metadata(total_tag_count, conservative_data_space);

    ASSERT_NE((void*)NULL, (void*)m);

    int result;

    int counter = 0;
    for (int i = 0; i < ANDROID_SECTION_COUNT; i++) {
        for (uint32_t tag = camera_metadata_section_bounds[i][0];
                tag < camera_metadata_section_bounds[i][1];
             tag++, counter++) {
            int type = get_camera_metadata_tag_type(tag);
            ASSERT_NE(-1, type);

            switch (type) {
                case TYPE_BYTE:
                    data[0] = tag & 0xFF;
                    data[1] = (tag >> 8) & 0xFF;
                    data[2] = (tag >> 16) & 0xFF;
                    break;
                case TYPE_INT32:
                    data_int32[0] = tag;
                    data_int32[1] = i;
                    data_int32[2] = counter;
                    break;
                case TYPE_FLOAT:
                    data_float[0] = tag;
                    data_float[1] = i;
                    data_float[2] = counter / (float)total_tag_count;
                    break;
                case TYPE_INT64:
                    data_int64[0] = (int64_t)tag | ( (int64_t)tag << 32);
                    data_int64[1] = i;
                    data_int64[2] = counter;
                    break;
                case TYPE_DOUBLE:
                    data_double[0] = tag;
                    data_double[1] = i;
                    data_double[2] = counter / (double)total_tag_count;
                    break;
                case TYPE_RATIONAL:
                    data_rational[0].numerator = tag;
                    data_rational[0].denominator = 1;
                    data_rational[1].numerator = i;
                    data_rational[1].denominator = 1;
                    data_rational[2].numerator = counter;
                    data_rational[2].denominator = total_tag_count;
                    break;
                default:
                    FAIL() << "Unknown type field encountered:" << type;
                    break;
            }
            result = add_camera_metadata_entry(m,
                    tag,
                    data,
                    entry_data_count);
            ASSERT_EQ(OK, result);

        }
    }

    dump_camera_metadata(m, 0, 2);

    free_camera_metadata(m);
}

TEST(camera_metadata, sort_metadata) {
    camera_metadata_t *m = NULL;
    const size_t entry_capacity = 5;
    const size_t data_capacity = 100;

    int result;

    m = allocate_camera_metadata(entry_capacity, data_capacity);

    // Add several unique entries in non-sorted order

    float colorTransform[9] = {
        0.9f, 0.0f, 0.0f,
        0.2f, 0.5f, 0.0f,
        0.0f, 0.1f, 0.7f
    };
    result = add_camera_metadata_entry(m,
            ANDROID_COLOR_TRANSFORM,
            colorTransform, 9);
    EXPECT_EQ(OK, result);

    float focus_distance = 0.5f;
    result = add_camera_metadata_entry(m,
            ANDROID_LENS_FOCUS_DISTANCE,
            &focus_distance, 1);
    EXPECT_EQ(OK, result);

    int64_t exposure_time = 1000000000;
    result = add_camera_metadata_entry(m,
            ANDROID_SENSOR_EXPOSURE_TIME,
            &exposure_time, 1);
    EXPECT_EQ(OK, result);

    int32_t sensitivity = 800;
    result = add_camera_metadata_entry(m,
            ANDROID_SENSOR_SENSITIVITY,
            &sensitivity, 1);
    EXPECT_EQ(OK, result);

    // Test unsorted find
    uint8_t type;
    float  *f;
    size_t  data_count;
    result = find_camera_metadata_entry(m,
            ANDROID_LENS_FOCUS_DISTANCE,
            &type,
            (void**)&f,
            &data_count);
    EXPECT_EQ(OK, result);
    EXPECT_EQ(TYPE_FLOAT, type);
    EXPECT_EQ(1, (int)data_count);
    EXPECT_EQ(focus_distance, *f);

    result = find_camera_metadata_entry(m,
            ANDROID_NOISE_STRENGTH,
            &type,
            (void**)&f,
            &data_count);
    EXPECT_EQ(NOT_FOUND, result);

    // Sort
    std::cout << "Pre-sorted metadata" << std::endl;
    dump_camera_metadata(m, 0, 2);

    result = sort_camera_metadata(m);
    EXPECT_EQ(OK, result);

    std::cout << "Sorted metadata" << std::endl;
    dump_camera_metadata(m, 0, 2);

    // Test sorted find

    result = find_camera_metadata_entry(m,
            ANDROID_LENS_FOCUS_DISTANCE,
            &type,
            (void**)&f,
            &data_count);
    EXPECT_EQ(OK, result);
    EXPECT_EQ(TYPE_FLOAT, type);
    EXPECT_EQ(1, (int)data_count);
    EXPECT_EQ(focus_distance, *f);

    result = find_camera_metadata_entry(m,
            ANDROID_NOISE_STRENGTH,
            &type,
            (void**)&f,
            &data_count);
    EXPECT_EQ(NOT_FOUND, result);


    free_camera_metadata(m);
}
