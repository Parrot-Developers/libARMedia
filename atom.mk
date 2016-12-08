LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE := libARMedia
LOCAL_DESCRIPTION := Library to manage video encapsulation
LOCAL_CATEGORY_PATH := dragon/libs

LOCAL_MODULE_FILENAME := libarmedia.so

LOCAL_LIBRARIES := \
	libARSAL \
	libARDiscovery \
	json

LOCAL_C_INCLUDES := \
	$(LOCAL_PATH)/Includes \
	$(LOCAL_PATH)/Sources

LOCAL_CFLAGS := \
	-D_FILE_OFFSET_BITS=64 \
	-D_LARGEFILE64_SOURCE \
	-DHAVE_CONFIG_H

LOCAL_SRC_FILES := \
	gen/Sources/ARMEDIA_Error.c \
	Sources/ARMEDIA_VideoEncapsuler.c \
	Sources/ARMEDIA_VideoAtoms.c

LOCAL_INSTALL_HEADERS := \
	Includes/libARMedia/ARMEDIA_VideoAtoms.h:usr/include/libARMedia/ \
	Includes/libARMedia/ARMEDIA_Error.h:usr/include/libARMedia/ \
	Includes/libARMedia/ARMEDIA_VideoEncapsuler.h:usr/include/libARMedia/ \
	Includes/libARMedia/ARMedia.h:usr/include/libARMedia/

ifeq ("$(TARGET_OS)","darwin")
ifneq ("$(TARGET_OS_FLAVOUR)","native")
LOCAL_LIBRARIES += libARDataTransfer
LOCAL_SRC_FILES += \
	Sources/ARMEDIA_Manager.m \
	Sources/ALAssetRepresentation+VideoAtoms.m \
	Sources/ARMEDIA_Object.m
LOCAL_INSTALL_HEADERS += \
	Includes/libARMedia/ARMEDIA_Manager.h:usr/include/libARMedia/ \
	Includes/libARMedia/ARMEDIA_Object.h:usr/include/libARMedia/
endif
endif

include $(BUILD_LIBRARY)
