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

#include <stddef.h>
#include <stdlib.h>

#include <cutils/log.h>

#include "filter/native_buffer.h"
#include "data_buffer.h"

int NativeBuffer_AllocateData(NativeBuffer buffer, int size) {
  DataBuffer* native_buffer = reinterpret_cast<DataBuffer*>(buffer);
  if (native_buffer && !native_buffer->data() && native_buffer->is_mutable()) {
    native_buffer->AllocateData(size);
    return 1;
  } else {
    LOGE("NativeBuffer allocation failed! Make sure your buffer is empty and mutable!");
    return 0;
  }
}

const char* NativeBuffer_GetData(NativeBuffer buffer) {
  return buffer ? reinterpret_cast<DataBuffer*>(buffer)->data() : NULL;
}

char* NativeBuffer_GetMutableData(NativeBuffer buffer) {
  DataBuffer* native_buffer = reinterpret_cast<DataBuffer*>(buffer);
  if (native_buffer && native_buffer->is_mutable()) {
    return native_buffer->data();
  } else {
    LOGE("NativeBuffer: Attempting to access non-mutable data buffer. Access denied.");
    return NULL;
  }
}

int NativeBuffer_GetSize(NativeBuffer buffer) {
  return buffer ? reinterpret_cast<DataBuffer*>(buffer)->size() : 0;
}

int NativeBuffer_IsEmpty(NativeBuffer buffer) {
  return buffer ? reinterpret_cast<DataBuffer*>(buffer)->data() == NULL : 1;
}

int NativeBuffer_IsNull(NativeBuffer buffer) {
  return buffer == NULL;
}

int NativeBuffer_IsMutable(NativeBuffer buffer) {
  return buffer ? (reinterpret_cast<DataBuffer*>(buffer)->is_mutable() ? 1 : 0) : 0;
}

char* NativeBuffer_GetDataPtrFromJavaObject(JNIEnv* env, jobject buffer, int offset) {
  DataBuffer data_buffer;
  if (DataBuffer::FromJavaObject(env, buffer, &data_buffer)) {
    char* result = data_buffer.data();
    if (offset >= data_buffer.size()) {
      LOGE("Error: Buffer offset out-of-bounds access! Accessing %d-sized buffer at offset %d!",
           data_buffer.size(), offset);
      return NULL;
    }
    return result + offset;
  }
  return NULL;
}

