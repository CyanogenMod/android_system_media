LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE_TAGS := tests
LOCAL_MODULE    := libnative-media-jni
LOCAL_SRC_FILES := native-media-jni.c
LOCAL_CFLAGS += -Isystem/media/opensles/include

LOCAL_PRELINK_MODULE := false
LOCAL_SHARED_LIBRARIES += libutils libOpenSLES

include $(BUILD_SHARED_LIBRARY)
