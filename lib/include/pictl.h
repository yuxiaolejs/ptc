#ifndef PICTL_H
#define PICTL_H
#include "types.h"
#include "serial.h"
#include "regs.h"
#include "mem.h"

#define POWER_STATE_ON (1 << 0)
#define POWER_STATE_WAIT (1 << 1)
#define USB_DEVICE_ID 3

void rpi_reboot(void);
void delay_ms(uint32_t ms);
void delay_cycles(uint32_t cycles);
uint32_t timer_get_usec_raw(void);
uint32_t timer_get_usec(void);
uint32_t timer_get_msec(void);

static inline void udelay(uint32_t n)
{
    while (n--)
        asm volatile("nop");
}


// static inline void dsb(void)
// {
//     asm volatile("mcr p15, 0, %0, c7, c10, 4" : : "r"(0) : "memory");
// }

static inline void writel(uint32_t v, uintptr_t a)
{
    dsb();
    *(volatile uint32_t *)a = v;
    dmb();
}

static inline uint32_t readl(uintptr_t a)
{
    dmb();
    uint32_t v = *(volatile uint32_t *)a;
    dsb();
    return v;
}

uint32_t read_cpsr(void);

void write_cpsr(uint32_t cpsr);

#endif