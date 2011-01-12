LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

LOCAL_SRC_FILES :=     \
        ut/OpenSLESUT.c   \
        ut/slesutResult.c

LOCAL_C_INCLUDES:= \
        system/media/wilhelm/include

LOCAL_CFLAGS += -fvisibility=hidden

LOCAL_MODULE := libOpenSLESUT

include $(BUILD_STATIC_LIBRARY)

include $(CLEAR_VARS)

LOCAL_CFLAGS += -Wno-override-init
# -Wno-missing-field-initializers
# optional, see comments in MPH_to.c: -DUSE_DESIGNATED_INITIALIZERS -S
LOCAL_CFLAGS += -DUSE_DESIGNATED_INITIALIZERS

LOCAL_SRC_FILES:=                     \
        MPH_to.c

LOCAL_MODULE:= libopensles_helper

include $(BUILD_STATIC_LIBRARY)

include $(CLEAR_VARS)

#LOCAL_CFLAGS += -DSL_API= -DXA_API=SLAPIENTRY -DXAAPIENTRY=
#LOCAL_CFLAGS += -DUSE_PROFILES=0 -UUSE_TRACE -UUSE_DEBUG -DNDEBUG -DUSE_LOG=SLAndroidLogLevel_Info
LOCAL_CFLAGS += -DUSE_PROFILES=0 -DUSE_TRACE -DUSE_DEBUG -UNDEBUG \
# select the level of log messages
#   -DUSE_LOG=SLAndroidLogLevel_Verbose
   -DUSE_LOG=SLAndroidLogLevel_Info
# trace all the OpenSL ES method enter/exit in the logs
#LOCAL_CFLAGS += -DSL_TRACE_DEFAULT=SL_TRACE_ALL

# Reduce size of .so and hide internal global symbols
LOCAL_CFLAGS += -fvisibility=hidden -DSL_API='__attribute__((visibility("default")))' \
        -DXA_API='__attribute__((visibility("default")))'

LOCAL_SRC_FILES:=                     \
        OpenSLES_IID.c                \
        classes.c                     \
        data.c                        \
        devices.c                     \
        entry.c                       \
        trace.c                       \
        locks.c                       \
        sles.c                        \
        sllog.c                       \
        ThreadPool.c                  \
        android/AudioPlayer_to_android.cpp    \
        android/AudioRecorder_to_android.cpp  \
        android/MediaPlayer_to_android.cpp    \
        android/OutputMix_to_android.cpp      \
        android/android_AVPlayer.cpp          \
        android/android_LocAVPlayer.cpp       \
        android/android_StreamPlayer.cpp      \
        android/android_SfPlayer.cpp          \
        android/android_Effect.cpp            \
        autogen/IID_to_MPH.c                  \
        objects/C3DGroup.c                    \
        objects/CAudioPlayer.c                \
        objects/CAudioRecorder.c              \
        objects/CEngine.c                     \
        objects/COutputMix.c                  \
        objects/CMediaPlayer.c                \
        itf/IAndroidBufferQueue.c         \
        itf/IAndroidConfiguration.c       \
        itf/IAndroidEffect.cpp            \
        itf/IAndroidEffectCapabilities.c  \
        itf/IAndroidEffectSend.c          \
        itf/IBassBoost.c                  \
        itf/IBufferQueue.c                \
        itf/IDynamicInterfaceManagement.c \
        itf/IEffectSend.c                 \
        itf/IEngine.c                     \
        itf/IEngineCapabilities.c         \
        itf/IEnvironmentalReverb.c        \
        itf/IEqualizer.c                  \
        itf/IMuteSolo.c                   \
        itf/IObject.c                     \
        itf/IOutputMix.c                  \
        itf/IPlay.c                       \
        itf/IPlaybackRate.c               \
        itf/IPrefetchStatus.c             \
        itf/IPresetReverb.c               \
        itf/IRecord.c                     \
        itf/ISeek.c                       \
        itf/IStreamInformation.c          \
        itf/IVirtualizer.c                \
        itf/IVolume.c

EXCLUDE_SRC :=                            \
        sync.c                            \
        itf/I3DCommit.c                   \
        itf/I3DDoppler.c                  \
        itf/I3DGrouping.c                 \
        itf/I3DLocation.c                 \
        itf/I3DMacroscopic.c              \
        itf/I3DSource.c                   \
        itf/IAudioDecoderCapabilities.c   \
        itf/IAudioEncoder.c               \
        itf/IAudioEncoderCapabilities.c   \
        itf/IAudioIODeviceCapabilities.c  \
        itf/IDeviceVolume.c               \
        itf/IDynamicSource.c              \
        itf/ILEDArray.c                   \
        itf/IMIDIMessage.c                \
        itf/IMIDIMuteSolo.c               \
        itf/IMIDITempo.c                  \
        itf/IMIDITime.c                   \
        itf/IMetadataExtraction.c         \
        itf/IMetadataTraversal.c          \
        itf/IPitch.c                      \
        itf/IRatePitch.c                  \
        itf/IThreadSync.c                 \
        itf/IVibra.c                      \
        itf/IVisualization.c

LOCAL_C_INCLUDES:=                                                  \
        $(JNI_H_INCLUDE)                                            \
        system/media/wilhelm/include                                \
        frameworks/base/media/libstagefright                        \
        frameworks/base/media/libstagefright/include                \
        frameworks/base/include/media/stagefright/openmax

LOCAL_CFLAGS += -x c++ -Wno-multichar -Wno-invalid-offsetof

LOCAL_STATIC_LIBRARIES += \
        libopensles_helper        \
        libOpenSLESUT

LOCAL_SHARED_LIBRARIES :=         \
        libutils                  \
        libmedia                  \
        libbinder                 \
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
else
        LOCAL_CFLAGS += -DTARGET_SIMULATOR
endif

ifeq ($(TARGET_OS)-$(TARGET_SIMULATOR),linux-true)
        LOCAL_LDLIBS += -lpthread
endif

LOCAL_PRELINK_MODULE:= false

LOCAL_MODULE:= libOpenSLES

ifeq ($(TARGET_BUILD_VARIANT),userdebug)
        LOCAL_CFLAGS += -DUSERDEBUG_BUILD=1
endif

include $(BUILD_SHARED_LIBRARY)

include $(call all-makefiles-under,$(LOCAL_PATH))
