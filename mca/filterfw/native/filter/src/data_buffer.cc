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

#include "data_buffer.h"

bool DataBuffer::FromJavaObject(JNIEnv* env, jobject buffer, DataBuffer* result) {
  jclass base_class = env->FindClass("android/filterfw/core/NativeBuffer");

  // Get fields
  jfieldID ptr_field = env->GetFieldID(base_class, "mDataPointer", "J");
  jfieldID size_field = env->GetFieldID(base_class, "mSize", "I");

  // Get their values
  char* data = reinterpret_cast<char*>(env->GetLongField(buffer, ptr_field));
  int size = env->GetIntField(buffer, size_field);

  // Clean-up
  env->DeleteLocalRef(base_class);

  return result->SetData(data, size);
}

bool DataBuffer::AttachToJavaObject(JNIEnv* env, jobject buffer) {
  jclass base_class = env->FindClass("android/filterfw/core/NativeBuffer");

  // Get fields
  jfieldID ptr_field = env->GetFieldID(base_class, "mDataPointer", "J");
  jfieldID size_field = env->GetFieldID(base_class, "mSize", "I");

  // Set their values
  env->SetLongField(buffer, ptr_field, reinterpret_cast<jlong>(data_));
  env->SetIntField(buffer, size_field, size_);

  return true;
}

