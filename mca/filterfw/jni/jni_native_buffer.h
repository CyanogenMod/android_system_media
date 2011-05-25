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

#ifndef JNI_NATIVE_BUFFER_H
#define JNI_NATIVE_BUFFER_H

#include <jni.h>

#ifdef __cplusplus
extern "C" {
#endif

JNIEXPORT jboolean JNICALL
Java_android_filterfw_core_NativeBuffer_allocate(JNIEnv* env, jobject thiz, jint size);

JNIEXPORT jboolean JNICALL
Java_android_filterfw_core_NativeBuffer_deallocate(JNIEnv* env, jobject thiz, jboolean owns_data);

JNIEXPORT jboolean JNICALL
Java_android_filterfw_core_NativeBuffer_nativeCopyTo(JNIEnv* env, jobject thiz, jobject new_buffer);

#ifdef __cplusplus
}
#endif

#endif /* JNI_NATIVE_BUFFER_H */
