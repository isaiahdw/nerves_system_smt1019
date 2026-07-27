# Changelog

## v0.2.0

- Camera support, validated on hardware: GC5035 (5 MP MIPI-CSI) via the
  rkcif/rkisp pipeline (kernel patches `0017`/`0018`,
  `CONFIG_VIDEO_GC5035`, CMA raised to 64 MB for the ISP's temporal-NR
  buffers), the rkaiq 3A engine as a prebuilt package with tuned GC5035
  IQ (`package/rkaiq` — build recipe and quick-test steps in its
  README), the `camsnap` live-view/calibration daemon, and libv4l +
  v4l-utils for pipeline debugging. An application must supervise
  `rkaiq_3A_server` for usable imaging.

## v0.1.0

Initial public release, derived from the internal SMT1019 bring-up system.

Hardware support at release:

- Boot: Rockchip vendor boot chain (idbloader + U-Boot/BL31), Nerves
  U-Boot-environment A/B slots with automatic revert, delta firmware
  updates (fwup >= 1.12.0 on device), extlinux manual-recovery path.
  Kernel and device tree are both per-slot (`Image.<slot>` +
  `rk3576-smt1019.<slot>.dtb`) so a reverted upgrade never mixes
  versions. The maskrom download loader is committed
  (`uboot/rk3576_spl_loader_v1.09.108.bin`) with the recovery
  procedure documented.
- Boot splash: vendor U-Boot logo path via the Rockchip `resource`
  partition (resource.img = kernel dtb + splash BMPs; `stdout=
  serial,vidconsole` in the env is load-bearing for the display probe);
  the kernel holds the splash until a DRM client takes over. Artwork
  generator in `uboot/logo/gen_splash.py`.
- Kernel: armbian/linux-rockchip `rk-6.1-rkr5.1` pinned by commit SHA,
  board delta carried as the `linux/*.patch` stack (see README),
  including boot-log cleanups (Mali IRQ names, quiet fiq-debugger
  probe, drm no-splash warn demotion). WL_ROCKCHIP/cfg80211/mac80211
  build as modules so regulatory.db loads from the rootfs.
- NPU: RKNPU driver validated with librknnrt 2.3.2 (3 TOPS INT8 rated
  on the RK3576S; runtime userspace not shipped — see README).
- Display + touch: 800×1280 MIPI-DSI panel on VOP2/DSI2, mainline Goodix
  GT9271 touch (udev retag + rotation calibration for libinput consumers),
  Mali G52 via vendor kmod + libmali (EGL/GLES/GBM), kmscube smoke test.
  No UI toolkit ships in the system — the application brings its own
  DRM/KMS stack.
- WiFi: AP6281S (SYN4381) via in-tree bcmdhd with firmware/NVRAM/CLM in
  `package/ap6281-firmware`.
- Ethernet: RTL8111H with stable vendor-storage MAC; PoE powered.
- Audio: ES8323-family codec playback with a LADSPA voicing/protection
  chain for the 2 W speakers, ES7202/PDM 4-mic capture.
- RGB light ring on mainline leds-pwm; GXHT30, STK3311, KXTJ3, HYM8563
  sensors/RTC; watchdog-backed heart; USB gadget networking; both USB
  hosts enabled.
