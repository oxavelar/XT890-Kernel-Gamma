LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

ifeq ($(BUILD_WITH_CHAABI_SUPPORT),true)
LOCAL_C_INCLUDES = \
	$(TARGET_OUT_HEADERS)/chaabi
endif

LOCAL_SRC_FILES:= \
	wlan_provisioning.c

LOCAL_CFLAGS:=

ifeq ($(BUILD_WITH_CHAABI_SUPPORT),true)
LOCAL_CFLAGS += -DBUILD_WITH_CHAABI_SUPPORT
LOCAL_STATIC_LIBRARIES := \
	CC6_UMIP_ACCESS CC6_ALL_BASIC_LIB
endif

ifeq ($(TARGET_PRODUCT),mfld_gi)
LOCAL_CFLAGS += -DSINGLE_BAND
endif

LOCAL_SHARED_LIBRARIES := \
	libc libcutils libhardware_legacy libcrypto

LOCAL_MODULE:= wlan_prov
LOCAL_MODULE_TAGS := optional

include $(BUILD_EXECUTABLE)

NVS_FILE = wl1271-nvs.bin
FAC_NVS_FILE = nvs_map_mac80211.bin
FAC_NVS_FILE_SYM = wl12xx-fac-nvs.bin

SYMLINKS := $(addprefix $(PRODUCT_OUT)/system/etc/firmware/ti-connectivity/,$(NVS_FILE))
$(SYMLINKS): $(LOCAL_INSTALLED_MODULE) $(LOCAL_PATH)/Android.mk
	@echo "Symlink: $@"
	@mkdir -p $(dir $@)
	@rm -rf $@
	$(hide) ln -sf $(addprefix /data/misc/firmware/ti-connectivity/,$(NVS_FILE)) $@

FACSYMLINKS := $(addprefix $(PRODUCT_OUT)/system/etc/firmware/ti-connectivity/,$(FAC_NVS_FILE_SYM))
$(FACSYMLINKS) : $(LOCAL_INSTALLED_MODULE) $(LOCAL_PATH)/Android.mk
	@echo "Symlink: $@"
	@mkdir -p $(dir $@)
	@rm -rf $@
	$(hide) ln -sf $(addprefix /pds/wifi/,$(FAC_NVS_FILE)) $@

ALL_DEFAULT_INSTALLED_MODULES += $(SYMLINKS) $(FACSYMLINKS)
ALL_MODULES.$(LOCAL_MODULE).INSTALLED := \
	$(ALL_MODULES.$(LOCAL_MODULE).INSTALLED) $(SYMLINKS) $(FACSYMLINKS)

