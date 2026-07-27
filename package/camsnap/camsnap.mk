################################################################################
#
# camsnap — camera live-view + calibration daemon. Owns the rkisp mainpath,
# streams 720p NV12, publishes /dev/shm/cam.jpg (~10 fps JPEG for an application
# live view) and /dev/shm/cam_stats.json (center-patch RGB from raw NV12, the
# white-balance calibration instrument). Built from src/ in-tree.
#
################################################################################

CAMSNAP_VERSION = 1.0
CAMSNAP_SITE = $(CAMSNAP_PKGDIR)/src
CAMSNAP_SITE_METHOD = local
CAMSNAP_LICENSE = MIT
CAMSNAP_DEPENDENCIES = jpeg

define CAMSNAP_BUILD_CMDS
	$(TARGET_CC) $(TARGET_CFLAGS) -O2 -o $(@D)/camsnap $(@D)/camsnap.c \
		$(TARGET_LDFLAGS) -ljpeg
endef

define CAMSNAP_INSTALL_TARGET_CMDS
	$(INSTALL) -D -m 0755 $(@D)/camsnap $(TARGET_DIR)/usr/bin/camsnap
endef

$(eval $(generic-package))
