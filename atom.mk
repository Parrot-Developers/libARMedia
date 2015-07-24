LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_CATEGORY_PATH := dragon/libs
LOCAL_MODULE := libARMedia
LOCAL_DESCRIPTION := Library to manage video encapsulation

LOCAL_CFLAGS := -D_FILE_OFFSET_BITS=64
LOCAL_LIBRARIES := ARSDKBuildUtils libARSAL libARDiscovery json
LOCAL_EXPORT_LDLIBS := -larmedia

# Copy in build dir so bootstrap files are generated in build dir
LOCAL_AUTOTOOLS_COPY_TO_BUILD_DIR := 1

# Configure script is not at the root
LOCAL_AUTOTOOLS_CONFIGURE_SCRIPT := Build/configure

#Autotools variables
LOCAL_AUTOTOOLS_CONFIGURE_ARGS := \
	--with-libARSALInstallDir="" \
	--with-libARDiscoveryInstallDir="" \
	--with-jsonInstallDir=""

# User define command to be launch before configure step.
# Generates files used by configure
define LOCAL_AUTOTOOLS_CMD_POST_UNPACK
	$(Q) cd $(PRIVATE_SRC_DIR)/Build && ./bootstrap
endef

include $(BUILD_AUTOTOOLS)
