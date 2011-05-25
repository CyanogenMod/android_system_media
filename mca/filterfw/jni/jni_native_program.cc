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

#include "jni/jni_native_program.h"
#include "jni/jni_util.h"

#include "native/base/basictypes.h"
#include "native/base/logging.h"
#include "native/core/native_frame.h"
#include "native/core/native_program.h"
#include "native/filter/src/data_buffer.h"

using android::filterfw::NativeFrame;
using android::filterfw::NativeProgram;

jboolean Java_android_filterfw_core_NativeProgram_allocate(JNIEnv* env, jobject thiz) {
  return ToJBool(WrapObjectInJava(new NativeProgram(), env, thiz, true));
}

jboolean Java_android_filterfw_core_NativeProgram_deallocate(JNIEnv* env, jobject thiz) {
  return ToJBool(DeleteNativeObject<NativeProgram>(env, thiz));
}

jboolean Java_android_filterfw_core_NativeProgram_nativeInit(JNIEnv* env, jobject thiz) {
  NativeProgram* program = ConvertFromJava<NativeProgram>(env, thiz);
  return ToJBool(program && program->CallInit());
}

jboolean Java_android_filterfw_core_NativeProgram_openNativeLibrary(JNIEnv* env,
                                                                    jobject thiz,
                                                                    jstring lib_name) {
  NativeProgram* program = ConvertFromJava<NativeProgram>(env, thiz);
  return ToJBool(program && lib_name && program->OpenLibrary(ToCppString(env, lib_name)));
}

jboolean Java_android_filterfw_core_NativeProgram_bindInitFunction(JNIEnv* env,
                                                                   jobject thiz,
                                                                   jstring func_name) {
  NativeProgram* program = ConvertFromJava<NativeProgram>(env, thiz);
  return ToJBool(program && func_name && program->BindInitFunction(ToCppString(env, func_name)));
}

jboolean Java_android_filterfw_core_NativeProgram_bindSetValueFunction(JNIEnv* env,
                                                                       jobject thiz,
                                                                       jstring func_name) {
  NativeProgram* program = ConvertFromJava<NativeProgram>(env, thiz);
  return ToJBool(program &&
                 func_name &&
                 program->BindSetValueFunction(ToCppString(env, func_name)));
}

jboolean Java_android_filterfw_core_NativeProgram_bindGetValueFunction(JNIEnv* env,
                                                                       jobject thiz,
                                                                       jstring func_name) {
  NativeProgram* program = ConvertFromJava<NativeProgram>(env, thiz);
  return ToJBool(program &&
                 func_name &&
                 program->BindGetValueFunction(ToCppString(env, func_name)));
}

jboolean Java_android_filterfw_core_NativeProgram_bindProcessFunction(JNIEnv* env,
                                                                      jobject thiz,
                                                                      jstring func_name) {
  NativeProgram* program = ConvertFromJava<NativeProgram>(env, thiz);
  return ToJBool(program && func_name && program->BindProcessFunction(ToCppString(env, func_name)));
}

jboolean Java_android_filterfw_core_NativeProgram_bindTeardownFunction(JNIEnv* env,
                                                                       jobject thiz,
                                                                       jstring func_name) {
  NativeProgram* program = ConvertFromJava<NativeProgram>(env, thiz);
  return ToJBool(program &&
                 func_name &&
                 program->BindTeardownFunction(ToCppString(env, func_name)));
}

jboolean Java_android_filterfw_core_NativeProgram_callNativeInit(JNIEnv* env, jobject thiz) {
  NativeProgram* program = ConvertFromJava<NativeProgram>(env, thiz);
  return ToJBool(program && program->CallInit());
}

jboolean Java_android_filterfw_core_NativeProgram_callNativeSetValue(JNIEnv* env,
                                                                     jobject thiz,
                                                                     jstring key,
                                                                     jobject value) {
  if (!value) {
    LOGE("Native Program: Attempting to set null value for key %s!",
         ToCppString(env, key).c_str());
  }
  NativeProgram* program = ConvertFromJava<NativeProgram>(env, thiz);
  const Value c_value = ToCValue(env, value);
  const string c_key = ToCppString(env, key);
  if (!ValueIsNull(c_value)) {
    return ToJBool(program && program->CallSetValue(c_key, c_value));
  } else {
    LOGE("NativeProgram: Could not convert java object value passed for key '%s'!", c_key.c_str());
    return JNI_FALSE;
  }
}

jobject Java_android_filterfw_core_NativeProgram_callNativeGetValue(JNIEnv* env,
                                                                    jobject thiz,
                                                                    jstring key) {
  NativeProgram* program = ConvertFromJava<NativeProgram>(env, thiz);
  const string c_key = ToCppString(env, key);
  if (program) {
    Value result = program->CallGetValue(c_key);
    return ToJObject(env, result);
  }
  return JNI_FALSE;
}

jboolean Java_android_filterfw_core_NativeProgram_callNativeProcess(JNIEnv* env,
                                                                    jobject thiz,
                                                                    jobjectArray inputs,
                                                                    jobject output) {
  NativeProgram* program = ConvertFromJava<NativeProgram>(env, thiz);

  // Sanity checks
  if (!program || !inputs) {
    return JNI_FALSE;
  } else if (!output) {
    LOGE("NativeProgram: Null-frame given as output!");
    return JNI_FALSE;
  }

  // Get the input buffers
  bool is_copy;
  bool err = false;
  const int input_count = env->GetArrayLength(inputs);
  vector<DataBuffer*> input_buffers(input_count, NULL);
  for (int i = 0 ; i < input_count; ++i) {
    jobject input = env->GetObjectArrayElement(inputs, i);
    if (!input) {
      LOGE("NativeProgram: Null-frame given as input %d!", i);
      err = true;
      break;
    }
    NativeFrame* native_frame = ConvertFromJava<NativeFrame>(env, input);
    if (!native_frame) {
      LOGE("NativeProgram: Could not grab NativeFrame input %d!", i);
      err = true;
      break;
    }
    input_buffers[i] = new DataBuffer(reinterpret_cast<char*>(native_frame->MutableData()),
                                      native_frame->Size());
  }

  // Get the output buffer
  if (!err) {
    DataBuffer output_buffer;
    NativeFrame* output_frame = ConvertFromJava<NativeFrame>(env, output);
    if (!output_frame) {
      LOGE("NativeProgram: Could not grab NativeFrame output!");
      err = true;;
    }

    if (!err) {
      output_buffer.SetData(reinterpret_cast<char*>(output_frame->MutableData()),
                            output_frame->Size());
      output_buffer.set_mutable(true);
      const bool output_was_empty = (output_buffer.data() == NULL);

      // Process the frames!
      program->CallProcess(input_buffers, input_count, &output_buffer);

      // Attach buffer to output frame if this was allocated by the program
      if (output_was_empty && output_buffer.data()) {
        output_frame->SetData(reinterpret_cast<uint8*>(output_buffer.data()), output_buffer.size());
      }
    }
  }

  // Delete input objects
  for (int i = 0; i < input_count; ++i) {
    delete input_buffers[i];
  }

  return err ? JNI_FALSE : JNI_TRUE;
}

jboolean Java_android_filterfw_core_NativeProgram_callNativeTeardown(JNIEnv* env, jobject thiz) {
  NativeProgram* program = ConvertFromJava<NativeProgram>(env, thiz);
  return ToJBool(program && program->CallTeardown());
}

