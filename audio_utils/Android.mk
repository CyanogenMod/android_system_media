LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE := libaudioutils
LOCAL_MODULE_TAGS := optional

LOCAL_SRC_FILES:= \
	ReSampler.cpp \
	EchoReference.cpp

LOCAL_C_INCLUDES += $(call include-path-for, speex)
LOCAL_C_INCLUDES += \
	$(call include-path-for, speex) \
	system/media/audio_utils/include

LOCAL_SHARED_LIBRARIES := \
	libutils \
  libspeexresampler

include $(BUILD_SHARED_LIBRARY)
