# Hardware datasheets

Spec sheets for the components on the SMT1019 (RK3576S R157 V10A board).
Revisions verified as the latest publicly available as of July 2026.

| File | Component | Revision | Role |
|------|-----------|----------|------|
| `rk3576s-soc.pdf` | Rockchip RK3576S | V1.2 (2025-09) | The fitted SoC variant (documents the S-specific specs, e.g. the 3 TOPS INT8 NPU rating) |
| `rk3576-soc.pdf` | Rockchip RK3576 | V1.6 (2025-09-30) | The full-part datasheet (newer revision, more detail) |
| `rk806-pmic.pdf` | Rockchip RK806 | V1.4 | PMIC |
| `gt9271-touch-controller.pdf` | Goodix GT9271 | Rev.04 (2014-11-11, final) | Touch controller (i2c0@0x14) |
| `ap6281s-wifi-bt-module.pdf` | AMPAK AP6281S | 1.5 (2026-02-11) | WiFi 6E + BT 6.0 + 802.15.4/Thread module (SDIO/UART) |
| `syn4381-triple-combo-brief.pdf` | Synaptics SYN4381 | product brief | The SoC inside the AP6281S (Matter/Thread border-router capable) |
| `es8388-audio-codec.pdf` | Everest ES8388 | Rev 10.0 (2021-10) | Audio codec (i2c3@0x10, speaker/headphone) |
| `es8388-user-guide.pdf` | Everest ES8388 | user guide (2011) | Register-level application notes for the codec |
| `es7202-pdm-adc.pdf` | Everest ES7202 | product brief | PDM mic-array ADC (i2c3@0x32; full DS is NDA-only) |
| `hym8563-rtc.pdf` | Haoyu HYM8563 | final | RTC (i2c2@0x51) |
| `stk3311-als-proximity.pdf` | Sensortek STK3311-X | v0.9.2 (2016-01-25) | Ambient light / proximity (i2c4) |
| `gxht30-temp-humidity.pdf` | GXCAS GXHT30 | V3.9 (2025-09-19) | Temp/humidity sensor, SHT30-compatible (i2c0@0x44) |
| `kxtj3-accelerometer.pdf` | Kionix/ROHM KXTJ3-1057 | current (ROHM) | Accelerometer (i2c7@0x0e; WHO_AM_I 0x35 probed on-device) |

All sheets are verified against the fitted silicon (July 2026 probe):
the SoC reports `rockchip,rk3576s` (PCIe root 1d87:3576); the RK806 binds
at i2c1@0x23 and powers the board; the GT9271 reports "ID 9271" at probe;
the WiFi module enumerates as SDIO 02d0:4381 (SYN4381) rev 1; the ethernet
driver identifies "RTL8168H/8111H" (PCI 10ec:8168 rev 0x15); the RTC binds
as rtc-hym8563 at i2c2@0x51 and keeps time; the ALS answers PDT_ID 0x12 =
STK3311-X at i2c4@0x48 (no kernel driver -- the app reads it directly);
the accelerometer answers WHO_AM_I 0x35 = KXTJ3 at i2c7@0x0e; the codec
(0x10) and PDM ADC (0x32) answer on i2c3 and the audio path works; the
temp/humidity sensor answers at i2c0@0x44 and feeds the app. Two caveats:
the ES8388-vs-ES8323 and GXHT30-vs-SHT30 pairs have no ID registers, so
those identities rest on the working register maps, not silicon IDs.

Notes:

- The vendor DT declares two accelerometer footprints on i2c7 (`gs_kxtj9`
  @0x0e and `bma2xx_acc` @0x18) for BOM variants. Only 0x0e is populated on
  our unit, and its WHO_AM_I (0x35) identifies a KXTJ3, the KXTJ2's drop-in
  successor.
- The AP6281S 802.15.4/Thread interface is UART; the module exposes a single
  UART, shared with Bluetooth (uart4 on this board).
- Not included (no public version exists): the RTL8111H ethernet datasheet
  (Realtek "development partners" material — the r8168 driver source is the
  practical reference), the SQ101E-Q4EI409-39C501 MIPI panel (panel timings
  live in `linux/0001`'s lcd dtsi), and a full Synaptics SYN4381 datasheet
  (the module datasheet plus the product brief cover the integration).
