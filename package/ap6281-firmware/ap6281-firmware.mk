################################################################################
#
# ap6281-firmware - WiFi/BT firmware for the Broadcom/Synaptics AP6281S
#
# Firmware, nvram, CLM and bcmdhd config.txt from the vendor's reference
# Debian image. Installed to both /lib/firmware and /vendor/etc/firmware
# (the latter is the path compiled into the in-kernel bcmdhd driver).
#
################################################################################

AP6281_FIRMWARE_VERSION = 20251118
AP6281_FIRMWARE_SOURCE =
AP6281_FIRMWARE_LICENSE = PROPRIETARY

define AP6281_FIRMWARE_INSTALL_TARGET_CMDS
	mkdir -p $(TARGET_DIR)/lib/firmware $(TARGET_DIR)/vendor/etc/firmware
	cp -f $(AP6281_FIRMWARE_PKGDIR)/firmware-broadcom/* $(TARGET_DIR)/lib/firmware/
	cp -f $(AP6281_FIRMWARE_PKGDIR)/firmware-broadcom/* $(TARGET_DIR)/vendor/etc/firmware/
endef

$(eval $(generic-package))
