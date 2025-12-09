#pragma once
#include <hardware/hardware.h>
#include <system/graphics.h>
#include <cstdint>
#include <cstddef>

struct FbSinkConfig {
    uint32_t width;
    uint32_t height;
    uint32_t format;   // must be HAL_PIXEL_FORMAT_RGBA_8888
};

class FbSink {
public:
    virtual ~FbSink() = default;
    virtual int init(const FbSinkConfig& cfg) = 0;
    virtual int present(buffer_handle_t srcHandle,
                        uint32_t srcWidth,
                        uint32_t srcHeight,
                        uint32_t srcStridePixels,
                        uint32_t srcFormat) = 0;
};
