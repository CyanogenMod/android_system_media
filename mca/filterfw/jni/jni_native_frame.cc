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

#include "android/bitmap.h"

#include "jni/jni_native_frame.h"
#include "jni/jni_native_buffer.h"
#include "jni/jni_util.h"

#include "native/base/basictypes.h"
#include "native/base/logging.h"
#include "native/core/gl_frame.h"
#include "native/core/native_frame.h"
#include "native/filter/src/data_buffer.h"

using android::filterfw::NativeFrame;
using android::filterfw::GLFrame;

jboolean Java_android_filterfw_core_NativeFrame_allocate(JNIEnv* env, jobject thiz, jint size) {
  return ToJBool(WrapObjectInJava(new NativeFrame(size), env, thiz, true));
}

jboolean Java_android_filterfw_core_NativeFrame_deallocate(JNIEnv* env, jobject thiz) {
  return ToJBool(DeleteNativeObject<NativeFrame>(env, thiz));
}

jint Java_android_filterfw_core_NativeFrame_nativeIntSize(JNIEnv*, jclass) {
  return sizeof(jint);
}

jint Java_android_filterfw_core_NativeFrame_nativeFloatSize(JNIEnv*, jclass) {
  return sizeof(jfloat);
}

jboolean Java_android_filterfw_core_NativeFrame_setNativeData(JNIEnv* env,
                                                              jobject thiz,
                                                              jbyteArray data,
                                                              jint offset,
                                                              jint length) {
  NativeFrame* frame = ConvertFromJava<NativeFrame>(env, thiz);
  if (frame && data) {
    jbyte* bytes = env->GetByteArrayElements(data, NULL);
    if (bytes) {
      const bool success = frame->WriteData(reinterpret_cast<const uint8*>(bytes + offset),
                                            0,
                                            length);
      env->ReleaseByteArrayElements(data, bytes, JNI_ABORT);
      return ToJBool(success);
    }
  }
  return JNI_FALSE;
}

jbyteArray Java_android_filterfw_core_NativeFrame_getNativeData(JNIEnv* env,
                                                                jobject thiz,
                                                                jint size) {
  NativeFrame* frame = ConvertFromJava<NativeFrame>(env, thiz);
  if (frame) {
    const uint8* data = frame->Data();
    if (!data || size > frame->Size())
      return NULL;
    jbyteArray result = env->NewByteArray(size);
    env->SetByteArrayRegion(result, 0, size, reinterpret_cast<const jbyte*>(data));
    return result;
  }
  return NULL;
}

jboolean Java_android_filterfw_core_NativeFrame_getNativeBuffer(JNIEnv* env,
                                                                jobject thiz,
                                                                jobject buffer) {
  NativeFrame* frame = ConvertFromJava<NativeFrame>(env, thiz);
  if (frame) {
    uint8* data = frame->MutableData();
    DataBuffer nbuffer(reinterpret_cast<char*>(data), frame->Size());
    return ToJBool(nbuffer.AttachToJavaObject(env, buffer));
  }
  return JNI_FALSE;
}

jboolean Java_android_filterfw_core_NativeFrame_setNativeInts(JNIEnv* env,
                                                              jobject thiz,
                                                              jintArray ints) {
  NativeFrame* frame = ConvertFromJava<NativeFrame>(env, thiz);
  if (frame && ints) {
    jint* int_ptr = env->GetIntArrayElements(ints, NULL);
    const int length = env->GetArrayLength(ints);
    if (int_ptr) {
      const bool success = frame->WriteData(reinterpret_cast<const uint8*>(int_ptr),
                                            0,
                                            length * sizeof(jint));
      env->ReleaseIntArrayElements(ints, int_ptr, JNI_ABORT);
      return ToJBool(success);
    }
  }
  return JNI_FALSE;
}

jintArray Java_android_filterfw_core_NativeFrame_getNativeInts(JNIEnv* env,
                                                               jobject thiz,
                                                               jint size) {
  NativeFrame* frame = ConvertFromJava<NativeFrame>(env, thiz);
  if (frame) {
    const uint8* data = frame->Data();
    if (!data || size > frame->Size() || (size % sizeof(jint)) != 0)
      return NULL;
    const int count = size / sizeof(jint);
    jintArray result = env->NewIntArray(count);
    env->SetIntArrayRegion(result, 0, count, reinterpret_cast<const jint*>(data));
    return result;
  }
  return NULL;
}

