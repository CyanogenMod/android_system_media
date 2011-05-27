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
# Build module libfilterfw_static

include $(CLEAR_VARS)

LOCAL_MODULE_TAGS := optional

LOCAL_MODULE := libfilterfw_static

# Compile source files
NDK_FILES = filter/src/data_buffer.cc \
            filter/src/native_buffer.cc \
            filter/src/value.cc

LOCAL_SRC_FILES += core/geometry.cc \
                   core/gl_env.cc \
                   core/gl_frame.cc \
                   core/native_frame.cc \
                   core/native_program.cc \
                   core/shader_program.cc \
                   core/vertex_frame.cc \
                   $(NDK_FILES)

# default to read .cc file as c++ file, need to set both variables.
LOCAL_CPP_EXTENSION := .cc
LOCAL_DEFAULT_CPP_EXTENSION := .cc

# add local includes
include $(LOCAL_PATH)/libfilterfw.mk

# gcc should always be placed at the end.
LOCAL_EXPORT_LDLIBS := -llog -lgcc

# TODO: Build a shared library as well?
include $(BUILD_STATIC_LIBRARY)

#####################
# Build module libfilterfw.so

include $(CLEAR_VARS)

LOCAL_MODULE := libfilterfw

LOCAL_MODULE_TAGS := optional

LOCAL_WHOLE_STATIC_LIBRARIES := libfilterfw_static

LOCAL_SHARED_LIBRARIES := libandroid \
                          libGLESv2 \
                          libEGL \
                          libgui \
                          libdl \
                          libstlport \
                          libcutils \
                          libutils \

# Don't prelink this library.  For more efficient code, you may want
# to add this library to the prelink map and set this to true. However,
# it's difficult to do this for applications that are not supplied as
# part of a system image.
LOCAL_PRELINK_MODULE := false

include $(BUILD_SHARED_LIBRARY)
