# libmali (Mali-G52 userspace)

Proprietary ARM Mali userspace (EGL/GLESv2/GBM) for the RK3576 Mali-G52,
DDK **g24p0**, plain-GBM variant (no X11/Wayland deps). Pairs with the
in-kernel Mali Bifrost module (`CONFIG_MALI_BIFROST`) that creates
`/dev/mali0`.

Forked from Buildroot's `rockchip-mali` package, which is hardcoded to G31.

## The blob (downloaded — 54 MB)

`libmali-bifrost-g52-g24p0-gbm.so` is fetched at build time from Rockchip's
public libmali distribution — the `libmali` branch of
[JeffyCN/mirrors](https://github.com/JeffyCN/mirrors/tree/libmali), pinned
by commit in `libmali.mk` and verified against `libmali.hash` (the public
blob is byte-identical to the one in the vendor SDK's `external/libmali`).

Headers (`include/`) and pkgconfig (`pkgconfig/`) are committed.

## DDK version note

The kernel's Mali kmod reports DDK **g25p0**; this userspace is **g24p0**
(the newest G52 GBM variant available). Adjacent DDK releases share the
kbase UABI, so they are expected to be compatible — verify with `kmscube`.
If the GPU fails with a kbase version mismatch, try another DDK variant
from the mirror's `lib/aarch64-linux-gnu/` or switch to the matched
out-of-tree `mali-driver` kmod.

## Display bring-up findings (verified with kmscube on the DSI panel)

Getting GPU output onto the MIPI-DSI panel required two things — both will
apply to any EGL/GBM UI stack:

1. **Linear buffers, not AFBC.** libmali allocates ARM AFBC-compressed GPU
   buffers by default, but the vop2 primary plane driving the DSI panel
   cannot scan out AFBC (boot log: `unsupported AFBC format`). Result is a
   blank panel even though rendering succeeds. `kmscube -m 0` forces the
   linear modifier and the cube appears. A UI stack must likewise use
   linear scanout buffers (e.g. GBM `LINEAR` modifier, or the libmali
   AFBC-disable env var).

2. **Release the framebuffer console.** fbcon owns the panel (IEx renders
   there via `tty1`). Unbind it before a KMS app takes over:
   `for f in /sys/class/vtconsole/vtcon*/bind; do echo 0 > $f; done`.
   For the product, move the console off the panel (SSH/usb0 are available)
   or have the UI app unbind fbcon on launch.

Confirmed working: `kmscube -m 0` renders a spinning cube on the panel;
`renderer = Mali-G52`, GLES 3.2, over DRM/KMS EGL.

## Open alternative (Panfrost)

The open Mesa Panfrost path was evaluated (see git history: the
`0004-dts-...panfrost` patch and `CONFIG_DRM_PANFROST`). It works on this
GPU but Mesa 25's panfrost requires building LLVM, which needs a Docker VM
with >12 GB RAM. Revisit when that's available for a blob-free GPU stack.