jboolean Java_android_filterfw_core_NativeFrame_setNativeFloats(JNIEnv* env,
                                                                jobject thiz,
                                                                jfloatArray floats) {
  NativeFrame* frame = ConvertFromJava<NativeFrame>(env, thiz);
  if (frame && floats) {
    jfloat* float_ptr = env->GetFloatArrayElements(floats, NULL);
    const int length = env->GetArrayLength(floats);
    if (float_ptr) {
      const bool success = frame->WriteData(reinterpret_cast<const uint8*>(float_ptr),
                                            0,
                                            length * sizeof(jfloat));
      env->ReleaseFloatArrayElements(floats, float_ptr, JNI_ABORT);
      return ToJBool(success);
    }
  }
  return JNI_FALSE;
}

jfloatArray Java_android_filterfw_core_NativeFrame_getNativeFloats(JNIEnv* env,
                                                                   jobject thiz,
                                                                   jint size) {
  NativeFrame* frame = ConvertFromJava<NativeFrame>(env, thiz);
  if (frame) {
    const uint8* data = frame->Data();
    if (!data || size > frame->Size() || (size % sizeof(jfloat)) != 0)
      return NULL;
    const int count = size / sizeof(jfloat);
    jfloatArray result = env->NewFloatArray(count);
    env->SetFloatArrayRegion(result, 0, count, reinterpret_cast<const jfloat*>(data));
    return result;
  }
  return NULL;
}

jboolean Java_android_filterfw_core_NativeFrame_setNativeBitmap(JNIEnv* env,
                                                                jobject thiz,
                                                                jobject bitmap,
                                                                jint size) {
  NativeFrame* frame = ConvertFromJava<NativeFrame>(env, thiz);
  if (frame && bitmap) {
    uint8* pixels;
    const int result = AndroidBitmap_lockPixels(env, bitmap, reinterpret_cast<void**>(&pixels));
    if (result == ANDROID_BITMAP_RESUT_SUCCESS) {
      const bool success = frame->WriteData(pixels, 0, size);
      return (AndroidBitmap_unlockPixels(env, bitmap) == ANDROID_BITMAP_RESUT_SUCCESS) && success;
    }
  }
  return JNI_FALSE;
}

jboolean Java_android_filterfw_core_NativeFrame_getNativeBitmap(JNIEnv* env,
                                                                jobject thiz,
                                                                jobject bitmap) {
  NativeFrame* frame = ConvertFromJava<NativeFrame>(env, thiz);
  if (frame && bitmap) {
    uint8* pixels;
    const int result = AndroidBitmap_lockPixels(env, bitmap, reinterpret_cast<void**>(&pixels));
    if (result == ANDROID_BITMAP_RESUT_SUCCESS) {
      memcpy(pixels, frame->Data(), frame->Size());
      return (AndroidBitmap_unlockPixels(env, bitmap) == ANDROID_BITMAP_RESUT_SUCCESS);
    }
  }
  return JNI_FALSE;
}

jint Java_android_filterfw_core_NativeFrame_getNativeCapacity(JNIEnv* env, jobject thiz) {
  NativeFrame* frame = ConvertFromJava<NativeFrame>(env, thiz);
  return frame ? frame->Capacity() : -1;
}

jboolean Java_android_filterfw_core_NativeFrame_nativeCopyFromNative(JNIEnv* env,
                                                                     jobject thiz,
                                                                     jobject frame) {
  NativeFrame* this_frame = ConvertFromJava<NativeFrame>(env, thiz);
  NativeFrame* other_frame = ConvertFromJava<NativeFrame>(env, frame);
  if (this_frame && other_frame) {
    return ToJBool(this_frame->WriteData(other_frame->Data(), 0, other_frame->Size()));
  }
  return JNI_FALSE;
}

jboolean Java_android_filterfw_core_NativeFrame_nativeCopyFromGL(JNIEnv* env,
                                                                 jobject thiz,
                                                                 jobject frame) {
  NativeFrame* this_frame = ConvertFromJava<NativeFrame>(env, thiz);
  GLFrame* other_frame = ConvertFromJava<GLFrame>(env, frame);
  if (this_frame && other_frame) {
    return ToJBool(other_frame->CopyDataTo(this_frame->MutableData(), this_frame->Size()));
  }
  return JNI_FALSE;
}

