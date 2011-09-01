#Android makefile to build kernel as a part of Android Build
ifeq ($(TARGET_USE_ST_ERICSSON_KERNEL),true)

# Give other modules a nice, symbolic name to use as a dependent
# Yes, there are modules that cannot build unless the kernel has
# been built. Typical (only?) example: loadable kernel modules.
.phony: build-kernel clean-kernel

PRIVATE_KERNEL_ARGS := -C kernel ARCH=arm CROSS_COMPILE=$(CROSS_COMPILE) LOCALVERSION=+

PRIVATE_OUT := $(abspath $(PRODUCT_OUT)/system)

PATH := $(PATH):$(BOOT_PATH)/u-boot/tools:$(abspath $(UBOOT_OUTPUT)/tools)
export PATH

# For compat-wireless gits to compile with kernel
export STERICSSON_WLAN_BUILT_IN=y

# only do this if we are buidling out of tree
ifneq ($(KERNEL_OUTPUT),)
ifneq ($(KERNEL_OUTPUT), $(abspath $(TOP)/kernel))
PRIVATE_KERNEL_ARGS += O=$(KERNEL_OUTPUT)
endif
else
KERNEL_OUTPUT := $(call my-dir)
endif

# Include kernel in the Android build system
include $(CLEAR_VARS)

KERNEL_LIBPATH := $(KERNEL_OUTPUT)/arch/arm/boot
LOCAL_PATH := $(KERNEL_LIBPATH)
LOCAL_SRC_FILES := uImage
LOCAL_MODULE := $(LOCAL_SRC_FILES)
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_CLASS := EXECUTABLES
LOCAL_MODULE_PATH := $(PRODUCT_OUT)

$(KERNEL_LIBPATH)/$(LOCAL_SRC_FILES): build-kernel

include $(BUILD_PREBUILT)

include $(CLEAR_VARS)

KERNEL_LIBPATH := $(KERNEL_OUTPUT)
LOCAL_PATH := $(KERNEL_LIBPATH)
LOCAL_SRC_FILES := vmlinux
LOCAL_MODULE := $(LOCAL_SRC_FILES)
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_CLASS := EXECUTABLES
LOCAL_MODULE_PATH := $(PRODUCT_OUT)

$(KERNEL_LIBPATH)/$(LOCAL_SRC_FILES): build-kernel

include $(BUILD_PREBUILT)

# Configures, builds and installs the kernel. KERNEL_DEFCONFIG usually
# comes from the BoardConfig.mk file, but can be overridden on the
# command line or by an environment variable.
# If KERNEL_DEFCONFIG is set to 'local', configuration is skipped.
# This is useful if you want to play with your own, custom configuration.

ifeq ($(ONE_SHOT_MAKEFILE),)
build-kernel: $(UBOOT_OUTPUT)/tools/mkimage
else
build-kernel:
endif

# only do this if we are buidling out of tree
ifneq ($(KERNEL_OUTPUT),)
ifneq ($(KERNEL_OUTPUT), $(abspath $(TOP)/kernel))
	@mkdir -p $(KERNEL_OUTPUT)
endif
endif

ifeq ($(KERNEL_DEFCONFIG),local)
	@echo Skipping kernel configuration, KERNEL_DEFCONFIG set to local
else
	$(MAKE) $(PRIVATE_KERNEL_ARGS) $(KERNEL_DEFCONFIG)
endif

# Enable openMAC from here, since the defconfig is now set for UMAC
ifeq ($(WLAN_ENABLE_OPEN_MAC_SOLUTION),true)
ifeq ($(shell [ -f kernel/net/compat-wireless-openmac/Makefile ] && echo "OK"), OK)
	kernel/scripts/config --file $(KERNEL_OUTPUT)/.config \
		--enable CONFIG_MAC80211 \
		--module CONFIG_CW1200 \
		--enable CONFIG_CW1200_WAPI_SUPPORT \
		--enable CONFIG_CW1200_USE_STE_EXTENSIONS \
		--disable CONFIG_CW1200_NON_POWER_OF_TWO_BLOCKSIZES \
		--disable CONFIG_CW1200_USE_GPIO_IRQ \
		--disable CONFIG_CW1200_5GHZ_SUPPORT \
		--enable CONFIG_CW1200_STA_DEBUG \
		--enable CONFIG_CW1200_DEBUGFS \
		--disable CONFIG_CW1200_BH_DEBUG \
		--disable CONFIG_CW1200_WSM_DEBUG \
		--disable CONFIG_CW1200_WSM_DUMPS \
		--disable CONFIG_CW1200_TXRX_DEBUG \
		--disable CONFIG_CW1200_TX_POLICY_DEBUG
endif
endif

	$(MAKE) $(PRIVATE_KERNEL_ARGS) uImage
ifeq ($(KERNEL_NO_MODULES),)
	$(MAKE) $(PRIVATE_KERNEL_ARGS) modules
	$(MAKE) $(PRIVATE_KERNEL_ARGS) INSTALL_MOD_PATH:=$(PRIVATE_OUT) modules_install
else
	@echo Skipping building of kernel modules, KERNEL_NO_MODULES set
endif

# Configures and runs menuconfig on the kernel based on
# KERNEL_DEFCONFIG given on commandline or in BoardConfig.mk.
# The build after running menuconfig must be run with
# KERNEL_DEFCONFIG=local to not override the configuration modification done.

menuconfig-kernel:
# only do this if we are buidling out of tree
ifneq ($(KERNEL_OUTPUT),)
ifneq ($(KERNEL_OUTPUT), $(abspath $(TOP)/kernel))
	@mkdir -p $(KERNEL_OUTPUT)
endif
endif

	$(MAKE) $(PRIVATE_KERNEL_ARGS) $(KERNEL_DEFCONFIG)
	$(MAKE) $(PRIVATE_KERNEL_ARGS) menuconfig

clean clobber : clean-kernel

clean-kernel:
	$(MAKE) $(PRIVATE_KERNEL_ARGS) clean


endif
