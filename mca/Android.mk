# Copyright (C) 2011 The Android Open Source Project
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

FILTERFW_PATH:= $(call my-dir)

# for shared defintion of libfilterfw_to_document
include $(FILTERFW_PATH)/Docs.mk

#
# Build all native libraries
#
include $(call all-subdir-makefiles)

#
# Build Java library from filterfw core and all open-source filterpacks
#

LOCAL_PATH := $(FILTERFW_PATH)
include $(CLEAR_VARS)

LOCAL_MODULE_TAGS := optional

LOCAL_SRC_FILES := $(call libfilterfw_to_document,$(LOCAL_PATH))

LOCAL_MODULE := filterfw

LOCAL_JNI_SHARED_LIBRARIES := libfilterfw libfilterpack_imageproc

LOCAL_PROGUARD_ENABLED := disabled

LOCAL_NO_STANDARD_LIBRARIES := true
LOCAL_JAVA_LIBRARIES := core core-junit ext framework # to avoid circular dependency

include $(BUILD_JAVA_LIBRARY)

#
# Local droiddoc for faster libfilterfw docs testing
#
#
# Run with:
#     m libfilterfw-docs
#
# Main output:
#     out/target/common/docs/libfilterfw/reference/packages.html
#
# All text for proofreading (or running tools over):
#     out/target/common/docs/libfilterfw-proofread.txt
#
# TODO list of missing javadoc, etc:
#     out/target/common/docs/libfilterfw-docs-todo.html
#
# Rerun:
#     rm -rf out/target/common/docs/libfilterfw-timestamp && m libfilterfw-docs
#

LOCAL_PATH := $(FILTERFW_PATH)
include $(CLEAR_VARS)

LOCAL_SRC_FILES:=$(call libfilterfw_to_document,$(LOCAL_PATH))

# rerun doc generation without recompiling the java
LOCAL_JAVA_LIBRARIES:=
LOCAL_MODULE_CLASS:=JAVA_LIBRARIES

LOCAL_MODULE := libfilterfw

LOCAL_DROIDDOC_OPTIONS:= \
 -offlinemode \
 -title "libfilterfw" \
 -proofread $(OUT_DOCS)/$(LOCAL_MODULE)-proofread.txt \
 -todo ../$(LOCAL_MODULE)-docs-todo.html \
 -hdf android.whichdoc offline

LOCAL_DROIDDOC_CUSTOM_TEMPLATE_DIR:=build/tools/droiddoc/templates-sdk

include $(BUILD_DROIDDOC)
