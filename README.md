# Nerves System for the SMT1019 (Rockchip RK3576)

A custom [Nerves](https://nerves-project.org) system for the SMT1019
smart-home panel (Rockchip RK3576S, board `r157-v2.0-wf1019`): 800×1280
MIPI-DSI touch panel, WiFi/BT, speaker + PDM mics, RGB light ring, and an
assortment of I²C sensors. The IEx console is on `ttyFIQ0` (1.5 Mbaud
debug UART) — see `rootfs_overlay/etc/erlinit.config`.

## Hardware status

Working:

- Display: 800×1280 MIPI-DSI panel, GPU-accelerated (Mali G52 via
  libmali, DRM/KMS). No UI toolkit ships in the system — bring your own;
  `kmscube` verifies the graphics path.
- Touch: Goodix GT9271 (mainline driver, udev retag + rotation
  calibration for libinput consumers)
- WiFi: AP6281S (WiFi 6E)
- Ethernet: gigabit, stable MAC from vendor storage
- Power: PoE, powers the device standalone (no DC input)
- Audio out: stereo speakers through a LADSPA voicing/protection chain
- Audio in: 4-mic PDM array
- RGB light ring (`/sys/class/leds/{red,green,blue}`)
- Sensors: GXHT30 temp/humidity, STK3311 ALS/proximity, KXTJ3
  accelerometer (userspace via Circuits.I2C)
- HYM8563 RTC
- Boot splash: vendor U-Boot draws `logo.bmp` from the resource
  partition at ~2 s and the kernel holds it until a DRM client takes
  over (artwork + generator in `uboot/logo/`)
- Camera: GC5035 (5 MP, MIPI-CSI) through the rkcif/rkisp pipeline,
  with the rkaiq 3A engine (prebuilt, tuned GC5035 IQ in
  `/etc/iqfiles`) and the `camsnap` live-view/calibration daemon. An
  application must run `rkaiq_3A_server` for usable imaging — quick-test
  steps and details in `package/rkaiq/README.md`
- NPU: validated end to end with librknnrt (2.3.2, matmul API; 558 GOPS
  dense int8 measured). The RK3576S is rated 3 TOPS INT8
  (sparsity-assisted, per its [datasheet](docs/datasheets/rk3576s-soc.pdf))
  vs the full RK3576's 6 TOPS:
  the S bin runs at 500 MHz (`opp-s-500000000` is its only OPP-table
  entry) instead of 950 MHz. The librknnrt userspace is not shipped in
  the image.
- Watchdog-backed heart, OTA A/B upgrades with automatic revert, delta
  updates, USB gadget networking, both USB hosts

Not working / not enabled:

- Bluetooth: bring-up pending (UART HCI; `BCM4381A1.hcd` extracted)
- NFC (NXP PN5xx family, i2c7@0x28): does not ACK on the bus (likely
  needs VEN power-up); needs the NXP userspace stack
- Thread/802.15.4: the radio module is border-router capable but shares
  its single UART with Bluetooth; not brought up

## Building

Linux (or the Nerves Docker build environment) is required:

```sh
mix deps.get
mix compile
```

The result is a Nerves system artifact consumed by an application project.

### Using in an application

Add the system to your app's `mix.exs` as an `:smt1019` target:

```elixir
@all_targets [:smt1019]

# in deps():
{:nerves_system_smt1019,
 github: "isaiahdw/nerves_system_smt1019",
 tag: "v0.2.0",
 runtime: false,
 targets: :smt1019}
```

Then set `MIX_TARGET=smt1019` for every mix command:

```sh
export MIX_TARGET=smt1019
mix deps.get
mix firmware
```

## Flashing and upgrades

**Initial (factory) flash** over USB maskrom:

```sh
fwup -a -d disk.img -t complete -i <firmware>.fw     # generate a raw image
rkdeveloptool db uboot/rk3576_spl_loader_v1.09.108.bin  # MASKROM MODE ONLY — skip in loader mode
rkdeveloptool wl 0 disk.img
rkdeveloptool rd                                     # reboot into the firmware
```

Host tools: `brew install fwup rkdeveloptool` (or distro equivalents).
Entering loader/maskrom mode is described in
[uboot/README.md](uboot/README.md). In **loader mode** (the pin-hole
button) the vendor loader is already running: skip `db` (it fails with
"The device does not support this operation!") and go straight to `wl`.
`db` bootstraps the committed loader blob only from bare **maskrom mode**.

> **Caution:** a linear full-disk write covers the Rockchip vendor-storage
> region, erasing any persisted or factory-provisioned MAC addresses (they
> are regenerated on the next boot). A production factory flash must skip
> or re-provision vendor storage (`VENDOR_LAN_MAC_ID`).

Two known consequences of flashing a fixed-size image file: the kernel
warns `GPT: Alternate GPT header not at the end of the disk` (cosmetic —
the image's backup GPT sits at the image size, not the eMMC size), and
the application partition stays at its image size (512 MB) rather than
expanding to fill the eMMC.

To get the fully expanded layout, flash a **second time from the running
device**, where fwup can see the real eMMC size (`expand = true` then
fills the disk, the backup GPT lands at the true end of the disk, and —
unlike a linear image write — vendor storage is untouched):

```sh
scp <firmware>.fw nerves-XXXX.local:/root/
ssh nerves-XXXX.local  # then in IEx:
#   cmd("fwup -a -d /dev/mmcblk0 -t complete -i /root/<firmware>.fw")
#   File.write!("/proc/sysrq-trigger", "b")
```

Reboot with the SysRq trigger (or a power cycle) — the complete task
rewrites the disk under the running system, after which nothing that
needs a disk read works: a graceful `Nerves.Runtime.reboot/0` hangs
paging in shutdown code, and even spawning `reboot -f` fails with I/O
errors because the binary can no longer be loaded. Writing `b` to
`/proc/sysrq-trigger` reboots from inside the already-running kernel
with no disk access. The flash itself is complete and safe the moment
fwup prints Success. Expect the application partition to be freshly
re-initialized.

**OTA upgrades** use the standard Nerves flow (`mix upload`, or `fwup` over
SSH). Upgrades write only the inactive slot; the new firmware boots
unvalidated and U-Boot automatically reverts to the previous slot unless
the application validates it (`Nerves.Runtime.validate_firmware/0`).

## Boot flow

```
RK3576 boot ROM
  └─ idbloader.img          raw @ sector 64      (TPL/DDR init + SPL)
      └─ u-boot-env.itb     raw @ sector 16384   (U-Boot + BL31)
          └─ bootcmd = run nerves_init nerves_boot
              └─ Image.<slot> + rk3576-smt1019.<slot>.dtb from p1 (FAT)
                  └─ squashfs rootfs on p2 (A) or p3 (B)
```

U-Boot reads the Nerves environment block (slot selection, validation flag,
firmware metadata) and boots the active slot directly; unvalidated firmware
reverts automatically. The kernel **and** its device tree are per-slot
(`Image.a` + `rk3576-smt1019.a.dtb`, etc.), so a reverted upgrade never
runs the old kernel against a new dtb. One exception: the `resource`
partition (boot splash + U-Boot's display dtb) is shared between slots — a
revert keeps the new resource content, which affects only U-Boot-time
display/splash, not the booted kernel. An `extlinux/extlinux.conf` is kept
on the FAT partition purely as a manual-recovery boot path. U-Boot build
details and provenance: `uboot/README.md`.

### Disk layout

| Region            | Offset (512 B sectors) | Size    | Contents                          |
| ----------------- | ---------------------- | ------- | --------------------------------- |
| GPT               | 0                      | 32 KB   |                                   |
| idbloader.img     | 64                     | < 4 MB  | TPL + SPL                         |
| (vendor env)      | ~8192 (4 MB)           | 32 KB   | Vendor U-Boot `saveenv` area      |
| u-boot-env.itb    | 16384 (8 MB)           | < 7 MB  | U-Boot + BL31                     |
| Nerves U-Boot env | 30720 (15 MB)          | 128 KB  | Firmware metadata (fw_env.config) |
| p1 `bootfs` (FAT32) | 32768 (16 MB)        | 256 MB  | Image.a/b, per-slot dtbs, extlinux |
| p2 `rootfs_a`     |                        | 512 MB  | squashfs                          |
| p3 `rootfs_b`     |                        | 512 MB  | squashfs                          |
| p4 `resource`     |                        | 16 MB   | resource.img: dtb + boot-splash BMPs for the vendor U-Boot |
| p5 `app` (f2fs)   |                        | expands | Application data (`/root`)        |

## Kernel

The kernel source is [armbian/linux-rockchip](https://github.com/armbian/linux-rockchip)
branch `rk-6.1-rkr5.1` (Rockchip BSP 6.1 + upstream-stable merges + Armbian
fixes), pinned by commit SHA in `nerves_defconfig` and fetched by Buildroot
as a GitHub archive. To update the kernel, bump the SHA and re-verify that
the patch stack applies.

The board support is carried as `linux/*.patch`:

| Patch | Change |
| --- | --- |
| `0000` | Restore the USB1 nodes (`u2phy1`/`usb_drd1`/`combphy1`) that upstream `rk3576s.dtsi` deletes |
| `0001` | Add the SMT1019 device-tree family (board dts, two `r157` dtsi, MIPI panel dtsi) |
| `0002` | Drop the hardcoded root device from the chosen bootargs |
| `0003` | Cap the SDIO bus at 50 MHz |
| `0004` | Enable the watchdog |
| `0005` | RGB light ring on PWM LEDs (`leds-pwm`) |
| `0006` | Board-dts cleanup: correct `wifi_chip_type`, drop the camera dtsi (keeping i2c4 for the ALS), keep the drm-logo splash handover, set panel `bpc` |
| `0007` | eth0 MAC from Rockchip vendor storage, generated and persisted on first boot |
| `0008` | Disable the EVB IR-remote decoder |
| `0009` | Un-alias bcmdhd's MSG log bits from its ERROR bits so `*_msg_level=1` gives errors-only logging |
| `0011` | Keep the WiFi chip powered after module load (firmware downloads once) |
| `0012` | Point the touch node at the mainline Goodix driver (`goodix,gt9271`, edge-falling IRQ) |
| `0013` | Enable `usb_drd1` as a USB2 host |
| `0014` | Demote the drm driver's no-splash "failed to parse loader memory" warn to debug |
| `0015` | Rename the Mali GPU interrupts to the uppercase names kbase requests (removes probe errors) |
| `0016` | Quiet the fiq-debugger's expected probe errors in irq mode |
| `0017` | Enable the GC5035 camera pipeline (board dts, trimmed to the fitted sensor) |
| `0018` | rkisp v39: guard the LSC config against missing LUT buffers (Oops during 3A restart) |

The configuration is the in-tree `rockchip_linux_defconfig` plus two
fragments: `linux/rk3576.config` (Mali Bifrost) and `linux/nerves.config`
(Nerves requirements and board-specific settings, documented inline). The
device tree is `rk3576s-r157-v2.0-wf1019-linux`. RK3576 mainline support
is maturing (VOP2/DSI landed, Panfrost replaces the Mali blob); a future
migration would drop most of the vendor code this system carries.

### WiFi

The radio is a Broadcom/AMPAK **AP6281S** (Synaptics SYN4381, SDIO
`02D0:4381`), driven by the in-tree `bcmdhd` (`CONFIG_AP6XXX=m`), loaded by
`rootfs_overlay/usr/bin/load-wifi-modules` with errors-only log levels.
Firmware, NVRAM, CLM, and the driver's runtime tuning file
(`config_syn4381a0.txt`) ship in `package/ap6281-firmware` and install to
both `/lib/firmware` and `/vendor/etc/firmware` (the path compiled into
bcmdhd).

The tuning file targets a stationary, mains-powered device: roaming off
(`roam_off=1`), powersave off (`PM=0`), a 120 s firmware keepalive, and
gratuitous ARP on link-up. Keys are case-sensitive; the full list is in
the driver's `dhd_config.c`. No packet filters are set — the device must
receive mDNS multicast.

## Hardware

The device is the **ELC (Shenzhen Electron Technology) SMT1019** smart-home
panel (sold retail as "JEESTON"; FCC ID
[2ABC5-E0119](https://fccid.io/2ABC5-E0119), hardware `R157-V1.0A`). Per the
manufacturer's manual: four microphones, 2x2 W box-chamber speakers, RGB LED
strip, temp/humidity + light sensors, 5 MP camera, PoE, and an in-wall 86-box
module carrying RS-485/RS-232/relays/IO (the DT's `io_control` GPIOs and
spare UARTs). Matter/Thread capable via the radio module; Zigbee optional.
What the FCC filing's public photo annexes establish (86-box terminal
pinout, PoE module, mic bar, DEBUG pads, radio-variant evidence) is
summarized in [docs/fcc-overview.md](docs/fcc-overview.md). Recovery via
loader/maskrom mode is documented in [uboot/README.md](uboot/README.md)
with the maskrom button location.

Spec sheets for these components live in [docs/datasheets/](docs/datasheets/).
SoC: Rockchip **RK3576S** ([datasheet](docs/datasheets/rk3576s-soc.pdf);
[full RK3576 datasheet](docs/datasheets/rk3576-soc.pdf)), powered by a
Rockchip RK806 PMIC on i2c1@0x23
([datasheet](docs/datasheets/rk806-pmic.pdf)).

| Peripheral | Device | Notes |
| --- | --- | --- |
| Touchscreen | Goodix GT9271 (i2c0@0x14) — [datasheet](docs/datasheets/gt9271-touch-controller.pdf) | Mainline `goodix.c` (patch 0012) |
| Display | 800×1280 MIPI-DSI panel (SQ101E-Q4EI409-39C501, no public datasheet) | VOP2 + DSI2, panel timings/init in the DT; Mali G52 GPU via vendor kmod + libmali (EGL/GBM) |
| Temp/humidity | GXCAS GXHT30, SHT30-compatible (i2c0@0x44) — [datasheet](docs/datasheets/gxht30-temp-humidity.pdf) | Userspace via Circuits.I2C |
| Proximity + ALS | Sensortek STK3311-X (i2c4@0x48) — [datasheet](docs/datasheets/stk3311-als-proximity.pdf) | Userspace via Circuits.I2C (DT node `ls_stk3x1x`; no kernel driver bound) |
| RGB light ring | R/G/B on pwm2 ch6/ch2/ch5, enable GPIO4_C7 | Mainline `leds-pwm` → `/sys/class/leds/{red,green,blue}` |
| Audio | ES8388/ES8323-family codec (i2c3@0x10) — [datasheet](docs/datasheets/es8388-audio-codec.pdf), [user guide](docs/datasheets/es8388-user-guide.pdf) + ES7202 PDM mic ADC (i2c3@0x32) — [brief](docs/datasheets/es7202-pdm-adc.pdf) | ALSA; LADSPA voicing/protection chain (`package/smt-audio-dsp`) |
| WiFi/BT/Thread | AMPAK AP6281S (Synaptics SYN4381, SDIO 02d0:4381 + UART ttyS4) — [module datasheet](docs/datasheets/ap6281s-wifi-bt-module.pdf), [SoC brief](docs/datasheets/syn4381-triple-combo-brief.pdf) | WiFi 6E working; BT and 802.15.4/Thread share the single module UART |
| Ethernet | Realtek RTL8111H (PCIe 10ec:8168 rev 0x15) | Gigabit; MAC from vendor storage |
| Power | PoE over the ethernet port | Powers the device standalone (no DC input) |
| RTC | Haoyu HYM8563 (i2c2@0x51) — [datasheet](docs/datasheets/hym8563-rtc.pdf) | Sets system clock at boot |
| Accelerometer | Kionix KXTJ3 (i2c7@0x0e) — [datasheet](docs/datasheets/kxtj3-accelerometer.pdf) | Userspace via Circuits.I2C; the DT's second footprint (BMA2xx @0x18) is unpopulated |
| NPU | RKNPU (driver 0.9.8, DRM render node `/dev/dri/renderD129`) | Works with librknnrt 2.3.2; 3 TOPS INT8 rated (500 MHz S bin), 558 GOPS measured dense int8 matmul |
| NFC | NXP PN5xx family (i2c7@0x28) | Not working — see hardware status |
| Camera | GC5035 (MIPI-CSI, 5 MP) | Working: rkcif/rkisp + rkaiq 3A (`package/rkaiq`), `camsnap` live view |
| io_control | 4 general-purpose GPIOs | Circuits.GPIO |
