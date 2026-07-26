# ap6281-firmware — licensing and provenance

The blobs in `firmware-broadcom/` are the proprietary runtime firmware,
NVRAM, and CLM data for the AMPAK AP6281S WiFi/BT module
(Broadcom/Synaptics SYN4381 silicon). They were extracted unmodified
from the board vendor's reference OS image for this device, where they
ship for exactly this purpose; the same families of Broadcom/AMPAK
firmware files are commonly redistributed by Linux distributions and
board-support projects for AMPAK modules. They are **not** covered by
the repository's GPL licensing (`AP6281_FIRMWARE_LICENSE = PROPRIETARY`
in the .mk) and are provided solely for use with this hardware.

If you are the rights holder and object to this redistribution, open an
issue and the blobs will be replaced with a download-at-build step.

`config_syn4381a0.txt` (the bcmdhd runtime tuning file) is plain
configuration text maintained in this repository.
