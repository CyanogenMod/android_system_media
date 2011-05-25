# Copyright (C) 2009 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

LOCAL_PATH := $(call my-dir)

#####################
# Build module libfilterfw2-jni
include $(CLEAR_VARS)

LOCAL_MODULE_TAGS := optional

LOCAL_MODULE    = libfilterfw2-jni_static

LOCAL_SRC_FILES = jni_init.cc \
                  jni_gl_environment.cc \
                  jni_gl_frame.cc \
                  jni_native_buffer.cc \
                  jni_native_frame.cc \
                  jni_native_program.cc \
                  jni_shader_program.cc \
                  jni_util.cc \
                  jni_vertex_frame.cc

# default to read .cc file as c++ file, need to set both variables.
LOCAL_CPP_EXTENSION = .cc
LOCAL_DEFAULT_CPP_EXTENSION = .cc

# Need FilterFW lib
include $(LOCAL_PATH)/../native/libfilterfw.mk

# Also need the JNI headers.
LOCAL_C_INCLUDES += \
	$(JNI_H_INCLUDE) \
	$(LOCAL_PATH)/..

# Don't prelink this library.  For more efficient code, you may want
# to add this library to the prelink map and set this to true. However,
# it's difficult to do this for applications that are not supplied as
# part of a system image.
LOCAL_PRELINK_MODULE := false

include $(BUILD_STATIC_LIBRARY)

#####################
# Build module libfilterfw2-jni
include $(CLEAR_VARS)

LOCAL_MODULE := libfilterfw2-jni

LOCAL_MODULE_TAGS := optional

LOCAL_WHOLE_STATIC_LIBRARIES := libfilterfw2-jni_static

LOCAL_SHARED_LIBRARIES := libfilterfw2 \
                          libstlport \
                          libGLESv2 \
                          libEGL \
                          libutils \
                          libandroid \
                          libjnigraphics

# Don't prelink this library.  For more efficient code, you may want
# to add this library to the prelink map and set this to true. However,
# it's difficult to do this for applications that are not supplied as
# part of a system image.
LOCAL_PRELINK_MODULE := false

include $(BUILD_SHARED_LIBRARY)

