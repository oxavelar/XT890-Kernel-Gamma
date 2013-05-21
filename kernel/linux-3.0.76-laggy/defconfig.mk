DEFCONFIGSRC			:= $(KERNEL_SRC)/arch/x86/configs
LJAPDEFCONFIGSRC		:= ${DEFCONFIGSRC}/ext_config
PRODUCT_SPECIFIC_DEFCONFIGS	:= $(DEFCONFIGSRC)/$(KERNEL_DEFCONFIG)
TARGET_DEFCONFIG		:= $(KERNEL_OUT)/mapphone_defconfig
TARGET_DROIDBOOT_DEFCONFIG	:= $(KERNEL_DROIDBOOT_OUT)/droidboot_defconfig

# product-specific overrides (deprecated)
ifneq ($(wildcard ${LJAPDEFCONFIGSRC}/product/${TARGET_DEVICE}.config),)
PRODUCT_SPECIFIC_DEFCONFIGS += ${LJAPDEFCONFIGSRC}/product/${TARGET_DEVICE}.config
endif

# build eng kernel for eng and userdebug Android variants
ifneq ($(TARGET_BUILD_VARIANT), user)
PRODUCT_SPECIFIC_DEFCONFIGS += ${LJAPDEFCONFIGSRC}/eng_bld.config
endif

# select graphics driver configuration options based on android build variant
SGX_VARIANT_DEFCONFIG := ${LJAPDEFCONFIGSRC}/sgx540/$(or $(SGX_VARIANT),$(TARGET_BUILD_VARIANT)).config
ifneq ($(wildcard $(SGX_VARIANT_DEFCONFIG)),)
PRODUCT_SPECIFIC_DEFCONFIGS += $(SGX_VARIANT_DEFCONFIG)
endif

define do-make-defconfig
	$(hide) mkdir -p $(dir $(1))
	( perl -le 'print "# This file was automatically generated from:\n#\t" . join("\n#\t", @ARGV) . "\n"' $(2) && cat $(2) ) > $(1) || ( rm -f $(1) && false )
endef

DROIDBOOT_DEFCONFIGS = $(PRODUCT_SPECIFIC_DEFCONFIGS) $(LJAPDEFCONFIGSRC)/droidboot.config

#
# make combined defconfig file
#---------------------------------------
$(TARGET_DEFCONFIG): FORCE $(PRODUCT_SPECIFIC_DEFCONFIGS)
	$(call do-make-defconfig,$@,$(PRODUCT_SPECIFIC_DEFCONFIGS))

$(TARGET_DROIDBOOT_DEFCONFIG): FORCE $(DROIDBOOT_DEFCONFIGS)
	$(call do-make-defconfig,$@,$(DROIDBOOT_DEFCONFIGS))

.PHONY: FORCE
FORCE:
