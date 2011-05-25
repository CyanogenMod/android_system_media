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

#ifndef BASE_LOGGING_H_
#define BASE_LOGGING_H_

#include <assert.h>

#include <android/log.h>

#define  LOG_TAG "MCA"
#define  LOG_EVERY_FRAME false

enum LogFormat {
  LogFormat_Long,
  LogFormat_Short
};

// LOGE and LOGI are shortcuts to writing out to the Android info and error
// logs. Example usage: LOGE("Critical Error: %d is zero.", x);
#define  LOGV(...) __android_log_print(ANDROID_LOG_VERBOSE, LOG_TAG, __VA_ARGS__)
#define  LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define  LOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)
#define  LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

#define  LOG_FRAME(...) if (LOG_EVERY_FRAME) __android_log_print(ANDROID_LOG_VERBOSE, LOG_TAG, __VA_ARGS__)

// ASSERT provides a short-cut to the Android log assert (on the error log).
// Example usage: ASSERT(1 != 0, "This is bad.");
#ifndef ASSERT
#define ASSERT(CONDITION) assert(CONDITION)
#endif

#ifndef LOG_ASSERT
#define LOG_ASSERT(CONDITION, ...) \
  if (!CONDITION) {\
    __android_log_assert( #CONDITION,\
                          LOG_TAG,\
                          __VA_ARGS__); \
    assert(false); \
  }
#endif

#endif // BASE_LOGGING_H_
