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

#ifndef ANDROID_FILTERFW_JNI_GL_ENVIRONMENT_H
#define ANDROID_FILTERFW_JNI_GL_ENVIRONMENT_H

#include <jni.h>

#ifdef __cplusplus
extern "C" {
#endif

JNIEXPORT jboolean JNICALL
Java_android_filterfw_core_GLEnvironment_nativeAllocate(JNIEnv* env, jobject thiz);

JNIEXPORT jboolean JNICALL
Java_android_filterfw_core_GLEnvironment_nativeDeallocate(JNIEnv* env, jobject thiz);

JNIEXPORT jboolean JNICALL
Java_android_filterfw_core_GLEnvironment_nativeInitWithNewContext(JNIEnv* env, jobject thiz);

JNIEXPORT jboolean JNICALL
Java_android_filterfw_core_GLEnvironment_nativeInitWithCurrentContext(JNIEnv* env, jobject thiz);

JNIEXPORT jboolean JNICALL
Java_android_filterfw_core_GLEnvironment_nativeIsActive(JNIEnv* env, jobject thiz);

JNIEXPORT jboolean JNICALL
Java_android_filterfw_core_GLEnvironment_nativeActivate(JNIEnv* env, jobject thiz);

JNIEXPORT jboolean JNICALL
Java_android_filterfw_core_GLEnvironment_nativeDeactivate(JNIEnv* env, jobject thiz);

JNIEXPORT jobject JNICALL
Java_android_filterfw_core_GLEnvironment_nativeActiveEnvironment(JNIEnv* env, jclass clazz);

JNIEXPORT jboolean JNICALL
Java_android_filterfw_core_GLEnvironment_nativeSwapBuffers(JNIEnv* env, jobject thiz);

JNIEXPORT jint JNICALL
Java_android_filterfw_core_GLEnvironment_nativeAddSurface(JNIEnv* env,
                                                          jobject thiz,
                                                          jobject surface);

JNIEXPORT jboolean JNICALL
Java_android_filterfw_core_GLEnvironment_nativeActivateSurfaceId(JNIEnv* env,
                                                                 jobject thiz,
                                                                 jint surfaceId);

JNIEXPORT jboolean JNICALL
Java_android_filterfw_core_GLEnvironment_nativeRemoveSurfaceId(JNIEnv* env,
                                                               jobject thiz,
                                                               jint surfaceId);

#ifdef __cplusplus
}
#endif

#endif // ANDROID_FILTERFW_JNI_GL_ENVIRONMENT_H
