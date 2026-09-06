#ifndef DEBUG_H
#define DEBUG_H
#include "cstr.h"
#include "pictl.h"
static inline void panic(const char *msg)
{
    printk("PANIC: %s\n", msg);
    rpi_reboot();
}
static inline void assert(int x)
{
    if (!x)
        panic("assertion failed\n");
}
#define debug printk
#endif