# Build only if NOT simulator
ifneq ($(TARGET_SIMULATOR),true)

LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_MODULE_TAGS := tests

LOCAL_C_INCLUDES:= \
    bionic \
    bionic/libstdc++/include \
    external/gtest/include \
    $(JNI_H_INCLUDE) \
    $(TOP)/system/media/opensles/include \
    external/stlport/stlport

LOCAL_SRC_FILES:= \
    BufferQueue_test.cpp

LOCAL_SHARED_LIBRARIES := \
	libutils \
	libOpenSLES \
    libstlport

LOCAL_STATIC_LIBRARIES := \
    libgtest

ifeq ($(TARGET_OS),linux)
	LOCAL_CFLAGS += -DXP_UNIX
	#LOCAL_SHARED_LIBRARIES += librt
endif

LOCAL_MODULE:= BufferQueue_test

include $(BUILD_EXECUTABLE)

endif
