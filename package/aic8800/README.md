# aic8800

AICSemi AIC8800D80 SDIO WiFi support for SMT1019 units fitted with that
radio (the manufacturer ships two sourcing variants — see the top-level
README). The fitted radio is identified at boot by
`/usr/bin/load-wifi-modules` from the SDIO IDs:

| Radio | SDIO IDs | Driver |
|---|---|---|
| AMPAK AP6281S (Broadcom SYN4381) | `02d0:4381` | in-kernel `bcmdhd` + `package/ap6281-firmware` |
| AICSemi AIC8800D80 | `c8a1:0082` / `c8a1:0182` | this package (`aic8800_bsp` + `aic8800_fdrv`) |

## Provenance

- `src/` — the Rockchip SDK's `external/rkwifibt/drivers/aic8800_sdio`
  (`rk3576-linux6.1-20251118`, AICSemi driver release
  `2025_0410_b99ca8b6`), GPL-2.0. Built against the system kernel via
  Buildroot's kernel-module infrastructure. The Bluetooth module
  (`aic8800_btlpm`) is not built.
- `firmware/` — proprietary AICSemi firmware from
  `external/rkwifibt/firmware/aic/sdio`, installed flat into
  `/lib/firmware` (the driver is built with `CONFIG_USE_FW_REQUEST=n`
  and opens bare file names under `CONFIG_AIC_FW_PATH`). Redistributed
  unmodified for use with this hardware.

## Quick test (on an AIC8800 unit)

```
cmd("lsmod | grep aic")               # aic8800_fdrv + aic8800_bsp loaded
cmd("ip link show wlan0")             # interface exists
VintageNet.info                       # then configure wifi as usual
```
