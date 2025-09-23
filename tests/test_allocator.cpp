
#include "../include/allocator.h"
#include <cstdio>
#include <cstring>
#include <cassert>
#include <iostream>

int main() {
    mini_alloc::init_allocator(1024 * 1024); // 1 MiB heap for test
    std::puts("=== mini_alloc test starting ===");

    // simple alloc
    void* a = mini_alloc::malloc(64);
    assert(a && "malloc returned null");
    std::memset(a, 0xAA, 64);
    std::printf("allocated a @ %p\n", a);

    // second alloc
    void* b = mini_alloc::malloc(128);
    std::memset(b, 0xBB, 128);
    std::printf("allocated b @ %p\n", b);

    mini_alloc::dump_heap();

    // free a and coalesce
    mini_alloc::free(a);
    std::puts("freed a");
    mini_alloc::dump_heap();

    // realloc b to larger
    b = mini_alloc::realloc(b, 512);
    std::puts("reallocated b -> 512");
    mini_alloc::dump_heap();

    // allocate many small blocks
    void* blocks[8];
    for (int i = 0; i < 8; ++i) {
        blocks[i] = mini_alloc::malloc(50 + i * 10);
        std::printf("blocks[%d] = %p\n", i, blocks[i]);
    }
    mini_alloc::dump_heap();

    // free some (only if non-null)
    if (blocks[2]) mini_alloc::free(blocks[2]);
    if (blocks[3]) mini_alloc::free(blocks[3]);
    std::puts("freed blocks[2], blocks[3]");
    mini_alloc::dump_heap();

    // calloc test
    char* z = static_cast<char*>(mini_alloc::calloc(10, 4)); // 40 bytes zeroed
    for (int i = 0; i < 40; ++i) assert(z[i] == 0);
    std::puts("calloc test OK");
    mini_alloc::free(z);

    // big realloc shrink
    void* big = mini_alloc::malloc(1024);
    std::puts("allocated big");
    mini_alloc::dump_heap();
    big = mini_alloc::realloc(big, 128);
    std::puts("shrunk big -> 128");
    mini_alloc::dump_heap();

    // cleanup
    for (int i = 0; i < 8; ++i) mini_alloc::free(blocks[i]);
    mini_alloc::free(b);
    mini_alloc::free(big);

    std::puts("Final heap:");
    mini_alloc::dump_heap();

    std::puts("=== mini_alloc test finished ===");
    return 0;
}
