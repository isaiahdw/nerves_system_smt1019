################################################################################
#
# rkaiq — Rockchip camera 3A engine (AE/AWB/ISP control) for the GC5035
# via rkisp39. Prebuilt aarch64 binaries (bin/) cross-compiled from the SDK's
# external/camera_engine_rkaiq with the Nerves toolchain — recipe and the
# mandatory UAPI ABI patch in this package's README.md. IQ tuning files ship
# to /etc/iqfiles (the server's built-in path). An application must supervise
# /usr/bin/rkaiq_3A_server (camera is unusable for imaging without it).
#
################################################################################

RKAIQ_VERSION = 6.0x30.4.6
RKAIQ_SOURCE =
RKAIQ_LICENSE = PROPRIETARY (Rockchip)

define RKAIQ_INSTALL_TARGET_CMDS
	$(INSTALL) -D -m 0755 $(RKAIQ_PKGDIR)/bin/rkaiq_3A_server \
		$(TARGET_DIR)/usr/bin/rkaiq_3A_server
	$(INSTALL) -D -m 0755 $(RKAIQ_PKGDIR)/bin/librkaiq.so \
		$(TARGET_DIR)/usr/lib/librkaiq.so
	mkdir -p $(TARGET_DIR)/etc/iqfiles/CAC_gc05a2_KYT-11210-V2_default
	$(INSTALL) -D -m 0644 $(RKAIQ_PKGDIR)/iqfiles/gc5035_default_default.json \
		$(TARGET_DIR)/etc/iqfiles/gc5035_default_default.json
	$(INSTALL) -D -m 0644 $(RKAIQ_PKGDIR)/iqfiles/CAC_gc05a2_KYT-11210-V2_default/cac2_map_hw_2592x1944.bin \
		$(TARGET_DIR)/etc/iqfiles/CAC_gc05a2_KYT-11210-V2_default/cac2_map_hw_2592x1944.bin
endef

$(eval $(generic-package))
