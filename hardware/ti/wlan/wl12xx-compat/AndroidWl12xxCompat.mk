# This makefile is included from vendor/intel/*/AndroidBoard.mk.

.PHONY: build_wl12xx-compat

ifeq ($(TARGET_DEVICE),smi)
BUILD_BOARD = smi
else ifeq ($(TARGET_DEVICE),henry)
BUILD_BOARD = henry
else
BUILD_BOARD = $(CUSTOM_BOARD)
endif

build_wl12xx-compat: build_kernel
	TARGET_TOOLS_PREFIX="$(ANDROID_BUILD_TOP)/$(TARGET_TOOLS_PREFIX)" vendor/intel/support/wl12xx-compat-build.sh -c $(BUILD_BOARD)

