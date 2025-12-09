#include "fb_sink.h"
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <cerrno>
#include <cstring>
#include <algorithm>

// Minimal RGBA sink that writes to /dev/shm/fb
class DevShmFbSink : public FbSink {
    int m_fd = -1;
    void* m_ptr = nullptr;
    size_t m_size = 0;
    FbSinkConfig m_cfg{};

    static size_t bpp(uint32_t /*fmt*/) {
        return 4; // RGBA
    }

public:
    ~DevShmFbSink() override {
        if (m_ptr && m_ptr != MAP_FAILED) munmap(m_ptr, m_size);
        if (m_fd >= 0) close(m_fd);
        // Do not unlink: external readers may mmap it
    }

    int init(const FbSinkConfig& cfg) override {
        if (cfg.format != HAL_PIXEL_FORMAT_RGBA_8888) {
            return -EINVAL;
        }
        m_cfg = cfg;
        m_size = static_cast<size_t>(cfg.width) * static_cast<size_t>(cfg.height) * bpp(cfg.format);

        // Create or truncate /dev/shm/fb
        m_fd = ::open("/dev/shm/fb", O_RDWR | O_CREAT | O_TRUNC, 0666);
        if (m_fd < 0) return -errno;
        if (ftruncate(m_fd, static_cast<off_t>(m_size)) < 0) return -errno;

        m_ptr = mmap(nullptr, m_size, PROT_READ | PROT_WRITE, MAP_SHARED, m_fd, 0);
        if (m_ptr == MAP_FAILED) { m_ptr = nullptr; return -errno; }
        return 0;
    }

    int present(buffer_handle_t srcHandle,
                uint32_t srcWidth,
                uint32_t srcHeight,
                uint32_t srcStridePixels,
                uint32_t srcFormat) override
    {
        if (!m_ptr) return -EINVAL;
        if (srcFormat != HAL_PIXEL_FORMAT_RGBA_8888) {
            // Enforce RGBA
            return -EINVAL;
        }

        // TODO: Lock srcHandle via platform gralloc/mapper for CPU read on Android 13
        // Example (pseudo):
        // void* srcPtr = nullptr;
        // uint32_t srcRowBytes = srcStridePixels * 4;
        // mapper_lock_for_read(srcHandle, &srcPtr, &srcRowBytes);

        void* srcPtr = nullptr; // Replace with mapper lock result
        size_t bytesPerPixel = 4;
        size_t srcRowBytes = static_cast<size_t>(srcStridePixels) * bytesPerPixel;
        size_t dstRowBytes = static_cast<size_t>(m_cfg.width) * bytesPerPixel;
        size_t copyRowBytes = std::min(dstRowBytes, srcRowBytes);
        size_t rows = std::min(static_cast<size_t>(m_cfg.height), static_cast<size_t>(srcHeight));

        if (!srcPtr) {
            // No-op until mapper wired
            return 0;
        }

        uint8_t* dst = static_cast<uint8_t*>(m_ptr);
        const uint8_t* src = static_cast<const uint8_t*>(srcPtr);

        for (size_t y = 0; y < rows; ++y) {
            std::memcpy(dst + y * dstRowBytes, src + y * srcRowBytes, copyRowBytes);
        }

        // TODO: mapper_unlock(srcHandle)
        return 0;
    }
};

extern "C" FbSink* create_fb_sink() {
    return new DevShmFbSink();
}
