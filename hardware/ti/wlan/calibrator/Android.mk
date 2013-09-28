LOCAL_PATH:= $(call my-dir)

#
# Calibrator
#
include $(CLEAR_VARS)

LOCAL_SRC_FILES := \
		nvs.c \
		misc_cmds.c \
		calibrator.c \
		plt.c \
		ini.c

LOCAL_CFLAGS := -DCONFIG_LIBNL20
LOCAL_LDFLAGS := -Wl,--no-gc-sections

LOCAL_C_INCLUDES := \
	$(LOCAL_PATH) \
	external/libnl-headers

LOCAL_STATIC_LIBRARIES := libnl_2
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE := calibrator

LOCAL_SHARED_LIBRARIES:= libcutils

include $(BUILD_EXECUTABLE)
