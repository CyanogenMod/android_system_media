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
  float brightness;
} BrightnessParameters;

void brightness_init(void** user_data) {
  (*user_data) = malloc(sizeof(BrightnessParameters));
}

void brightness_teardown(void* user_data) {
  free(user_data);
}

void brightness_setvalue(const char* key, Value value, void* user_data) {
  if (strcmp(key, "brightness") == 0)
    ((BrightnessParameters*)user_data)->brightness = GetFloatValue(value);
  else
    LOGE("Unknown parameter: %s!", key);
  LOGI("Done!");
}

int brightness_process(const NativeBuffer* inputs,
                       int input_count,
                       NativeBuffer output,
                       void* user_data) {
  // Make sure we have exactly one input
  if (input_count != 1) {
    LOGE("Brightness: Incorrect input count! Expected 1 but got %d!", input_count);
    return 0;
  }

  // Make sure sizes match up
  if (NativeBuffer_GetSize(inputs[0]) != NativeBuffer_GetSize(output)) {
    LOGE("Brightness: Input-output sizes do not match up. %d vs. %d!",
         NativeBuffer_GetSize(inputs[0]),
         NativeBuffer_GetSize(output));
    return 0;
  }

  // Get the input and output pointers
  const char* input_ptr = NativeBuffer_GetData(inputs[0]);
  char* output_ptr = NativeBuffer_GetMutableData(output);
  if (!input_ptr || !output_ptr) {
    LOGE("Brightness: No input or output pointer found!");
    return 0;
  }

  // Get the parameters
  BrightnessParameters* params = (BrightnessParameters*)user_data;
  const float brightness = params->brightness;

  // Run the brightness adjustment
  int i;
  const int b = (int)(brightness * 255.0f);
  int val = 0;
  for (i = 0; i < NativeBuffer_GetSize(output); ++i) {
    val = (*(input_ptr++) * b) / 255;
    *(output_ptr++) = val > 255 ? 255 : val;
  }

  return 1;
}

