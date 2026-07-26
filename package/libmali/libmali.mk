################################################################################
#
# libmali - ARM Mali proprietary userspace (EGL/GLESv2/GBM) for RK3576 G52
#
# Forked from Buildroot's rockchip-mali (hardcoded to G31). Downloads the G52
# g24p0 plain-GBM blob from Rockchip's public libmali distribution (the
# JeffyCN/mirrors `libmali` branch, pinned by commit; byte-identical to the
# SDK's external/libmali copy) and installs it with EGL/GLESv2/GBM symlinks
# against soname libmali.so.1, headers, and pkgconfig. Pairs with the
# in-kernel Mali Bifrost kmod (/dev/mali0).
#
################################################################################

LIBMALI_VERSION = g24p0
LIBMALI_BLOB = libmali-bifrost-g52-g24p0-gbm.so
LIBMALI_MIRROR_COMMIT = 44bcc8e3ed82ee3ff10568d56c30931cda577387
LIBMALI_SITE = https://github.com/JeffyCN/mirrors/raw/$(LIBMALI_MIRROR_COMMIT)/lib/aarch64-linux-gnu
LIBMALI_SOURCE = $(LIBMALI_BLOB)
LIBMALI_LICENSE = PROPRIETARY (ARM End User License Agreement)
LIBMALI_INSTALL_STAGING = YES
LIBMALI_DEPENDENCIES = host-patchelf libdrm
LIBMALI_PROVIDES = libegl libgles libgbm

# The source is a single .so, not an archive — "extract" is a copy.
define LIBMALI_EXTRACT_CMDS
	cp $(LIBMALI_DL_DIR)/$(LIBMALI_SOURCE) $(@D)/
endef

# Symlinks: the SONAME target plus the -dev .so names the linker resolves.
LIBMALI_SYMLINKS = \
	libmali.so.1 \
	libmali.so \
	libMali.so \
	libEGL.so \
	libEGL.so.1 \
	libgbm.so \
	libgbm.so.1 \
	libGLESv2.so \
	libGLESv2.so.2 \
	libGLESv1_CM.so \
	libGLESv1_CM.so.1

# The runtime library + symlinks — installed to both target and staging.
define LIBMALI_INSTALL_LIB
	$(INSTALL) -D -m 0755 $(@D)/$(LIBMALI_BLOB) \
		$(1)/usr/lib/$(LIBMALI_BLOB)
	$(HOST_DIR)/bin/patchelf --set-soname libmali.so.1 \
		$(1)/usr/lib/$(LIBMALI_BLOB)
	$(foreach l,$(LIBMALI_SYMLINKS), \
		ln -sf $(LIBMALI_BLOB) $(1)/usr/lib/$(l)
	)
endef

# Headers + pkgconfig are compile-time only — staging, not the target rootfs.
define LIBMALI_INSTALL_DEV
	mkdir -p $(1)/usr/include $(1)/usr/lib/pkgconfig
	cp -a $(LIBMALI_PKGDIR)/include/EGL $(LIBMALI_PKGDIR)/include/GLES2 \
		$(LIBMALI_PKGDIR)/include/GLES3 $(LIBMALI_PKGDIR)/include/KHR \
		$(1)/usr/include/
	$(INSTALL) -m 0644 $(LIBMALI_PKGDIR)/include/gbm.h $(1)/usr/include/gbm.h
	$(INSTALL) -m 0644 $(LIBMALI_PKGDIR)/pkgconfig/*.pc $(1)/usr/lib/pkgconfig/
endef

define LIBMALI_INSTALL_TARGET_CMDS
	$(call LIBMALI_INSTALL_LIB,$(TARGET_DIR))
endef

define LIBMALI_INSTALL_STAGING_CMDS
	$(call LIBMALI_INSTALL_LIB,$(STAGING_DIR))
	$(call LIBMALI_INSTALL_DEV,$(STAGING_DIR))
endef

$(eval $(generic-package))
