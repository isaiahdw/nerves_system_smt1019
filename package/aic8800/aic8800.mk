################################################################################
#
# aic8800 - AICSemi AIC8800D80 SDIO WiFi kernel modules + firmware
#
# Driver source is the Rockchip SDK's external/rkwifibt/drivers/aic8800_sdio
# (rk3576-linux6.1-20251118, driver release 2025_0410_b99ca8b6), built here
# against the system kernel with Buildroot's kernel-module infrastructure.
# The Bluetooth module (aic8800_btlpm) is not built.
#
# Firmware in firmware/ is from external/rkwifibt/firmware/aic/sdio. The
# driver is compiled with CONFIG_USE_FW_REQUEST=n and loads firmware by
# direct path from CONFIG_AIC_FW_PATH (/lib/firmware), using bare file
# names, so the blobs are installed flat.
#
################################################################################

AIC8800_VERSION = 20251118
AIC8800_SITE = $(AIC8800_PKGDIR)/src
AIC8800_SITE_METHOD = local
AIC8800_LICENSE = GPL-2.0 (kernel modules), proprietary (firmware)

AIC8800_MODULE_MAKE_OPTS = CONFIG_AIC8800_BTLPM_SUPPORT=n

define AIC8800_INSTALL_TARGET_CMDS
	mkdir -p $(TARGET_DIR)/lib/firmware
	cp -f $(AIC8800_PKGDIR)/firmware/*/* $(TARGET_DIR)/lib/firmware/
endef

$(eval $(kernel-module))
$(eval $(generic-package))
