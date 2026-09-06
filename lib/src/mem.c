#include "mem.h"
#include "cstr.h"
#define INIT_KLALLOC_ADDR 0xC1000000

#define MEM_LOG_ALLOC 0

static uint32_t current_alloc = INIT_KLALLOC_ADDR;
void *klalloc(uint32_t size)
{
    if (current_alloc % 8 != 0)
        current_alloc += 8 - (current_alloc % 8);
    uint32_t old_alloc = current_alloc;
    current_alloc += size;
    return (void *)old_alloc;
}
void klfreeall(void)
{
    current_alloc = INIT_KLALLOC_ADDR;
}

void memset(void *ptr, uint8_t value, uint32_t num)
{
    uint8_t *p = (uint8_t *)ptr;
    for (uint32_t i = 0; i < num; i++)
    {
        p[i] = value;
    }
}

void memset_fast(void *ptr, uint8_t value, uint32_t num)
{
    uint8_t *p = (uint8_t *)ptr;
    // Align to 4 bytes
    while (((uintptr_t)p % 4 != 0) && num > 0)
    {
        *p++ = value;
        num--;
    }
    // Set 4 bytes at a time
    uint32_t val4 = (value << 24) | (value << 16) | (value << 8) | value;
    while (num >= 4)
    {
        *(uint32_t *)p = val4;
        p += 4;
        num -= 4;
    }
    // Set remaining bytes
    while (num > 0)
    {
        *p++ = value;
        num--;
    }
}

// googled
void *memmove_fast(void *dst, const void *src, unsigned int n)
{
    unsigned char *d = dst;
    const unsigned char *s = src;

    if (d == s || n == 0)
        return dst;

    if (d < s)
    {
        // Forward copy
        while ((((unsigned int)d & 3) || ((unsigned int)s & 3)) && n)
        {
            *d++ = *s++;
            n--;
        }

        unsigned int *dw = (unsigned int *)d;
        const unsigned int *sw = (const unsigned int *)s;

        while (n >= 4)
        {
            *dw++ = *sw++;
            n -= 4;
        }

        d = (unsigned char *)dw;
        s = (const unsigned char *)sw;

        while (n--)
            *d++ = *s++;
    }
    else
    {
        // Backward copy
        d += n;
        s += n;

        while ((((unsigned int)d & 3) || ((unsigned int)s & 3)) && n)
        {
            *--d = *--s;
            n--;
        }

        unsigned int *dw = (unsigned int *)d;
        const unsigned int *sw = (const unsigned int *)s;

        while (n >= 4)
        {
            *--dw = *--sw;
            n -= 4;
        }

        d = (unsigned char *)dw;
        s = (const unsigned char *)sw;

        while (n--)
            *--d = *--s;
    }

    return dst;
}

void *memmove(void *dst, const void *src, unsigned int n)
{
    return memmove_fast(dst, src, n);
}

void memcpy(void *dest, const void *src, uint32_t num)
{
    if (!dest || !src)
        return;
    char *dchar = (char *)dest;
    char *dsrc = (char *)src;
    while (((uintptr_t)dchar % 4 != 0 || (uintptr_t)dsrc % 4 != 0) && num > 0)
    {
        *dchar++ = *dsrc++;
        num--;
    }
    while (num >= 4)
    {
        *(uint32_t *)dchar = *(uint32_t *)dsrc;
        dchar += 4;
        dsrc += 4;
        num -= 4;
    }
    while (num > 0)
    {
        *dchar++ = *dsrc++;
        num--;
    }
}

// Begin aligned allocator and free lists
// Allowed allocations: 16, 32, 64, 128, 256, 512, 1024, 2048 bytes
// To support pages and MMU, we need bigger options (section require 1MB aligned)

#define NUM_SIZE_CLASSES 21
#define ALIGNED_ALLOC_BASE 0xC1010000

#define BLOCKS_PER_ARENA 8192

static const uint32_t size_class_sizes[NUM_SIZE_CLASSES] = {
    16,
    32,
    64,
    128,
    256,
    512,
    1024,
    2048,
    4096,
    8192,
    16384,
    32768,
    65536,
    131072,
    262144,
    524288,
    1048576,     // 1MB MAX
    1048576 * 2, // 2MB MAX
    1048576 * 4, // 4MB MAX
    1048576 * 8, // 8MB MAX
    1048576 * 16, // 16MB MAX
};

