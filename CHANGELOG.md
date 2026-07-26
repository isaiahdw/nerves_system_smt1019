# Changelog

## v0.1.0

Initial public release, derived from the internal SMT1019 bring-up system.

Hardware support at release:

- Boot: Rockchip vendor boot chain (idbloader + U-Boot/BL31), Nerves
  U-Boot-environment A/B slots with automatic revert, delta firmware
  updates (fwup >= 1.12.0 on device), extlinux manual-recovery path.
- Kernel: armbian/linux-rockchip `rk-6.1-rkr5.1` pinned by commit SHA,
  board delta carried as the `linux/*.patch` stack (see README).
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
