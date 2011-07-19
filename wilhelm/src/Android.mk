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

LOCAL_C_INCLUDES:= \
        system/media/wilhelm/include

LOCAL_CFLAGS += -Wno-override-init
# -Wno-missing-field-initializers
# optional, see comments in MPH_to.c: -DUSE_DESIGNATED_INITIALIZERS -S
LOCAL_CFLAGS += -DUSE_DESIGNATED_INITIALIZERS

LOCAL_SRC_FILES:=                     \
        MPH_to.c \
        handlers.c

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
LOCAL_CFLAGS += -fvisibility=hidden -DLI_API='__attribute__((visibility("default")))'

LOCAL_SRC_FILES:=                     \
        OpenSLES_IID.c                \
        classes.c                     \
        data.c                        \
        devices.c                     \
        entry.c                       \
        handler_bodies.c              \
        trace.c                       \
        locks.c                       \
        sles.c                        \
        sl_iid.c                      \
        sllog.c                       \
        ThreadPool.c                  \
        android/AudioPlayer_to_android.cpp    \
        android/AudioRecorder_to_android.cpp  \
        android/MediaPlayer_to_android.cpp    \
        android/OutputMix_to_android.cpp      \
        android/VideoCodec_to_android.cpp     \
        android/CallbackProtector.cpp         \
        android/android_AudioSfDecoder.cpp    \
        android/android_AudioToCbRenderer.cpp \
        android/android_GenericMediaPlayer.cpp\
        android/android_GenericPlayer.cpp     \
        android/android_LocAVPlayer.cpp       \
        android/android_StreamPlayer.cpp      \
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
        itf/IMetadataExtraction.c         \
        itf/IMuteSolo.c                   \
        itf/IObject.c                     \
        itf/IOutputMix.c                  \
        itf/IPlay.c                       \
        itf/IPlaybackRate.c               \
        itf/IPrefetchStatus.c             \
        itf/IPresetReverb.c               \
        itf/IRecord.c                     \
        itf/ISeek.c                       \
        itf/IStreamInformation.cpp        \
        itf/IVideoDecoderCapabilities.cpp \
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
        itf/IMetadataTraversal.c          \
        itf/IPitch.c                      \
        itf/IRatePitch.c                  \
        itf/IThreadSync.c                 \
        itf/IVibra.c                      \
        itf/IVisualization.c

LOCAL_C_INCLUDES:=                                                  \
        system/media/wilhelm/include                                \
        frameworks/base/media/libstagefright                        \
        frameworks/base/media/libstagefright/include                \
        frameworks/base/include/media/stagefright/openmax           \
        system/media/audio_effects/include

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
        libcutils                 \
        libgui                    \
        libdl



LOCAL_MODULE := libwilhelm
LOCAL_MODULE_TAGS := optional

ifeq ($(TARGET_BUILD_VARIANT),userdebug)
        LOCAL_CFLAGS += -DUSERDEBUG_BUILD=1
endif

LOCAL_PRELINK_MODULE := false
include $(BUILD_SHARED_LIBRARY)

include $(CLEAR_VARS)
LOCAL_SRC_FILES := sl_entry.c sl_iid.c
LOCAL_C_INCLUDES:=                                                  \
        system/media/wilhelm/include                                \
        frameworks/base/media/libstagefright                        \
        frameworks/base/media/libstagefright/include                \
        frameworks/base/include/media/stagefright/openmax
LOCAL_MODULE := libOpenSLES
LOCAL_PRELINK_MODULE := false
LOCAL_MODULE_TAGS := optional
LOCAL_CFLAGS += -x c++ -DLI_API= -fvisibility=hidden \
                -DSL_API='__attribute__((visibility("default")))'
LOCAL_SHARED_LIBRARIES := libwilhelm
include $(BUILD_SHARED_LIBRARY)

include $(CLEAR_VARS)
LOCAL_SRC_FILES := xa_entry.c xa_iid.c
LOCAL_C_INCLUDES:=                                                  \
        system/media/wilhelm/include                                \
        frameworks/base/media/libstagefright                        \
        frameworks/base/media/libstagefright/include                \
        frameworks/base/include/media/stagefright/openmax
LOCAL_MODULE := libOpenMAXAL
LOCAL_PRELINK_MODULE := false
LOCAL_MODULE_TAGS := optional
LOCAL_CFLAGS += -x c++ -DLI_API= -fvisibility=hidden \
                -DXA_API='__attribute__((visibility("default")))'
LOCAL_SHARED_LIBRARIES := libwilhelm
include $(BUILD_SHARED_LIBRARY)
