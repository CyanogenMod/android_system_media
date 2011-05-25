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

#include <android/log.h>
#include <stdlib.h>

#include <filter/native_buffer.h>
#include <filter/value.h>

#define  LOGI(...) __android_log_print(ANDROID_LOG_INFO, "MCA", __VA_ARGS__)
#define  LOGW(...) __android_log_print(ANDROID_LOG_WARN, "MCA", __VA_ARGS__)
#define  LOGE(...) __android_log_print(ANDROID_LOG_ERROR, "MCA", __VA_ARGS__)

typedef struct {
  float contrast;
} ContrastParameters;

void contrast_init(void** user_data) {
  (*user_data) = malloc(sizeof(ContrastParameters));
}

void contrast_teardown(void* user_data) {
  free(user_data);
}

void contrast_setvalue(const char* key, Value value, void* user_data) {
  if (strcmp(key, "contrast") == 0)
    ((ContrastParameters*)user_data)->contrast = GetFloatValue(value);
  else
    LOGE("Unknown parameter: %s!", key);
  LOGI("Done!");
}

int contrast_process(const NativeBuffer* inputs,
                     int input_count,
                     NativeBuffer output,
                     void* user_data) {
  // Make sure we have exactly one input
  if (input_count != 1)
    return 0;

  // Make sure sizes match up
  if (NativeBuffer_GetSize(inputs[0]) != NativeBuffer_GetSize(output))
    return 0;

  // Get the input and output pointers
  const char* input_ptr = NativeBuffer_GetData(inputs[0]);
  char* output_ptr = NativeBuffer_GetMutableData(output);
  if (!input_ptr || !output_ptr)
    return 0;

  // Get the parameters
  ContrastParameters* params = (ContrastParameters*)user_data;
  const float contrast = params->contrast;

  // Run the contrast adjustment
  int i;
  for (i = 0; i < NativeBuffer_GetSize(output); ++i) {
    float px = *(input_ptr++) / 255.0;
    px -= 0.5;
    px *= contrast;
    px += 0.5;
    *(output_ptr++) = (char)(px > 1.0 ? 255.0 : (px < 0.0 ? 0.0 : px * 255.0));
  }

  return 1;
}

