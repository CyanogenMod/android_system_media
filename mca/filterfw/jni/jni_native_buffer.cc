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

#include "jni/jni_native_buffer.h"
#include "jni/jni_util.h"
#include "native/filter/src/data_buffer.h"

jboolean Java_android_filterfw_core_NativeBuffer_allocate(JNIEnv* env, jobject thiz, jint size) {
  DataBuffer buffer(size);
  return ToJBool(buffer.AttachToJavaObject(env, thiz));
}

jboolean Java_android_filterfw_core_NativeBuffer_deallocate(JNIEnv* env,
                                                            jobject thiz,
                                                            jboolean owns_data) {
  if (ToCppBool(owns_data)) {
    DataBuffer buffer;
    DataBuffer::FromJavaObject(env, thiz, &buffer);
    buffer.DeleteData();
  }
  return JNI_TRUE;
}

jboolean Java_android_filterfw_core_NativeBuffer_nativeCopyTo(JNIEnv* env,
                                                              jobject thiz,
                                                              jobject new_buffer) {
  // Get this buffer
  DataBuffer buffer;
  DataBuffer::FromJavaObject(env, thiz, &buffer);

  // Make copy
  DataBuffer buffer_copy;
  if (buffer_copy.CopyFrom(buffer)) {
    return ToJBool(buffer_copy.AttachToJavaObject(env, new_buffer));
  } else
    return JNI_FALSE;
}

