# Add include paths for native code.
FFW_PATH := $(call my-dir)

# Uncomment the requirements below, once we need them:

# STLport
ifneq ($(TARGET_SIMULATOR),true)
include external/stlport/libstlport.mk
endif

# Neven FaceDetect SDK
#LOCAL_C_INCLUDES += external/neven/FaceRecEm/common/src/b_FDSDK \
#	external/neven/FaceRecEm/common/src \
#	external/neven/Embedded/common/conf \
#	external/neven/Embedded/common/src \
#	external/neven/unix/src

# Finally, add this directory
LOCAL_C_INCLUDES += $(FFW_PATH)

