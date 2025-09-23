
#include "allocator.h"
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <cassert>
#include <new>
#include <algorithm>
#include <limits>

namespace mini_alloc {

constexpr std::size_t DEFAULT_HEAP_SIZE = 16 * 1024 * 1024; // 16 MiB
constexpr std::size_t ALIGNMENT = alignof(std::max_align_t);
constexpr std::size_t MIN_BLOCK_SIZE = 32; // minimum payload size

// Align up utility
static inline std::size_t align_up(std::size_t n, std::size_t a = ALIGNMENT) {
    return (n + (a - 1)) & ~(a - 1);
}

// Block header
struct alignas(ALIGNMENT) Block {
    std::size_t size;   // size of payload
    bool free;          // is block free?
    Block* next;
    Block* prev;
};

static char* heap_base = nullptr;
static std::size_t heap_total_size = 0;
static Block* head = nullptr;
static bool initialized = false;

void init_allocator(std::size_t heap_size) {
    if (initialized) return;
    heap_total_size = align_up(heap_size);
    heap_base = static_cast<char*>(std::malloc(heap_total_size));
    if (!heap_base) throw std::bad_alloc();
    std::memset(heap_base, 0, heap_total_size);

    head = reinterpret_cast<Block*>(heap_base);
    std::size_t hdr_sz = align_up(sizeof(Block));
    std::size_t initial_payload = heap_total_size - hdr_sz;
    head->size = initial_payload;
    head->free = true;
    head->next = nullptr;
    head->prev = nullptr;

    initialized = true;
}

static inline Block* payload_to_block(void* payload) {
    if (!payload) return nullptr;
    return reinterpret_cast<Block*>(
        reinterpret_cast<char*>(payload) - align_up(sizeof(Block))
    );
}

static inline void* block_to_payload(Block* b) {
    if (!b) return nullptr;
    return reinterpret_cast<void*>(reinterpret_cast<char*>(b) + align_up(sizeof(Block)));
}

static inline bool pointer_in_heap(void* p) {
    if (!p || !initialized) return false;
    auto pc = reinterpret_cast<char*>(p);
    return pc >= heap_base && pc < (heap_base + static_cast<std::ptrdiff_t>(heap_total_size));
}

static Block* find_fit(std::size_t asize) {
    for (Block* b = head; b; b = b->next) {
        if (b->free && b->size >= asize) return b;
    }
    return nullptr;
}

static void split_block(Block* b, std::size_t asize) {
    std::size_t hdr_sz = align_up(sizeof(Block));
    if (!b->free) return;

    if (b->size >= asize + hdr_sz + MIN_BLOCK_SIZE) {
        char* block_addr = reinterpret_cast<char*>(b);
        std::size_t remaining_payload = b->size - asize - hdr_sz;

        Block* newb = reinterpret_cast<Block*>(block_addr + hdr_sz + asize);
        newb->size = remaining_payload;
        newb->free = true;
        newb->prev = b;
        newb->next = b->next;
        if (b->next) b->next->prev = newb;
        b->next = newb;
        b->size = asize;
    }
    b->free = false;
}

static Block* coalesce(Block* b) {
    std::size_t hdr_sz = align_up(sizeof(Block));
    if (b->next && b->next->free) {
        Block* nxt = b->next;
        b->size += hdr_sz + nxt->size;
        b->next = nxt->next;
        if (nxt->next) nxt->next->prev = b;
    }
    if (b->prev && b->prev->free) {
        Block* pv = b->prev;
        pv->size += hdr_sz + b->size;
        pv->next = b->next;
        if (b->next) b->next->prev = pv;
        b = pv;
    }
    return b;
}

void* malloc(std::size_t size) {
    if (size == 0) return nullptr;
    if (!initialized) init_allocator(DEFAULT_HEAP_SIZE);

    std::size_t asize = align_up(size);
    if (asize < MIN_BLOCK_SIZE) asize = MIN_BLOCK_SIZE;

    Block* b = find_fit(asize);
    if (!b) return nullptr;

    split_block(b, asize);
    return block_to_payload(b);
}

void free(void* ptr) {
    // 1) free(nullptr) must be safe
    if (!ptr) return;

    // 2) check if pointer is inside our managed heap
    if (ptr < heap_base || ptr >= (heap_base + heap_total_size)) {
        std::fprintf(stderr, "[mini_alloc] free: pointer %p not from heap\n", ptr);
        return;
    }

    // 3) payload -> block header
    Block* b = payload_to_block(ptr);

    // 4) sanity check: header must lie inside heap
    if ((char*)b < heap_base || (char*)b >= (heap_base + heap_total_size)) {
        std::fprintf(stderr, "[mini_alloc] free: invalid block header for %p\n", ptr);
        return;
    }

    // 5) detect double free
    if (b->free) {
        std::fprintf(stderr, "[mini_alloc] warning: double free of %p\n", ptr);
        return;
    }

    // 6) mark free and coalesce
    b->free = true;
    coalesce(b);
}


void* realloc(void* ptr, std::size_t new_size) {
    if (!ptr) return malloc(new_size);
    if (new_size == 0) {
        free(ptr);
        return nullptr;
    }
    if (!pointer_in_heap(ptr)) {
        std::fprintf(stderr, "[mini_alloc] realloc: pointer %p not from heap\n", ptr);
        return nullptr;
    }
    Block* b = payload_to_block(ptr);
    std::size_t asize = align_up(new_size);
    if (asize < MIN_BLOCK_SIZE) asize = MIN_BLOCK_SIZE;

    if (b->size >= asize) {
        split_block(b, asize);
        return ptr;
    } else if (b->next && b->next->free) {
        std::size_t hdr_sz = align_up(sizeof(Block));
        std::size_t combined = b->size + hdr_sz + b->next->size;
        if (combined >= asize) {
            Block* nxt = b->next;
            b->size = combined;
            b->next = nxt->next;
            if (nxt->next) nxt->next->prev = b;
            split_block(b, asize);
            b->free = false;
            return ptr;
        }
    }

    void* newp = malloc(new_size);
    if (!newp) return nullptr;
    std::memcpy(newp, ptr, std::min(b->size, asize));
    free(ptr);
    return newp;
}

void* calloc(std::size_t nmemb, std::size_t size) {
    if (nmemb == 0 || size == 0) return nullptr;
    if (nmemb > std::numeric_limits<std::size_t>::max() / size) return nullptr;
    std::size_t total = nmemb * size;
    void* p = malloc(total);
    if (p) std::memset(p, 0, total);
    return p;
}

void dump_heap() {
    if (!initialized) {
        std::puts("[mini_alloc] heap not initialized");
        return;
    }
    std::printf("mini_alloc: heap_base=%p total=%zu bytes\n", (void*)heap_base, heap_total_size);
    std::size_t idx = 0;
    for (Block* b = head; b; b = b->next, ++idx) {
        std::printf(" block[%zu] hdr=%p payload=%p size=%zu free=%s prev=%p next=%p\n",
                    idx,
                    (void*)b,
                    block_to_payload(b),
                    b->size,
                    b->free ? "YES" : "NO",
                    (void*)(b->prev),
                    (void*)(b->next));
    }
}

} // namespace mini_alloc
