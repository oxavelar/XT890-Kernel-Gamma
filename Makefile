# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
# Hacked up a simple Makefile for the XT890 Kernel, this has a lot of optimized
# flags and went a little extreme on specifying the flags since this is for a 
# remote compile target.
#

################################################################################
####################### GLOBAL SETUP AND FILE STRUCTURES #######################
################################################################################

KVERSION = linux-3.0
ARCH = i386
KDEFCONFIG = i386_mfld_oxavelar_defconfig
PLATFORM = $(PWD)
KSRC_PATH = $(PWD)/kernel/$(KVERSION)
OUT_PATH = $(PLATFORM)/out
KBUILD_OUT_PATH = $(OUT_PATH)/kbuild
CROSS_COMPILE = $(PLATFORM)/gcc/i686-linux-android-4.7/bin/i686-linux-android-
NUMJOBS = `grep -c cores /proc/cpuinfo)`

KBUILD_VERBOSE = 0

################################################################################
###################### KERNEL OPTIMIZATION FLAGS FOR THE ATOM ##################
################################################################################

ANDROID_TOOLCHAIN_FLAGS = -mno-android -O3 \
                 -mx32 \
                 -march=atom \
                 -msse \
                 -msse3 \
                 -mssse3 \
                 -pipe \
                 -mpclmul \
                 -mcx16 \
                 -msahf \
                 -mmovbe \
                 -ftree-vectorize \
                 -finline-functions \
                 -ffast-math \
                 -fexcess-precision=fast \
                 -fomit-frame-pointer \
                 -floop-parallelize-all \
                 -ftree-parallelize-loops=2 \
                 --param l1-cache-line-size=64 \
                 --param l1-cache-size=24 \
                 --param l2-cache-size=512

# The following modules have problems with -ftree-vectorize
# and if removed will get battery reading errors
export CFLAGS_platform_max17042.o       = -fno-tree-vectorize
export CFLAGS_max17042_battery.o        = -fno-tree-vectorize
export CFLAGS_intel_mdf_battery.o       = -fno-tree-vectorize


################################################################################
############################# KERNEL BUILD STEPS ###############################
################################################################################

# Now we go and export the things we care for the kernel build
export ARCH CROSS_COMPILE ANDROID_TOOLCHAIN_FLAGS KBUILD_VERBOSE


BOOT_CMDLINE="init=/init pci=noearly console=logk0 vmalloc=300M \
earlyprintk=nologger hsu_dma=7 kmemleak=off androidboot.bootmedia=sdcard \
androidboot.hardware=sc1 emmc_ipanic.ipanic_part_number=6 loglevel=4"

.PHONY: bootimage
bootimage: kernel
	rm -fR /tmp/smi-ramdisk
	cp -R $(PWD)/root /tmp/smi-ramdisk
	find $(KBUILD_OUT_PATH) -iname "*.ko" -exec cp \
	 \{\} /tmp/smi-ramdisk/lib/modules/ \;
	# Workarounds, avoiding recompile of certain module for now...
	cp -f $(PWD)/root/lib/modules/compat.ko      /tmp/smi-ramdisk/lib/modules/
	cp -f $(PWD)/root/lib/modules/cfg80211.ko    /tmp/smi-ramdisk/lib/modules/
	cp -f $(PWD)/root/lib/modules/mac80211.ko    /tmp/smi-ramdisk/lib/modules/
	cp -f $(PWD)/root/lib/modules/wl12xx.ko      /tmp/smi-ramdisk/lib/modules/
	cp -f $(PWD)/root/lib/modules/wl12xx_sdio.ko /tmp/smi-ramdisk/lib/modules/
	# Done with driver workarounds...
	$(PWD)/tools/pack-ramdisk /tmp/smi-ramdisk
	mv /tmp/ramdisk.cpio.gz $(OUT_PATH)/ramdisk.cpio.gz
	cp $(KBUILD_OUT_PATH)/arch/x86/boot/bzImage $(OUT_PATH)/kernel
	# Pack the boot.img
	$(PWD)/tools/mkbootimg --kernel $(OUT_PATH)/kernel \
	 --ramdisk $(OUT_PATH)/ramdisk.cpio.gz                \
	 --cmdline $(BOOT_CMDLINE) --output $(PLATFORM)/out/

.PHONY: kernel
kernel:
	mkdir -p $(KBUILD_OUT_PATH)
	# I edited MAGIC_STRING to load Motorola's precompiled modules without issue
	##define VERMAGIC_STRING \
	#        "3.0.34-gc6f8fd7 SMP preempt mod_unload ATOM "
	make -j$(NUMJOBS) -C $(KSRC_PATH) O=$(KBUILD_OUT_PATH) $(KDEFCONFIG)
	make -j$(NUMJOBS) -C $(KSRC_PATH) O=$(KBUILD_OUT_PATH) bzImage

.PHONY: clean
clean:
	make -j$(NUMJOBS) -C $(KSRC_PATH) mrproper
	rm -rf $(PLATFORM)/out

