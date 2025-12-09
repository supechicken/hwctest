#pragma once
// Minimal placeholder; remove if unused.
struct waydroid_hwc_composer_device_1;
struct hwc_display_contents_1;
struct hwc_layer_1;
class waydroid_mode {
public:
    virtual ~waydroid_mode() = default;
    virtual int setup_prepare(waydroid_hwc_composer_device_1 *, hwc_display_contents_1 *) { return 0; }
    virtual int prepare(hwc_layer_1 *, size_t) { return 0; }
    virtual int setup_set(waydroid_hwc_composer_device_1 *, hwc_display_contents_1 *) { return 0; }
    virtual int handle_layer(waydroid_hwc_composer_device_1 *, hwc_layer_1 *, size_t) { return 0; }
    virtual int post_processing(waydroid_hwc_composer_device_1 *, hwc_display_contents_1 *) { return 0; }
};
