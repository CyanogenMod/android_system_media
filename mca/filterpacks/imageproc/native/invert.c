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

#include <filter/native_buffer.h>

int invert_process(const NativeBuffer* inputs,
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

  // Run the inversion
  int i;
  for (i = 0; i < NativeBuffer_GetSize(output); ++i)
    *(output_ptr++) = 255 - *(input_ptr++);

  return 1;
}

