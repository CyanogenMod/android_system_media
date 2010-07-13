LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

LOCAL_SRC_FILES := \
        OpenSLUT.c

LOCAL_C_INCLUDES:=                                                  \
	system/media/opensles/include

LOCAL_MODULE := libOpenSLUT

include $(BUILD_STATIC_LIBRARY)

include $(CLEAR_VARS)

LOCAL_CFLAGS += -Wno-override-init -Wno-missing-field-initializers

LOCAL_SRC_FILES:=                     \
        MPH_to.c

LOCAL_MODULE:= libopensles_helper

include $(BUILD_STATIC_LIBRARY)

include $(CLEAR_VARS)

LOCAL_CFLAGS += -DUSE_CONFORMANCE
# -DUSE_TRACE

LOCAL_SRC_FILES:=                     \
        OpenSLES_IID.c                \
        classes.c                     \
        devices.c                     \
        trace.c                       \
        interfaces.c                  \
        locks.c                       \
        sles.c                        \
        sles_to_android.cpp           \
        android_SfPlayer.cpp          \
        sync.c                        \
        IID_to_MPH.c                  \
        ThreadPool.c                  \
        CAudioPlayer.c                \
        CEngine.c                     \
        I3DCommit.c                   \
        I3DDoppler.c                  \
        I3DGrouping.c                 \
        I3DLocation.c                 \
        I3DMacroscopic.c              \
        I3DSource.c                   \
        IAndroidStreamType.c          \
        IAudioDecoderCapabilities.c   \
        IAudioEncoder.c               \
        IAudioEncoderCapabilities.c   \
        IAudioIODeviceCapabilities.c  \
        IBassBoost.c                  \
        IBufferQueue.c                \
        IDeviceVolume.c               \
        IDynamicInterfaceManagement.c \
        IDynamicSource.c              \
        IEffectSend.c                 \
        IEngine.c                     \
        IEngineCapabilities.c         \
        IEnvironmentalReverb.c        \
        IEqualizer.c                  \
        ILEDArray.c                   \
        IMIDIMessage.c                \
        IMIDIMuteSolo.c               \
        IMIDITempo.c                  \
        IMIDITime.c                   \
        IMetadataExtraction.c         \
        IMetadataTraversal.c          \
        IMuteSolo.c                   \
        IObject.c                     \
        IOutputMix.c                  \
        IPitch.c                      \
        IPlay.c                       \
        IPlaybackRate.c               \
        IPrefetchStatus.c             \
        IPresetReverb.c               \
        IRatePitch.c                  \
        IRecord.c                     \
        ISeek.c                       \
        IThreadSync.c                 \
        IVibra.c                      \
        IVirtualizer.c                \
        IVisualization.c              \
        IVolume.c

LOCAL_C_INCLUDES:=                                                  \
	$(JNI_H_INCLUDE)                                                \
	system/media/opensles/include                                   \
	frameworks/base/media/libstagefright                            \
	frameworks/base/media/libstagefright/include                    \
	external/opencore/extern_libs_v2/khronos/openmax/include

LOCAL_CFLAGS += -x c++ -Wno-multichar -Wno-invalid-offsetof

LOCAL_STATIC_LIBRARIES += \
        libopensles_helper        \
        libOpenSLUT

LOCAL_SHARED_LIBRARIES :=         \
        libstagefright            \
        libstagefright_foundation \
        libmedia                  \
        libutils                  \
        libcutils                 \
        libbinder

ifeq ($(TARGET_OS)-$(TARGET_SIMULATOR),linux-true)
        LOCAL_LDLIBS += -lpthread -ldl
        LOCAL_SHARED_LIBRARIES += libdvm
        LOCAL_CPPFLAGS += -DANDROID_SIMULATOR
endif

ifneq ($(TARGET_SIMULATOR),true)
LOCAL_SHARED_LIBRARIES += libdl
endif

ifeq ($(TARGET_OS)-$(TARGET_SIMULATOR),linux-true)
        LOCAL_LDLIBS += -lpthread
endif

LOCAL_PRELINK_MODULE:= false

LOCAL_MODULE:= libOpenSLES

include $(BUILD_SHARED_LIBRARY)

include $(call all-makefiles-under,$(LOCAL_PATH))
