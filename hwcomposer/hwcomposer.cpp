#include "display.h"
#include "fb_sink.h"
#include <cutils/properties.h>
#include <log/log.h>
#include <sync/sync.h>
#include <hardware/hwcomposer.h>

extern "C" FbSink* create_fb_sink();

struct waydroid_hwc_composer_device_1 {
    hwc_composer_device_1_t common;
    int64_t vsync_period_ns;
    int timeline_fd;
    int next_sync_point;
    display* display;
};

static int hwc_close(hw_device_t* device) { (void)device; ALOGV("hwc_close"); return 0; }

static int hwc_prepare(hwc_composer_device_1_t* dev, size_t numDisplays, hwc_display_contents_1_t** displays) {
    ALOGV("hwc_prepare: numDisplays=%zu", numDisplays);
    (void)dev; (void)numDisplays; (void)displays;
    return 0;
}

static int hwc_set(hwc_composer_device_1_t* dev, size_t numDisplays, hwc_display_contents_1_t** displays) {
    ALOGV("hwc_set: numDisplays=%zu", numDisplays);
    auto* pdev = reinterpret_cast<waydroid_hwc_composer_device_1*>(dev);
    if (!pdev || !displays || numDisplays == 0) { ALOGV("hwc_set: invalid args"); return -EINVAL; }
    auto* contents = displays[0];
    if (!contents) { ALOGV("hwc_set: contents[0] is null"); return 0; }

    ALOGV("hwc_set: numHwLayers=%zu", contents->numHwLayers);
    for (size_t i = 0; i < contents->numHwLayers; ++i) {
        hwc_layer_1_t& layer = contents->hwLayers[i];
        ALOGV("layer[%zu]: type=%d acquireFenceFd=%d handle=%p", i, layer.compositionType, layer.acquireFenceFd, (void*)layer.handle);
        if (layer.compositionType == HWC_FRAMEBUFFER_TARGET) {
            ALOGV("Found HWC_FRAMEBUFFER_TARGET at index %zu", i);
            if (layer.acquireFenceFd >= 0) { ALOGV("Closing acquireFenceFd=%d", layer.acquireFenceFd); close(layer.acquireFenceFd); layer.acquireFenceFd = -1; }
            if (pdev->display && pdev->display->sink) {
                uint32_t w = static_cast<uint32_t>(pdev->display->width);
                uint32_t h = static_cast<uint32_t>(pdev->display->height);
                uint32_t stridePixels = w; // TODO: query via mapper
                uint32_t fmt = HAL_PIXEL_FORMAT_RGBA_8888;
                ALOGV("present: handle=%p w=%u h=%u stride=%u fmt=%u", (void*)layer.handle, w, h, stridePixels, fmt);
                pdev->display->sink->present(layer.handle, w, h, stridePixels, fmt);
            } else {
                ALOGV("No display or sink available");
            }
            break;
        }
    }
    return 0;
}

static int hwc_get_display_configs(hwc_composer_device_1_t* dev, int dpy, uint32_t* configs, size_t* numConfigs) {
    ALOGV("hwc_get_display_configs: dpy=%d", dpy);
    if (!numConfigs) return -EINVAL;
    *numConfigs = 1;
    if (configs) configs[0] = 0;
    ALOGV("hwc_get_display_configs: numConfigs=%zu config[0]=%u", *numConfigs, configs ? configs[0] : 0);
    return 0;
}

