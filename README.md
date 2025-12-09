# hwctest

Minimal Android HWC (Android 13) that removes Wayland and writes the composed framebuffer to `/dev/shm/fb` in RGBA format. Compatible with minigbm-based gralloc.

## Overview
- HAL: HWC1.5-style device.
- Output: Final framebuffer target (composited by SurfaceFlinger) is copied into `/dev/shm/fb` as RGBA.
- No Wayland, no external window system.
- Display size is controlled by system properties `persist.waydroid.width` and `persist.waydroid.height` (defaults 1280x800).

## Build (AOSP)
- Place this project under `hardware/supechicken/hwctest` (or anywhere in the AOSP tree).
- Run `m hwcomposer.hwctest` or include the module as a dependency in your device build.

## Integration Notes
- Mapper/Gralloc: Implement buffer CPU-read lock/unlock in `fb_sink_devshm.cpp::present()` using your platform’s gralloc/mapper (Android 13 typically uses IMapper via HIDL/AIDL; older trees use mapper v2/3).
- Attributes: Ensure your `getDisplayAttributes`, `getDisplayConfigs`, etc., return data reflecting `display.width`, `display.height`, and `pdev->vsync_period_ns`.
- HAL naming: If your platform expects `hwcomposer.<ro.hardware>.so` naming, rename the module in Android.bp or add a symlink.

## Testing
- Boot the system with this HWC enabled.
- Run `tools/fb_reader` (or your own utility) to mmap `/dev/shm/fb` and inspect pixel data.
