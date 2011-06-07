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

#ifndef ANDROID_FILTERFW_CORE_NATIVE_PROGRAM_H
#define ANDROID_FILTERFW_CORE_NATIVE_PROGRAM_H

#include <vector>
#include <string>

#include "base/utilities.h"

#include "filter/value.h"
#include "filter/native_buffer.h"
#include "filter/src/data_buffer.h"

namespace android {
namespace filterfw {

class NativeFrame;

typedef void (*InitFunctionPtr)(void**);
typedef void (*SetValueFunctionPtr)(const char*, Value, void*);
typedef Value (*GetValueFunctionPtr)(const char*, void*);
typedef int (*ProcessFunctionPtr)(const NativeBuffer*, int, NativeBuffer, void*);
typedef void (*TeardownFunctionPtr)(void*);

class NativeProgram {
  public:
    // Create an empty native frame.
    NativeProgram();

    ~NativeProgram();

    bool OpenLibrary(const std::string& lib_name);

    bool BindInitFunction(const std::string& func_name);
    bool BindSetValueFunction(const std::string& func_name);
    bool BindGetValueFunction(const std::string& func_name);
    bool BindProcessFunction(const std::string& func_name);
    bool BindTeardownFunction(const std::string& func_name);

    bool CallInit();
    bool CallSetValue(const std::string& key, Value value);
    Value CallGetValue(const std::string& key);
    bool CallProcess(const std::vector<DataBuffer*>& inputs,
                     int size,
                     DataBuffer* output);
    bool CallTeardown();

  private:
    // Pointer to the data. Owned by the frame.
    void* lib_handle_;

    // The function pointers to the native function implementations.
    InitFunctionPtr init_function_;
    SetValueFunctionPtr setvalue_function_;
    GetValueFunctionPtr getvalue_function_;
    ProcessFunctionPtr process_function_;
    TeardownFunctionPtr teardown_function_;

    // Pointer to user data
    void* user_data_;

    DISALLOW_COPY_AND_ASSIGN(NativeProgram);
};

} // namespace filterfw
} // namespace android

#endif  // ANDROID_FILTERFW_CORE_NATIVE_PROGRAM_H