static const uint32_t size_class_arena_sizes[NUM_SIZE_CLASSES] = {
    16 * BLOCKS_PER_ARENA,
    32 * BLOCKS_PER_ARENA,
    64 * BLOCKS_PER_ARENA,
    128 * BLOCKS_PER_ARENA,
    256 * BLOCKS_PER_ARENA,
    512 * BLOCKS_PER_ARENA,
    1024 * BLOCKS_PER_ARENA,
    2048 * BLOCKS_PER_ARENA,
    4096 * BLOCKS_PER_ARENA << 1,  // We need more 4K pages
    8192 * BLOCKS_PER_ARENA >> 3,  // 8K pages not really useful
    16384 * BLOCKS_PER_ARENA >> 3, // ideally only one 16K page (the L1 section table)
    32768 * 4,
    65536 * 4,
    131072 * 4,
    262144 * 4,
    524288 * 4,
    1048576 * 4,     // ideally only two 1MB pages
    1048576 * 2 * 2, // ideally only two 1MB pages
    1048576 * 4 * 2, // ideally only two 1MB pages
    1048576 * 8 * 2, // ideally only two 1MB pages
    1048576 * 16 * 2, // ideally only two 1MB pages
};

static aligned_size_class_t size_classes[NUM_SIZE_CLASSES];

void 预制菜init()
{
    uint32_t base = ALIGNED_ALLOC_BASE;

    for (int i = 0; i < NUM_SIZE_CLASSES; i++)
    {
        size_classes[i].block_size = size_class_sizes[i];
        size_classes[i].arena_start = base;
        size_classes[i].arena_end = base + size_class_arena_sizes[i];
        force_printk("Size class %d: block size %d, arena [%x, %x)\n", i, size_classes[i].block_size, size_classes[i].arena_start, size_classes[i].arena_end);
        size_classes[i].current = base;

        INIT_LIST_HEAD(&size_classes[i].freelist.free_list);

        base = size_classes[i].arena_end;
    }
}

void *预制菜拼好饭(uint32_t size)
{
    // First, check which size class to use
    uint32_t lr;
    asm volatile("mov %0, lr" : "=r"(lr));
    int class_idx = -1;
    for (int i = 0; i < NUM_SIZE_CLASSES; i++)
    {
        // printk("Checking size class %d with block size %d for request size %d\n", i, size_classes[i].block_size, size);
        if (size <= size_classes[i].block_size)
        {
            class_idx = i;
            break;
        }
    }
    if (class_idx == -1)
    {
        force_printk("Aligned allocator request too large: %d\n", size);
        rpi_reboot();
        return 0;
    }
    aligned_size_class_t *sc = &size_classes[class_idx];
    // We will be allocating from this size class
    // Check freelist first
    if (sc->freelist.free_list.next != &sc->freelist.free_list)
    {
        // grab from freelist
        struct list_head *entry = sc->freelist.free_list.next;
        list_del(entry);
        if (MEM_LOG_ALLOC)
            force_printk("[kalloc] alloc %d,%x,%x\n", size, (uint32_t)entry, lr);
        return (void *)entry;
    }
    // No free stuff, just allocate from arena
    if (sc->current + sc->block_size <= sc->arena_end)
    {
        void *ptr = (void *)sc->current;
        sc->current += sc->block_size;
        if (MEM_LOG_ALLOC)
            force_printk("[kalloc] alloc %d,%x,%x\n", size, (uint32_t)ptr, lr);
        return ptr;
    }
    // Out of memory in this size class
    force_printk("Aligned allocator OOM for size %d, called from %x\n", size, lr);
    return 0;
}
void 预制菜退单(void *ptr)
{
    // Deallocate aligned allocation
    uint32_t lr;
    asm volatile("mov %0, lr" : "=r"(lr));
    if (MEM_LOG_ALLOC)
        force_printk("[kalloc] free %p,%x\n", ptr, lr);
    uint32_t addr = (uint32_t)ptr;
    // Check range to find size class
    for (int i = 0; i < NUM_SIZE_CLASSES; i++)
    {
        aligned_size_class_t *sc = &size_classes[i];
        if (addr >= sc->arena_start && addr < sc->arena_end)
        {
            if ((addr - sc->arena_start) % sc->block_size != 0)
            {
                printk("unaligned free %p\n", ptr);
                return;
            }
            // Found size class
            aligned_free_list_t *afl = (aligned_free_list_t *)ptr;
            list_add_tail(&afl->free_list, &sc->freelist.free_list);
            return;
        }
    }
    printk("Aligned allocator free error: pointer %p not found in any size class\n", ptr);
}
