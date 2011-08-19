LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE_TAGS := tests
LOCAL_MODULE    := libnative-media-jni
LOCAL_SRC_FILES := native-media-jni.c
LOCAL_CFLAGS += -Isystem/media/wilhelm/include
LOCAL_CFLAGS += -UNDEBUG

LOCAL_PRELINK_MODULE := false
LOCAL_SHARED_LIBRARIES += libutils libOpenMAXAL libandroid


include $(BUILD_SHARED_LIBRARY)
