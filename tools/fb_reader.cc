#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <cstdio>
#include <cstdint>

int main() {
    int fd = open("/dev/shm/fb", O_RDONLY);
    if (fd < 0) { perror("open"); return 1; }
    struct stat st{};
    if (fstat(fd, &st) < 0) { perror("fstat"); close(fd); return 1; }

    void* ptr = mmap(nullptr, st.st_size, PROT_READ, MAP_SHARED, fd, 0);
    if (ptr == MAP_FAILED) { perror("mmap"); close(fd); return 1; }

    auto* p = static_cast<uint8_t*>(ptr);
    for (int i = 0; i < 16; ++i) printf("%02x ", p[i]);
    printf("\n");

    munmap(ptr, st.st_size);
    close(fd);
    return 0;
}