static int hwc_get_display_attributes(hwc_composer_device_1_t* dev, int dpy, uint32_t config, const uint32_t* attributes, int32_t* values) {
    ALOGV("hwc_get_display_attributes: dpy=%d config=%u", dpy, config);
    auto* pdev = reinterpret_cast<waydroid_hwc_composer_device_1*>(dev);
    if (!pdev || !attributes || !values) return -EINVAL;
    for (size_t i = 0; attributes[i] != HWC_DISPLAY_NO_ATTRIBUTE; ++i) {
        switch (attributes[i]) {
            case HWC_DISPLAY_VSYNC_PERIOD: values[i] = static_cast<int32_t>(pdev->vsync_period_ns); ALOGV("attr[%zu]=VSYNC_PERIOD -> %d", i, values[i]); break;
            case HWC_DISPLAY_WIDTH: values[i] = pdev->display ? pdev->display->width : 1280; ALOGV("attr[%zu]=WIDTH -> %d", i, values[i]); break;
            case HWC_DISPLAY_HEIGHT: values[i] = pdev->display ? pdev->display->height : 800; ALOGV("attr[%zu]=HEIGHT -> %d", i, values[i]); break;
            case HWC_DISPLAY_DPI_X: values[i] = 320; ALOGV("attr[%zu]=DPI_X -> %d", i, values[i]); break;
            case HWC_DISPLAY_DPI_Y: values[i] = 320; ALOGV("attr[%zu]=DPI_Y -> %d", i, values[i]); break;
            default: values[i] = -1; ALOGV("attr[%zu]=unknown(%u) -> -1", i, attributes[i]); break;
        }
    }
    return 0;
}

static int hwc_module_open(const struct hw_module_t* module, const char* name, struct hw_device_t** device) {
    ALOGV("hwc_module_open: name=%s", name ? name : "(null)");
    char property[PROPERTY_VALUE_MAX];
    if (strcmp(name, HWC_HARDWARE_COMPOSER)) {
        ALOGE("%s called with bad name %s", __FUNCTION__, name);
        return -EINVAL;
    }

    auto* pdev = new waydroid_hwc_composer_device_1();
    if (!pdev) return -ENOMEM;

    pdev->common.tag = HARDWARE_DEVICE_TAG;
    pdev->common.version = HWC_DEVICE_API_VERSION_1_5;
    pdev->common.module = const_cast<hw_module_t *>(module);
    pdev->common.close = hwc_close;

    pdev->vsync_period_ns = 1000LL*1000LL*1000LL/60;
    ALOGV("vsync_period_ns=%lld", (long long)pdev->vsync_period_ns);
    pdev->timeline_fd = sw_sync_timeline_create();
    ALOGV("timeline_fd=%d", pdev->timeline_fd);
    pdev->next_sync_point = 1;

    auto* d = new display();
    int w = 1280, h = 800;
    if (property_get("persist.waydroid.width", property, nullptr) > 0) w = atoi(property);
    if (property_get("persist.waydroid.height", property, nullptr) > 0) h = atoi(property);
    ALOGV("display size: %dx%d", w, h);
    d->width = w; d->height = h;

    FbSink* sink = create_fb_sink();
    FbSinkConfig cfg{ static_cast<uint32_t>(d->width), static_cast<uint32_t>(d->height), HAL_PIXEL_FORMAT_RGBA_8888 };
    ALOGV("FbSink init: w=%u h=%u fmt=%u", cfg.width, cfg.height, cfg.format);
    if (sink->init(cfg) != 0) {
        ALOGE("Failed to init /dev/shm/fb sink");
        delete sink; delete d; delete pdev;
        return -ENODEV;
    }
    d->sink = sink;
    pdev->display = d;

    *device = reinterpret_cast<hw_device_t*>(pdev);
    ALOGI("hwctest HWC initialized: RGBA sink at /dev/shm/fb");
    return 0;
}

static struct hw_module_methods_t hwc_module_methods = { .open = hwc_module_open };

extern "C" {
    struct hw_module_t HAL_MODULE_INFO_SYM = {
        .tag = HARDWARE_MODULE_TAG,
        .module_api_version = HWC_MODULE_API_VERSION_0_1,
        .hal_api_version = HARDWARE_HAL_API_VERSION,
        .id = HWC_HARDWARE_MODULE_ID,
        .name = "hwctest HWC",
        .author = "supechicken",
        .methods = &hwc_module_methods,
        .dso = nullptr,
        .reserved = {0}
    };
}
