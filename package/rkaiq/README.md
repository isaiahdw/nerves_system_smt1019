# rkaiq — camera 3A engine

Rockchip `camera_engine_rkaiq` (AIQ v6.0x30.4, SDK 20251118) provides
auto-exposure/AWB/ISP control for the GC5035 via rkisp39. Ships as a
Buildroot package: prebuilt aarch64 binaries from `bin/` (built with the
recipe below) install to /usr/bin + /usr/lib, IQ files to /etc/iqfiles
(the server's built-in path). An application must supervise rkaiq_3A_server — the camera is
unusable for imaging without it.

## Build recipe (validated 2026-07-25, cross-compiled on the Mac)

Source: SDK tarball `external/camera_engine_rkaiq` (597 MB, extracted at
`~/projects/rk3576-sdk/rk3576-linux6.1-20251118/external/`).

```sh
# 1. apply patches/0001 (UAPI ABI fix — MANDATORY, see below)
# 2. configure with the Nerves toolchain (outer CMakeLists so
#    rkaiq_3A_server builds too); env needed on EVERY cmake/ninja run:
export AIQ_BUILD_HOST_DIR=$HOME/.nerves/artifacts/nerves_toolchain_aarch64_nerves_linux_gnu-darwin_arm-13.2.0
export AIQ_BUILD_TOOLCHAIN_TRIPLE=aarch64-nerves-linux-gnu
export AIQ_BUILD_SYSROOT=sysroot
export AIQ_BUILD_ARCH=aarch64
cmake -G Ninja -DCMAKE_BUILD_TYPE=MinSizeRel -DRKAIQ_TARGET_SOC=rk3576 \
  -DARCH=aarch64 -DCMAKE_TOOLCHAIN_FILE=<src>/rkaiq/cmake/toolchains/gcc.cmake \
  -DRKAIQ_BUILD_BINARY_IQ=OFF -DISP_HW_VERSION=-DISP_HW_V39 \
  -DCMAKE_POLICY_VERSION_MINIMUM=3.5 <src>
ninja rkaiq_3A_server   # produces rkaiq_3A_server + rkaiq/all_lib/*/librkaiq.so
```

`rkaiq_3A_server` hardcodes `#define IQ_PATH "/etc/iqfiles/"` — correct
for the eventual squashfs package; the staging build sed-patches it to
`/root/rkaiq/iqfiles/`.

## The three root causes (each masked the next)

1. **vblank < 1000 µs stalls CIF-online**: gc5035 vts_def 2000 leaves 56
   blank lines = 933 µs; RK3576 CIF→ISP online mode needs ≥1000 µs
   (kernel warns, then drops/stalls). Staging fix: set
   `vertical_blanking≥120` AFTER `--set-fmt-video` (the driver resets it
   on set_fmt). Once 3A runs, AIQ manages vblank compliantly itself.
2. **bayertnr (bay3d) buffer init fails (-1)** → aborts the per-frame
   params loop. Disabled in the IQ json (`bayertnr.en=0`), which skips
   the init (gated on ISP3X_MODULE_BAY3D). Suspected cause: 16 MB CMA
   too small for the ~10 MB contiguous IIR buffers. Re-enable after a
   CMA bump.
3. **4-byte UAPI mismatch killed 3A entirely** — see
   `patches/0001-isp39-uapi-match-rkr5.1-kernel.patch`. Engine and
   kernel rkisp UAPI must match exactly; when 3A "runs but never
   iterates", diff the uapi headers first.

## iqfiles/

`gc5035_default_default.json` — hybrid IQ: the SDK's isp39
`gc05a2_KYT-11210-V2_default.json` (GalaxyCore 5 MP sibling; no isp39
gc5035 tuning exists publicly) with the REAL gc5035 data transplanted
from the isp21 `gc5035_default_PC5322-M5.json`: `sensor_calib` (gain map
1–8x reg 128–1024, time limits), all 14 CCM color matrices, and the 7
AWB per-illuminant standard gains — plus `bayertnr.en=0` (root cause 2).
A/B vs the pure gc05a2 profile measured near-identical chroma in a dim
warm-lit room (V 158 vs 161), i.e. the remaining cast is scene/low-light
dominated; verify with the white-paper test in good light.
Filename = `<sensor>_<module>_<lens>.json` per our DTS
(`camera-module-name/lens-name = "default"`). The CAC dir must ship
alongside (relative path inside the json), though `cac.en=0`.

Known gap: AWB/CCM/LSC are still gc05a2+KYT-module calibrations →
warm/red cast, worst in low light. Real fix: migrate genuine gc5035
tuning (SDK `iqfiles/isp21/gc5035_*.json` + `tools/iqUpdateRule/RK3576`
schema rules, or Rockchip tuning tool), or obtain ELC's tuned file
("GC5035 ISP39 RKAIQ JSON for RK3576, module R157/wf1019").

## Productization TODO

- Buildroot pkg: build librkaiq + rkaiq_3A_server, install to /usr, ship
  iqfiles/ to /etc/iqfiles, patch hunk 0001, start under app supervision
  (supervised by the application; `LD_LIBRARY_PATH` not needed once lib is in /usr/lib).
- Kernel follow-ups: CMA 16→64 MB (re-enable bayertnr), consider
  vts_def bump for pre-3A vblank compliance.

## Live WB control socket (server patch, v6.0x30.4.3)

`rkaiq_3A_server` carries a local addition: a unix datagram socket at
`/tmp/rkaiq_ctl.sock` accepting text commands for live white-balance
tuning from the app/UI without engine restarts:

    mwb <rgain> <bgain>   manual WB, G gains fixed at 1.0
    awb                   back to auto WB
    get                   replies "<r> <gr> <gb> <b>"

Implemented as a `control_thread` in rkaiq_3A_server.cpp calling
`rk_aiq_uapi2_setWBMode` / `setMWBGain` / `getWBGain` on every available
camera ctx. Driven by POST /api/camera/wb in the app; surfaced as
application-side WB sliders. Background: AWB auto mode never
adapts on this sensor (white points don't land in the gc05a2-calibrated
detection regions — anchors alone don't fix it), so manual WB calibrated
against the room is the working v1; proper region recalibration or an
ELC tuning file is the long-term fix.
