LOCAL_PATH := $(call my-dir)

# JNI Wrapper
include $(CLEAR_VARS)

LOCAL_CFLAGS := -g
ifeq ($(TARGET_ARCH_ABI), armeabi-v7a)
    LOCAL_CFLAGS += -mfloat-abi=softfp -mfpu=neon
endif
LOCAL_MODULE := libarmedia_android
LOCAL_SRC_FILES := JNI/c/ARMEDIA_JNI_VideoAtoms.c
LOCAL_LDLIBS := -llog -lz
LOCAL_SHARED_LIBRARIES := libARMedia-prebuilt
include $(BUILD_SHARED_LIBRARY)
