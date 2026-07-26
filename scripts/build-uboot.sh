#!/bin/sh
#
# Reproducibly build the committed U-Boot blobs from the Rockchip SDK's
# vendor U-Boot, in a Docker container (the rkbin packing tools are x86-64
# Linux binaries, hence --platform linux/amd64).
#
# Produces, in uboot/:
#   idbloader.img     Rockchip idblock (prebuilt SPL + DDR init from rkbin)
#   u-boot.itb        phase-1 U-Boot (no env; extlinux boot only)
#   u-boot-env.itb    phase-2 U-Boot (CONFIG_ENV_IS_IN_MMC @ 0xF00000;
#                     A/B boot with automatic revert — the fwup default)
#
# Usage: scripts/build-uboot.sh /path/to/rk3576-linux6.1-20251118
#
set -e

SDK="$1"
REPO_DIR="$(cd "$(dirname "$0")/.." && pwd)"

if [ -z "$SDK" ] || [ ! -d "$SDK/u-boot" ]; then
    echo "Usage: $0 /path/to/rk3576-linux-sdk  (expects a 'u-boot' dir inside)" >&2
    exit 1
fi

build() {
    # $1 = variant tag, $2 = extra defconfig lines (may be empty)
    variant="$1"
    extra="$2"
    echo "=== building U-Boot variant: $variant ==="
    docker run --rm --platform linux/amd64 -v "$SDK":/sdk -w /sdk/u-boot \
        debian:bullseye bash -c "
set -e
apt-get update -qq
DEBIAN_FRONTEND=noninteractive apt-get install -y -qq make gcc \
    gcc-aarch64-linux-gnu bison flex libssl-dev bc python2 python3 \
    device-tree-compiler file xxd >/dev/null
git checkout configs/rk3576_defconfig 2>/dev/null || true
if [ -n '$extra' ]; then printf '%s\n' '$extra' >> configs/rk3576_defconfig; fi
./make.sh rk3576 CROSS_COMPILE=aarch64-linux-gnu-
"
}

# Phase 1: stock vendor config (no persistent env)
build phase1 ""
cp "$SDK"/u-boot/rk3576_idblock_*.img "$REPO_DIR/uboot/idbloader.img"
cp "$SDK"/u-boot/uboot.img            "$REPO_DIR/uboot/u-boot.itb"

# Phase 2: env in MMC at 0xF00000 (shared with fwup/nerves_runtime)
build phase2 "CONFIG_ENV_IS_IN_MMC=y
CONFIG_ENV_OFFSET=0xF00000
CONFIG_ENV_SIZE=0x20000
CONFIG_SYS_MMC_ENV_DEV=0"
cp "$SDK"/u-boot/uboot.img "$REPO_DIR/uboot/u-boot-env.itb"

echo "=== done. Updated uboot/idbloader.img, u-boot.itb, u-boot-env.itb"
echo "The maskrom download loader is at $SDK/u-boot/rk3576_spl_loader_*.bin"
