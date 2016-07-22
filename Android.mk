LOCAL_PATH := $(call my-dir)

# JNI Wrapper
include $(CLEAR_VARS)

LOCAL_CFLAGS := -g
LOCAL_MODULE := libarmedia_android
LOCAL_SRC_FILES := JNI/c/ARMEDIA_JNI_VideoAtoms.c
LOCAL_LDLIBS := -llog -lz
LOCAL_SHARED_LIBRARIES := libARMedia

include $(BUILD_SHARED_LIBRARY)
