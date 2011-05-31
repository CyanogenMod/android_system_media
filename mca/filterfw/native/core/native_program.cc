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

#include <dlfcn.h>

#include "base/logging.h"
#include "core/native_frame.h"
#include "core/native_program.h"

#include <string>
#include <vector>

namespace android {
namespace filterfw {

NativeProgram::NativeProgram()
    : lib_handle_(NULL),
      init_function_(NULL),
      setvalue_function_(NULL),
      process_function_(NULL),
      teardown_function_(NULL),
      user_data_(NULL) {
}

NativeProgram::~NativeProgram() {
  if (lib_handle_)
    dlclose(lib_handle_);
}

bool NativeProgram::OpenLibrary(const std::string& lib_name) {
  if (!lib_handle_) {
    lib_handle_ = dlopen(lib_name.c_str(), RTLD_NOW);
    if (!lib_handle_) {
      LOGE("NativeProgram: Could not find library: '%s'!", lib_name.c_str());
      return false;
    }
    return true;
  }
  return false;
}

bool NativeProgram::BindProcessFunction(const std::string& func_name) {
  if (!lib_handle_)
    return false;
  process_function_ = reinterpret_cast<ProcessFunctionPtr>(dlsym(lib_handle_, func_name.c_str()));
  if (!process_function_) {
    LOGE("NativeProgram: Could not find process function symbol: '%s'!", func_name.c_str());
    return false;
  }
  return true;
}

bool NativeProgram::BindInitFunction(const std::string& func_name) {
  if (!lib_handle_)
    return false;
  init_function_ = reinterpret_cast<InitFunctionPtr>(dlsym(lib_handle_, func_name.c_str()));
  return init_function_ != NULL;
}

bool NativeProgram::BindSetValueFunction(const std::string& func_name) {
  if (!lib_handle_)
    return false;
  setvalue_function_ = reinterpret_cast<SetValueFunctionPtr>(dlsym(lib_handle_, func_name.c_str()));
  return setvalue_function_ != NULL;
}

bool NativeProgram::BindGetValueFunction(const std::string& func_name) {
  if (!lib_handle_)
    return false;
  getvalue_function_ = reinterpret_cast<GetValueFunctionPtr>(dlsym(lib_handle_, func_name.c_str()));
  return getvalue_function_ != NULL;
}

bool NativeProgram::BindTeardownFunction(const std::string& func_name) {
  if (!lib_handle_)
    return false;
  teardown_function_ = reinterpret_cast<TeardownFunctionPtr>(dlsym(lib_handle_, func_name.c_str()));
  return teardown_function_ != NULL;
}

bool NativeProgram::CallProcess(const std::vector<DataBuffer*>& inputs,
                                int size,
                                DataBuffer* output) {
  if (process_function_) {
    process_function_(reinterpret_cast<const NativeBuffer*>(&inputs[0]),
                      size,
                      reinterpret_cast<NativeBuffer>(output),
                      user_data_);
    return true;
  }
  return false;
}

bool NativeProgram::CallInit() {
  if (init_function_) {
    init_function_(&user_data_);
    return true;
  }
  return false;
}

bool NativeProgram::CallSetValue(const std::string& key, Value value) {
  if (setvalue_function_) {
    setvalue_function_(key.c_str(), value, user_data_);
    return true;
  }
  return false;
}

Value NativeProgram::CallGetValue(const std::string& key) {
  if (getvalue_function_) {
    Value value = getvalue_function_(key.c_str(), user_data_);
    return value;
  }
  return MakeNullValue();
}

bool NativeProgram::CallTeardown() {
  if (teardown_function_) {
    teardown_function_(user_data_);
    return true;
  }
  return false;
}

} // namespace filterfw
} // namespace android
