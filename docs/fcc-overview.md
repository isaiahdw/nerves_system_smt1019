# FCC filing overview — 2ABC5-E0119 (ELC SMT1019)

The device is FCC-certified as **2ABC5-E0119**; the full filing, including
the external (report `…-AB`) and internal (`…-AC`) photo annexes, is public
at [fccid.io/2ABC5-E0119](https://fccid.io/2ABC5-E0119). The schematics
exhibit is permanently confidential. The tested unit is main board
`R157-V2.0 20251031` — our board family (our unit's model string is
`R157 V10A`).

This is a summary of what the photo annexes establish about the hardware;
consult the filing itself for the images.

## Retail kit

Ships with three green phoenix terminal plugs, a screwdriver + mounting
screw, a 12 V DC barrel adapter (for non-PoE installs), a wall-mount
bracket, and the panel. The 86-box module on the back fits an in-wall
86 box: RJ45 + USB-C + DC jack on its top face, the terminal row on the
bottom.

## 86-box terminal pinout

Embossed legend on the module, left to right:

| Group | Pins | Purpose |
|---|---|---|
| Relay | `1 · COM · 2` | Two dry-contact relay outputs, shared common |
| I/O | `GND · 1 · 2` | Two logic-level GPIO in/outs |
| IR | `GND · IR · VCC` | Wired IR emitter (blaster) output |
| 485 | `A · B` | RS-485 half-duplex bus |
| 232 | `GND · RX · TX` | RS-232 serial |

All driven from the SoC — the DT's `io_control` GPIOs and the
enabled-but-unused UARTs `uart1/6/9` are the likely back-ends; the exact
mapping needs a loopback test or the ODM. The relays switch real
low-voltage loads (door strike, HVAC call, siren); RS-485 enables Modbus
RTU multi-drop integration; RS-232 covers legacy AV/security gear.

## Internal findings

- **86-box module PCB** `R128-USB-V1.2` (the `R128` prefix is shared with
  the smaller SMT101 family): `WC-PD25E` PoE PD power module (~25 W class,
  explaining the comfortable PoE headroom), two mechanical relays, RJ45
  magnetics, USB-C, DC jack, terminal row.
- **Microphone bar** `R128-MIC-B-V1.1` (~125 mm strip): four MEMS
  microphones with back-side acoustic ports, FPC tail to the main board's
  `PDM-MIC` connector — digital PDM straight into the SoC's pdm1
  controller (the validated capture path; the on-board ES7202 analog ADC
  is unused in this configuration).
- **Main board** `R157-V2.0` under a copper SoC heatsink. Edge connector
  silk: `PWRON SPK CAMERA TEMP SPK LED TP MIPI EDP WIFI PDM-MIC VOL` —
  note two `SPK` connectors (stereo confirmed at board level).
- **Radio sourcing**: the FCC-tested unit carries an **AIC8800D80** radio
  module — the *other* sourcing option (our units carry the AMPAK
  AP6281S/Synaptics SYN4381, matching the DT's runtime radio detection).
  The FCC-tested configuration is therefore not the Thread-capable
  variant.
- **Antennas**: a combined BT + 2.4/5.1/5.8 GHz FPC antenna behind the
  top-left speaker box, plus a second FPC antenna (BT/auxiliary), both
  u.FL.
- **DEBUG pads** on the main-board back (top edge, next to the `BAT`
  connector): the serial/FIQ-debugger console (ttyFIQ0, 1.5 Mbaud). The
  small button near the eMMC on the same side is the **maskrom button** —
  see [uboot/README.md](../uboot/README.md) for the recovery-mode
  procedure and an annotated photo.
