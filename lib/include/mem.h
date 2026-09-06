#ifndef MEM_H
#define MEM_H
#include "linkedlist.h"
#include "types.h"
void *klalloc(uint32_t size); // Linear allocator without free
void klfreeall(void);         // Free all linear allocations
void memset(void *ptr, uint8_t value, uint32_t num);
void memcpy(void *dest, const void *src, uint32_t num);
void *memmove_fast(void *dst, const void *src, unsigned int n);
void *memmove(void *dst, const void *src, unsigned int n);
void memset_fast(void *ptr, uint8_t value, uint32_t num);

typedef struct aligned_free_list
{
    struct list_head free_list;
} aligned_free_list_t;

typedef struct
{
    uint32_t block_size;
    uint32_t arena_start;
    uint32_t arena_end;
    uint32_t current;
    aligned_free_list_t freelist;
} aligned_size_class_t;

void 预制菜init();
void *预制菜拼好饭(uint32_t size); // Aligned allocator
void 预制菜退单(void *ptr);        // Free aligned allocation

#define kalloc 预制菜拼好饭
#define kfree 预制菜退单
#define kalloc_init 预制菜init

// some cache related stuff
static inline void dmb(void)
{
    asm volatile("mcr p15, 0, %0, c7, c10, 5" : : "r"(0) : "memory");
}
static inline void dsb(void)
{
    asm volatile(
        "mov r0, #0\n"
        "mcr p15, 0, r0, c7, c10, 4\n" ::: "r0", "memory");
}

static inline void isb(void)
{
    asm volatile(
        "mov r0, #0\n"
        "mcr p15, 0, r0, c7, c5, 4\n" ::: "r0", "memory");
}

static inline void invalidate_icache(void)
{
    asm volatile(
        "mov r0, #0\n"
        "mcr p15, 0, r0, c7, c5, 0\n"
        "mcr p15, 0, r0, c7, c5, 0\n"
        "mcr p15, 0, r0, c7, c5, 0\n"
        "mcr p15, 0, r0, c7, c5, 0\n" ::: "r0", "memory");
}

static inline void invalidate_dcache(void)
{
    asm volatile(
        "mov r0, #0\n"
        "mcr p15, 0, r0, c7, c6, 0\n" ::: "r0", "memory");
}

static inline void flush_tlb(void)
{
    asm volatile(
        "mov r0, #0\n"
        "mcr p15, 0, r0, c8, c7, 0\n" ::: "r0", "memory");
}

static inline void dcache_clean_mva(void *addr)
{
    asm volatile(
        "mcr p15, 0, %0, c7, c10, 1" ::"r"(addr)
        : "memory");
}

static inline void flush_branch_predictor(void)
{
    asm volatile(
        "mov r0, #0\n"
        "mcr p15, 0, r0, c7, c5, 6\n" ::: "r0", "memory");
    isb();
}

static inline uint32_t read_u32(const uint8_t *p)
{
    return p[0] | (p[1] << 8) | (p[2] << 16) | (p[3] << 24);
}

static inline void write_u32(uint8_t *p, uint32_t v)
{
    p[0] = v & 0xFF;
    p[1] = (v >> 8) & 0xFF;
    p[2] = (v >> 16) & 0xFF;
    p[3] = (v >> 24) & 0xFF;
}

#endif
