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

#include "jni/jni_gl_frame.h"
#include "jni/jni_util.h"

#include "native/core/gl_frame.h"
#include "native/core/native_frame.h"

using android::filterfw::GLFrame;
using android::filterfw::NativeFrame;

jboolean Java_android_filterfw_core_GLFrame_allocate(JNIEnv* env,
                                                     jobject thiz,
                                                     jint width,
                                                     jint height) {
  GLFrame* frame = new GLFrame();
  if (frame->Init(width, height)) {
    return ToJBool(WrapObjectInJava(frame, env, thiz, true));
  } else {
    delete frame;
    return JNI_FALSE;
  }
}

jboolean Java_android_filterfw_core_GLFrame_allocateWithTexture(JNIEnv* env,
                                                                jobject thiz,
                                                                jint tex_id,
                                                                jint width,
                                                                jint height,
                                                                jboolean owns,
                                                                jboolean create) {
  GLFrame* frame = new GLFrame();
  if (frame->InitWithTexture(tex_id, width, height, ToCppBool(owns), ToCppBool(create))) {
    return ToJBool(WrapObjectInJava(frame, env, thiz, true));
  } else {
    delete frame;
    return JNI_FALSE;
  }
}

jboolean Java_android_filterfw_core_GLFrame_allocateWithFbo(JNIEnv* env,
                                                            jobject thiz,
                                                            jint fbo_id,
                                                            jint width,
                                                            jint height,
                                                            jboolean owns,
                                                            jboolean create) {
  GLFrame* frame = new GLFrame();
  if (frame->InitWithFbo(fbo_id, width, height, ToCppBool(owns), ToCppBool(create))) {
    return ToJBool(WrapObjectInJava(frame, env, thiz, true));
  } else {
    delete frame;
    return JNI_FALSE;
  }
}

jboolean Java_android_filterfw_core_GLFrame_allocateExternal(JNIEnv* env, jobject thiz) {
  GLFrame* frame = new GLFrame();
  if (frame->InitWithExternalTexture()) {
    return ToJBool(WrapObjectInJava(frame, env, thiz, true));
  } else {
    delete frame;
    return JNI_FALSE;
  }
}

jboolean Java_android_filterfw_core_GLFrame_deallocate(JNIEnv* env, jobject thiz) {
  return ToJBool(DeleteNativeObject<GLFrame>(env, thiz));
}

jboolean Java_android_filterfw_core_GLFrame_setNativeData(JNIEnv* env,
                                                              jobject thiz,
                                                              jbyteArray data,
                                                              jint offset,
                                                              jint length) {
  GLFrame* frame = ConvertFromJava<GLFrame>(env, thiz);
  if (frame && data) {
    jbyte* bytes = env->GetByteArrayElements(data, NULL);
    if (bytes) {
      const bool success = frame->WriteData(reinterpret_cast<const uint8_t*>(bytes + offset), length);
      env->ReleaseByteArrayElements(data, bytes, JNI_ABORT);
      return ToJBool(success);
    }
  }
  return JNI_FALSE;
}

jbyteArray Java_android_filterfw_core_GLFrame_getNativeData(JNIEnv* env, jobject thiz) {
  GLFrame* frame = ConvertFromJava<GLFrame>(env, thiz);
  if (frame && frame->Size() > 0) {
    jbyteArray result = env->NewByteArray(frame->Size());
    jbyte* data = env->GetByteArrayElements(result, NULL);
    frame->CopyDataTo(reinterpret_cast<uint8_t*>(data), frame->Size());
    env->ReleaseByteArrayElements(result, data, 0);
    return result;
  }
  return NULL;
}

jboolean Java_android_filterfw_core_GLFrame_setNativeInts(JNIEnv* env,
                                                          jobject thiz,
                                                          jintArray ints) {
  GLFrame* frame = ConvertFromJava<GLFrame>(env, thiz);
  if (frame && ints) {
    jint* int_ptr = env->GetIntArrayElements(ints, NULL);
    const int length = env->GetArrayLength(ints);
    if (int_ptr) {
      const bool success = frame->WriteData(reinterpret_cast<const uint8_t*>(int_ptr),
                                            length * sizeof(jint));
      env->ReleaseIntArrayElements(ints, int_ptr, JNI_ABORT);
      return ToJBool(success);
    }
  }
  return JNI_FALSE;
}

jintArray Java_android_filterfw_core_GLFrame_getNativeInts(JNIEnv* env, jobject thiz) {
  GLFrame* frame = ConvertFromJava<GLFrame>(env, thiz);
  if (frame && frame->Size() > 0 && (frame->Size() % sizeof(jint) == 0)) {
    jintArray result = env->NewIntArray(frame->Size() / sizeof(jint));
    jint* data = env->GetIntArrayElements(result, NULL);
    frame->CopyDataTo(reinterpret_cast<uint8_t*>(data), frame->Size());
    env->ReleaseIntArrayElements(result, data, 0);
    return result;
  }
  return NULL;
}

jboolean Java_android_filterfw_core_GLFrame_setNativeFloats(JNIEnv* env,
                                                            jobject thiz,
                                                            jfloatArray floats) {
  GLFrame* frame = ConvertFromJava<GLFrame>(env, thiz);
  if (frame && floats) {
    jfloat* float_ptr = env->GetFloatArrayElements(floats, NULL);
    const int length = env->GetArrayLength(floats);
    if (float_ptr) {
      const bool success = frame->WriteData(reinterpret_cast<const uint8_t*>(float_ptr),
                                            length * sizeof(jfloat));
      env->ReleaseFloatArrayElements(floats, float_ptr, JNI_ABORT);
      return ToJBool(success);
    }
  }
  return JNI_FALSE;
}

