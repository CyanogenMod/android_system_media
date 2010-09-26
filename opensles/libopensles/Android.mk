LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

LOCAL_SRC_FILES := \
        OpenSLESUT.c

LOCAL_C_INCLUDES:= \
        system/media/opensles/include

LOCAL_MODULE := libOpenSLESUT

include $(BUILD_STATIC_LIBRARY)

include $(CLEAR_VARS)

LOCAL_CFLAGS += -Wno-override-init -Wno-missing-field-initializers
# optional, see comments in MPH_to.c: -DUSE_DESIGNATED_INITIALIZERS -S

LOCAL_SRC_FILES:=                     \
        MPH_to.c

LOCAL_MODULE:= libopensles_helper

include $(BUILD_STATIC_LIBRARY)

include $(CLEAR_VARS)

LOCAL_CFLAGS += -DUSE_PROFILES=0 -DUSE_TRACE -DUSE_DEBUG -UNDEBUG \
-DUSE_LOG=SLAndroidLogLevel_Verbose
#-DSL_TRACE_DEFAULT=SL_TRACE_ALL

# Reduce size of .so and hide internal global symbols
LOCAL_CFLAGS += -fvisibility=hidden -DSLAPIENTRY='__attribute__((visibility("default")))'

LOCAL_SRC_FILES:=                     \
        OpenSLES_IID.c                \
        classes.c                     \
        devices.c                     \
        trace.c                       \
        locks.c                       \
        sles.c                        \
        sllog.c                       \
        android_AudioPlayer.cpp       \
        android_AudioRecorder.cpp     \
        android_OutputMix.cpp         \
        sync.c                        \
        IID_to_MPH.c                  \
        ThreadPool.c                  \
        C3DGroup.c                    \
        CAudioPlayer.c                \
        CAudioRecorder.c              \
        CEngine.c                     \
        COutputMix.c                  \
        IAndroidConfiguration.c       \
        IAndroidEffect.c              \
        IAndroidEffectCapabilities.c  \
        IAndroidEffectSend.c          \
        IBassBoost.c                  \
        IBufferQueue.c                \
        IEffectSend.c                 \
        IEngine.c                     \
        IEnvironmentalReverb.c        \
        IEqualizer.c                  \
        IMuteSolo.c                   \
        IObject.c                     \
        IPlay.c                       \
        IPrefetchStatus.c             \
        IPresetReverb.c               \
        IRecord.c                     \
        ISeek.c                       \
        IVirtualizer.c                \
        IVolume.c

EXCLUDE_SRC :=                        \
        I3DCommit.c                   \
        I3DDoppler.c                  \
        I3DGrouping.c                 \
        I3DLocation.c                 \
        I3DMacroscopic.c              \
        I3DSource.c                   \
        IAudioDecoderCapabilities.c   \
        IAudioEncoder.c               \
        IAudioEncoderCapabilities.c   \
        IAudioIODeviceCapabilities.c  \
        IDeviceVolume.c               \
        IDynamicInterfaceManagement.c \
        IDynamicSource.c              \
        IEngineCapabilities.c         \
        ILEDArray.c                   \
        IMIDIMessage.c                \
        IMIDIMuteSolo.c               \
        IMIDITempo.c                  \
        IMIDITime.c                   \
        IMetadataExtraction.c         \
        IMetadataTraversal.c          \
        IOutputMix.c                  \
        IPitch.c                      \
        IPlaybackRate.c               \
        IRatePitch.c                  \
        IThreadSync.c                 \
        IVibra.c                      \
        IVisualization.c

# comment out for USE_BACKPORT
LOCAL_SRC_FILES += \
        android_SfPlayer.cpp          \
        android_Effect.cpp

LOCAL_C_INCLUDES:=                                                  \
        $(JNI_H_INCLUDE)                                            \
        system/media/opensles/include

# comment out for USE_BACKPORT
LOCAL_C_INCLUDES += \
    frameworks/base/media/libstagefright                            \
    frameworks/base/media/libstagefright/include                    \
    external/opencore/extern_libs_v2/khronos/openmax/include

LOCAL_CFLAGS += -x c++ -Wno-multichar -Wno-invalid-offsetof

LOCAL_STATIC_LIBRARIES += \
        libopensles_helper        \
        libOpenSLESUT

LOCAL_SHARED_LIBRARIES :=         \
        libutils                  \
        libmedia                  \
        libbinder

# comment out for USE_BACKPORT
LOCAL_SHARED_LIBRARIES +=         \
        libstagefright            \
        libstagefright_foundation \
        libcutils

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
