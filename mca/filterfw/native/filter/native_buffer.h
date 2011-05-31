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

#ifndef ANDROID_FILTERFW_FILTER_NATIVE_BUFFER_H
#define ANDROID_FILTERFW_FILTER_NATIVE_BUFFER_H

#include <jni.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void* NativeBuffer;

int NativeBuffer_AllocateData(NativeBuffer buffer, int size);

const char* NativeBuffer_GetData(NativeBuffer buffer);

char* NativeBuffer_GetMutableData(NativeBuffer buffer);

int NativeBuffer_GetSize(NativeBuffer buffer);

int NativeBuffer_IsEmpty(NativeBuffer buffer);

int NativeBuffer_IsNull(NativeBuffer buffer);

int NativeBuffer_IsMutable(NativeBuffer buffer);

char* NativeBuffer_GetDataPtrFromJavaObject(JNIEnv* env, jobject buffer, int offset);

#ifdef __cplusplus
} // extern "C"
#endif

#endif  // ANDROID_FILTERFW_FILTER_NATIVE_BUFFER_H
