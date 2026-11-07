################################################################################
#
# smarthub-m4 - the Cortex-M4 firmware, built from this repository
#
################################################################################

SMARTHUB_M4_VERSION = local
SMARTHUB_M4_SITE = $(BR2_EXTERNAL_SMARTHUB_PATH)/..
SMARTHUB_M4_SITE_METHOD = local
SMARTHUB_M4_OVERRIDE_SRCDIR_RSYNC_EXCLUSIONS = \
	--exclude='build*/' --exclude='node_modules/' --exclude='*.pdf'
SMARTHUB_M4_DEPENDENCIES = host-arm-gnu-toolchain host-cmake

# Not a cmake-package, which would use the toolchain for the A7. This one has its own.
define SMARTHUB_M4_BUILD_CMDS
	PATH=$(HOST_DIR)/bin:$$PATH $(BR2_CMAKE) -S $(@D)/m4-firmware -B $(@D)/m4-firmware/buildroot-build \
		-DCMAKE_BUILD_TYPE=Release
	PATH=$(HOST_DIR)/bin:$$PATH $(BR2_CMAKE) --build $(@D)/m4-firmware/buildroot-build --parallel $(PARALLEL_JOBS)
endef

define SMARTHUB_M4_INSTALL_TARGET_CMDS
	$(INSTALL) -D -m 0644 $(@D)/m4-firmware/buildroot-build/smarthub-m4.elf \
		$(TARGET_DIR)/lib/firmware/smarthub-m4.elf
endef

$(eval $(generic-package))