jfloatArray Java_android_filterfw_core_GLFrame_getNativeFloats(JNIEnv* env, jobject thiz) {
  GLFrame* frame = ConvertFromJava<GLFrame>(env, thiz);
  if (frame && frame->Size() > 0 && (frame->Size() % sizeof(jfloat) == 0)) {
    jfloatArray result = env->NewFloatArray(frame->Size() / sizeof(jfloat));
    jfloat* data = env->GetFloatArrayElements(result, NULL);
    frame->CopyDataTo(reinterpret_cast<uint8_t*>(data), frame->Size());
    env->ReleaseFloatArrayElements(result, data, 0);
    return result;
  }
  return NULL;
}

jboolean Java_android_filterfw_core_GLFrame_setNativeBitmap(JNIEnv* env,
                                                            jobject thiz,
                                                            jobject bitmap,
                                                            jint size) {
  GLFrame* frame = ConvertFromJava<GLFrame>(env, thiz);
  if (frame && bitmap) {
    uint8_t* pixels;
    const int result = AndroidBitmap_lockPixels(env, bitmap, reinterpret_cast<void**>(&pixels));
    if (result == ANDROID_BITMAP_RESUT_SUCCESS) {
      const bool success = frame->WriteData(pixels, size);
      return ToJBool(success &&
                     AndroidBitmap_unlockPixels(env, bitmap) == ANDROID_BITMAP_RESUT_SUCCESS);
    }
  }
  return JNI_FALSE;
}

jboolean Java_android_filterfw_core_GLFrame_getNativeBitmap(JNIEnv* env,
                                                            jobject thiz,
                                                            jobject bitmap) {
  GLFrame* frame = ConvertFromJava<GLFrame>(env, thiz);
  if (frame && bitmap) {
    uint8_t* pixels;
    const int result = AndroidBitmap_lockPixels(env, bitmap, reinterpret_cast<void**>(&pixels));
    if (result == ANDROID_BITMAP_RESUT_SUCCESS) {
      frame->CopyDataTo(pixels, frame->Size());
      return (AndroidBitmap_unlockPixels(env, bitmap) == ANDROID_BITMAP_RESUT_SUCCESS);
    }
  }
  return JNI_FALSE;
}

jboolean Java_android_filterfw_core_GLFrame_setNativeViewport(JNIEnv* env,
                                                              jobject thiz,
                                                              jint x,
                                                              jint y,
                                                              jint width,
                                                              jint height) {
  GLFrame* frame = ConvertFromJava<GLFrame>(env, thiz);
  return frame ? ToJBool(frame->SetViewport(x, y, width, height)) : JNI_FALSE;
}

jint Java_android_filterfw_core_GLFrame_getNativeTextureId(JNIEnv* env, jobject thiz) {
  GLFrame* frame = ConvertFromJava<GLFrame>(env, thiz);
  return frame ? frame->GetTextureId() : -1;
}

jint Java_android_filterfw_core_GLFrame_getNativeFboId(JNIEnv* env, jobject thiz) {
  GLFrame* frame = ConvertFromJava<GLFrame>(env, thiz);
  return frame ? frame->GetFboId() : -1;
}

jboolean Java_android_filterfw_core_GLFrame_generateNativeMipMap(JNIEnv* env, jobject thiz) {
  GLFrame* frame = ConvertFromJava<GLFrame>(env, thiz);
  return frame ? ToJBool(frame->GenerateMipMap()) : JNI_FALSE;
}

jboolean Java_android_filterfw_core_GLFrame_setNativeTextureParam(JNIEnv* env,
                                                                  jobject thiz,
                                                                  jint param,
                                                                  jint value) {
  GLFrame* frame = ConvertFromJava<GLFrame>(env, thiz);
  return frame ? ToJBool(frame->SetTextureParameter(param, value)) : JNI_FALSE;
}

jboolean Java_android_filterfw_core_GLFrame_nativeCopyFromNative(JNIEnv* env,
                                                                 jobject thiz,
                                                                 jobject frame) {
  GLFrame* this_frame = ConvertFromJava<GLFrame>(env, thiz);
  NativeFrame* other_frame = ConvertFromJava<NativeFrame>(env, frame);
  if (this_frame && other_frame) {
    return ToJBool(this_frame->WriteData(other_frame->Data(), other_frame->Size()));
  }
  return JNI_FALSE;
}

jboolean Java_android_filterfw_core_GLFrame_nativeCopyFromGL(JNIEnv* env,
                                                             jobject thiz,
                                                             jobject frame) {
  GLFrame* this_frame = ConvertFromJava<GLFrame>(env, thiz);
  GLFrame* other_frame = ConvertFromJava<GLFrame>(env, frame);
  if (this_frame && other_frame) {
    return ToJBool(this_frame->CopyPixelsFrom(other_frame));
  }
  return JNI_FALSE;
}

jboolean Java_android_filterfw_core_GLFrame_nativeFocus(JNIEnv* env, jobject thiz) {
    GLFrame* frame = ConvertFromJava<GLFrame>(env, thiz);
    return ToJBool(frame && frame->FocusFrameBuffer());
}
