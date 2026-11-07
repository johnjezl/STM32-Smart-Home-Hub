################################################################################
#
# smarthub-app - the SmartHub application, built from this repository
#
################################################################################

SMARTHUB_APP_VERSION = local
SMARTHUB_APP_SITE = $(BR2_EXTERNAL_SMARTHUB_PATH)/..
SMARTHUB_APP_SITE_METHOD = local
SMARTHUB_APP_SUBDIR = app
# It also needs shared/, so it's the whole repository, without what a developer's built in it
SMARTHUB_APP_OVERRIDE_SRCDIR_RSYNC_EXCLUSIONS = \
	--exclude='build*/' --exclude='node_modules/' --exclude='*.pdf'
SMARTHUB_APP_DEPENDENCIES = libdrm lvgl json-for-modern-cpp mosquitto openssl sqlite yaml-cpp
SMARTHUB_APP_CONF_OPTS = -DSMARTHUB_BUILD_TESTS=OFF

# Its own install rule is for developers. This is where the image has things.
define SMARTHUB_APP_INSTALL_TARGET_CMDS
	$(INSTALL) -D -m 0755 $(@D)/app/smarthub $(TARGET_DIR)/usr/bin/smarthub
	$(INSTALL) -D -m 0644 $(@D)/app/assets/config.yaml $(TARGET_DIR)/etc/smarthub/config.yaml
	$(INSTALL) -d -m 0755 $(TARGET_DIR)/opt/smarthub/www
	$(INSTALL) -m 0644 $(@D)/web/*.html $(@D)/web/*.js $(@D)/web/*.css $(TARGET_DIR)/opt/smarthub/www/
endef

$(eval $(cmake-package))
