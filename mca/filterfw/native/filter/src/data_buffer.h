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

#ifndef ANDROID_FILTERFW_FILTER_DATA_BUFFER_H
#define ANDROID_FILTERFW_FILTER_DATA_BUFFER_H

#include <jni.h>
#include <stdlib.h>

class DataBuffer {
  public:
    DataBuffer()
      : data_(NULL),
        size_(0),
        is_mutable_(false) {
    }

    DataBuffer(int size)
      : data_(new char[size]),
        size_(size),
        is_mutable_(false) {
    }

    DataBuffer(char* data, int size)
      : data_(data),
        size_(size),
        is_mutable_(false) {
    }

    void DeleteData() {
      delete[] data_;
      data_ = NULL;
      size_ = 0;
    }

    void AllocateData(int buffer_size) {
      if (!data_) {
        data_ = buffer_size == 0 ? NULL : new char[buffer_size];
        size_ = buffer_size;
      }
    }

    bool SetData(char* data, int size) {
      if (!data_) {
        data_ = data;
        size_ = size;
        return true;
      }
      return false;
    }

    bool CopyFrom(const DataBuffer& buffer) {
      if (!data_ && buffer.data_) {
        AllocateData(buffer.size_);
        memcpy(data_, buffer.data_, buffer.size_);
        return true;
      }
      return false;
    }

    char* data() const {
      return data_;
    }

    int size() const {
      return size_;
    }

    bool is_mutable() const {
      return is_mutable_;
    }

    void set_mutable(bool flag) {
      is_mutable_ = flag;
    }

    static bool FromJavaObject(JNIEnv* env, jobject buffer, DataBuffer* result);

    bool AttachToJavaObject(JNIEnv* env, jobject buffer);

  private:
    char* data_;
    int size_;
    bool is_mutable_;

    // Disallow copying
    DataBuffer(const DataBuffer&);
    DataBuffer& operator=(const DataBuffer&);
};

#endif  // ANDROID_FILTERFW_FILTER_DATA_BUFFER_H
