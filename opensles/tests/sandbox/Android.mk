LOCAL_PATH:= $(call my-dir)

# intbufq

include $(CLEAR_VARS)

LOCAL_MODULE_TAGS := tests

LOCAL_C_INCLUDES:= \
	system/media/opensles/include

LOCAL_SRC_FILES:= \
    intbufq.c \
	getch.c

LOCAL_SHARED_LIBRARIES := \
	libutils \
	libOpenSLES

ifeq ($(TARGET_OS),linux)
	LOCAL_CFLAGS += -DXP_UNIX
	#LOCAL_SHARED_LIBRARIES += librt
endif

LOCAL_MODULE:= slesTest_intbufq

include $(BUILD_EXECUTABLE)

# multiplay

include $(CLEAR_VARS)

LOCAL_MODULE_TAGS := tests

LOCAL_C_INCLUDES:= \
	system/media/opensles/include

LOCAL_SRC_FILES:= \
	multiplay.c

LOCAL_SHARED_LIBRARIES := \
	libutils \
	libOpenSLES

ifeq ($(TARGET_OS),linux)
	LOCAL_CFLAGS += -DXP_UNIX
endif

LOCAL_MODULE:= slesTest_multiplay

include $(BUILD_EXECUTABLE)

# engine

include $(CLEAR_VARS)

LOCAL_MODULE_TAGS := tests

LOCAL_C_INCLUDES:= \
	system/media/opensles/include

LOCAL_SRC_FILES:= \
	engine.c

LOCAL_SHARED_LIBRARIES := \
	libutils \
	libOpenSLES

LOCAL_STATIC_LIBRARIES := \
    libOpenSLESUT

ifeq ($(TARGET_OS),linux)
	LOCAL_CFLAGS += -DXP_UNIX
endif

LOCAL_MODULE:= slesTest_engine

include $(BUILD_EXECUTABLE)

# object

include $(CLEAR_VARS)

LOCAL_MODULE_TAGS := tests

LOCAL_C_INCLUDES:= \
	system/media/opensles/include

LOCAL_SRC_FILES:= \
	object.c

LOCAL_SHARED_LIBRARIES := \
	libutils \
	libOpenSLES

LOCAL_STATIC_LIBRARIES := \
    libOpenSLESUT

ifeq ($(TARGET_OS),linux)
	LOCAL_CFLAGS += -DXP_UNIX
endif

LOCAL_MODULE:= slesTest_object

include $(BUILD_EXECUTABLE)

# configbq

include $(CLEAR_VARS)

LOCAL_MODULE_TAGS := tests

LOCAL_C_INCLUDES:= \
	system/media/opensles/include

LOCAL_SRC_FILES:= \
	configbq.c

LOCAL_SHARED_LIBRARIES := \
	libutils \
	libOpenSLES

LOCAL_STATIC_LIBRARIES := \
    libOpenSLESUT

ifeq ($(TARGET_OS),linux)
	LOCAL_CFLAGS += -DXP_UNIX
endif

LOCAL_CFLAGS += -UNDEBUG

LOCAL_MODULE:= slesTest_configbq

include $(BUILD_EXECUTABLE)
