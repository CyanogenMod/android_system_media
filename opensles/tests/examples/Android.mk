LOCAL_PATH:= $(call my-dir)

#

include $(CLEAR_VARS)

LOCAL_MODULE_TAGS := tests

LOCAL_C_INCLUDES:= \
	system/media/opensles/include

LOCAL_SRC_FILES:= \
	slesTestRecBuffQueue.cpp

LOCAL_SHARED_LIBRARIES := \
	libutils \
	libOpenSLES

ifeq ($(TARGET_OS),linux)
	LOCAL_CFLAGS += -DXP_UNIX
endif

LOCAL_MODULE:= slesTest_recBuffQueue

include $(BUILD_EXECUTABLE)

#

include $(CLEAR_VARS)

LOCAL_MODULE_TAGS := tests

LOCAL_C_INCLUDES:= \
	system/media/opensles/include

LOCAL_SRC_FILES:= \
	slesTestPlayFdPath.cpp

LOCAL_SHARED_LIBRARIES := \
	libutils \
	libOpenSLES

ifeq ($(TARGET_OS),linux)
	LOCAL_CFLAGS += -DXP_UNIX
endif

LOCAL_MODULE:= slesTest_playFdPath

include $(BUILD_EXECUTABLE)

#

include $(CLEAR_VARS)

LOCAL_MODULE_TAGS := tests

LOCAL_C_INCLUDES:= \
	$(JNI_H_INCLUDE) \
	system/media/opensles/include

LOCAL_SRC_FILES:= \
    feedback.c

LOCAL_SHARED_LIBRARIES := \
	libutils \
	libOpenSLES

ifeq ($(TARGET_OS),linux)
	LOCAL_CFLAGS += -DXP_UNIX
	#LOCAL_SHARED_LIBRARIES += librt
endif

LOCAL_MODULE:= slesTest_feedback

include $(BUILD_EXECUTABLE)

#

include $(CLEAR_VARS)

LOCAL_MODULE_TAGS := tests

LOCAL_C_INCLUDES:= \
	$(JNI_H_INCLUDE) \
	system/media/opensles/include

LOCAL_SRC_FILES:= \
    bufferQueue.c \
	playSawtoothBufferQueue.cpp

LOCAL_SHARED_LIBRARIES := \
	libutils \
	libOpenSLES

ifeq ($(TARGET_OS),linux)
	LOCAL_CFLAGS += -DXP_UNIX
	#LOCAL_SHARED_LIBRARIES += librt
endif

LOCAL_MODULE:= slesTest_sawtoothBufferQueue

include $(BUILD_EXECUTABLE)

#

include $(CLEAR_VARS)

LOCAL_MODULE_TAGS := tests

LOCAL_C_INCLUDES:= \
	system/media/opensles/include

LOCAL_SRC_FILES:= \
	slesTestEqFdPath.cpp

LOCAL_SHARED_LIBRARIES := \
	libutils \
	libOpenSLES

ifeq ($(TARGET_OS),linux)
	LOCAL_CFLAGS += -DXP_UNIX
endif

LOCAL_MODULE:= slesTest_eqFdPath

include $(BUILD_EXECUTABLE)
