LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_MODULE_TAGS := tests

LOCAL_C_INCLUDES:= \
	$(JNI_H_INCLUDE) \
	$(TOP)/system/media/opensles/include

LOCAL_SRC_FILES:= \
	slesTestPlayUri.cpp

LOCAL_SHARED_LIBRARIES := \
	libutils \
	libopensles
	

ifeq ($(TARGET_OS),linux)
	LOCAL_CFLAGS += -DXP_UNIX
endif

LOCAL_MODULE:= slesTest_playUri

include $(BUILD_EXECUTABLE)
