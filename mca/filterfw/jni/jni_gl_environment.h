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

#ifndef JNI_GL_ENVIRONMENT_H
#define JNI_GL_ENVIRONMENT_H

#include <jni.h>

#ifdef __cplusplus
extern "C" {
#endif

JNIEXPORT jboolean JNICALL
Java_android_filterfw_core_GLEnvironment_allocate(JNIEnv* env, jobject thiz);

JNIEXPORT jboolean JNICALL
Java_android_filterfw_core_GLEnvironment_deallocate(JNIEnv* env, jobject thiz);

JNIEXPORT jboolean JNICALL
Java_android_filterfw_core_GLEnvironment_initWithNewContext(JNIEnv* env, jobject thiz);

JNIEXPORT jboolean JNICALL
Java_android_filterfw_core_GLEnvironment_initWithCurrentContext(JNIEnv* env, jobject thiz);

JNIEXPORT jboolean JNICALL
Java_android_filterfw_core_GLEnvironment_activate(JNIEnv* env, jobject thiz);

JNIEXPORT jboolean JNICALL
Java_android_filterfw_core_GLEnvironment_deactivate(JNIEnv* env, jobject thiz);

JNIEXPORT jobject JNICALL
Java_android_filterfw_core_GLEnvironment_activeEnvironment(JNIEnv* env, jclass clazz);

JNIEXPORT jboolean JNICALL
Java_android_filterfw_core_GLEnvironment_swapBuffers(JNIEnv* env, jobject thiz);

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

#endif /* JNI_GL_ENVIRONMENT_H */
