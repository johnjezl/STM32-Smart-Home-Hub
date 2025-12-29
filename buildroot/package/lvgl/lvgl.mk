################################################################################
#
# lvgl - Light and Versatile Graphics Library
#
################################################################################

LVGL_VERSION = $(call qstrip,$(BR2_PACKAGE_LVGL_VERSION))
LVGL_SITE = $(call github,lvgl,lvgl,v$(LVGL_VERSION))
LVGL_LICENSE = MIT
LVGL_LICENSE_FILES = LICENCE.txt
LVGL_INSTALL_STAGING = YES

# Copy our lv_conf.h to the build directory
define LVGL_CONFIGURE_CMDS_PRE
	cp $(BR2_EXTERNAL_SMARTHUB_PATH)/package/lvgl/config/lv_conf.h $(@D)/
endef
LVGL_PRE_CONFIGURE_HOOKS += LVGL_CONFIGURE_CMDS_PRE

LVGL_CONF_OPTS = \
	-DLV_CONF_BUILD_DISABLE_EXAMPLES=ON \
	-DLV_CONF_BUILD_DISABLE_DEMOS=ON \
	-DLV_CONF_PATH=$(@D)/lv_conf.h \
	-DLV_USE_DRAW_SW_ASM=LV_DRAW_SW_ASM_NONE \
	-DLV_USE_NEON=OFF

# Display driver selection
ifeq ($(BR2_PACKAGE_LVGL_FBDEV),y)
LVGL_CONF_OPTS += -DLV_USE_LINUX_FBDEV=ON
endif

ifeq ($(BR2_PACKAGE_LVGL_DRM),y)
LVGL_DEPENDENCIES += libdrm
LVGL_CONF_OPTS += -DLV_USE_LINUX_DRM=ON
endif

ifeq ($(BR2_PACKAGE_LVGL_SDL),y)
LVGL_DEPENDENCIES += sdl2
LVGL_CONF_OPTS += -DLV_USE_SDL=ON
endif

# Input driver selection
ifeq ($(BR2_PACKAGE_LVGL_EVDEV),y)
LVGL_CONF_OPTS += -DLV_USE_EVDEV=ON
endif

ifeq ($(BR2_PACKAGE_LVGL_LIBINPUT),y)
LVGL_DEPENDENCIES += libinput
LVGL_CONF_OPTS += -DLV_USE_LIBINPUT=ON
endif

# Create pkg-config file for LVGL
define LVGL_INSTALL_STAGING_CMDS_POST
	$(INSTALL) -D -m 0644 $(BR2_EXTERNAL_SMARTHUB_PATH)/package/lvgl/config/lvgl.pc \
		$(STAGING_DIR)/usr/lib/pkgconfig/lvgl.pc
endef
LVGL_POST_INSTALL_STAGING_HOOKS += LVGL_INSTALL_STAGING_CMDS_POST

$(eval $(cmake-package))
