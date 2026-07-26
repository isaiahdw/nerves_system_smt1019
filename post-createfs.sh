#!/bin/sh

set -e

FWUP_CONFIG=$NERVES_DEFCONFIG_DIR/fwup.conf

# Run the common post-image processing for nerves
$BR2_EXTERNAL_NERVES_PATH/board/nerves-common/post-createfs.sh $TARGET_DIR $FWUP_CONFIG

# Pack the Rockchip resource image (boot splash): kernel dtb + logo BMPs.
# The vendor U-Boot reads logo.bmp (shown at ~2s) and logo_kernel.bmp
# (handed to the kernel's drm-logo path) from the GPT partition named
# "resource"; it also uses the packed dtb (as rk-kernel.dtb) for its own
# display bring-up (CONFIG_USING_KERNEL_DTB).
RESOURCE_TOOL_SRC="${BUILD_DIR}/linux-custom/scripts/resource_tool.c"
DTB="${BINARIES_DIR}/rk3576s-r157-v2.0-wf1019-linux.dtb"
LOGO_DIR="${NERVES_DEFCONFIG_DIR}/uboot/logo"
if [ -f "${RESOURCE_TOOL_SRC}" ] && [ -f "${DTB}" ]; then
    cc -O2 -o "${BUILD_DIR}/resource_tool" "${RESOURCE_TOOL_SRC}"
    (cd "${BINARIES_DIR}" && "${BUILD_DIR}/resource_tool" --pack \
        --image=resource.img \
        "${DTB}" "${LOGO_DIR}/logo.bmp" "${LOGO_DIR}/logo_kernel.bmp")
    echo "post-createfs: packed resource.img ($(wc -c < ${BINARIES_DIR}/resource.img) bytes)"
else
    echo "post-createfs: ERROR: resource_tool source or dtb missing; cannot pack resource.img"
    exit 1
fi
