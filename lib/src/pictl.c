#include "pictl.h"
#include "cstr.h"

static inline void cycle_cnt_init(void)
{
    uint32_t in = 1;
    asm volatile("MCR p15, 0, %0, c15, c12, 0" ::"r"(in));
}

// read cycle counter: should add a write().
static inline uint32_t cycle_cnt_read(void)
{
    uint32_t out;
    asm volatile("MRC p15, 0, %0, c15, c12, 1" : "=r"(out));
    return out;
}

void wait_till_deadline(uint32_t deadline)
{
    while (cycle_cnt_read() < deadline)
        ;
}

void rpi_reboot(void)
{
    uint32_t lr = 0;
    asm volatile("mov %0, lr" : "=r"(lr));
    force_printk("Reboot issued from LR=%x\n", lr);
    libc_uart_puts("DONE!!!\n");
    libc_uart_wait_tx();
    // is there a way to speed this up?
    const int PM_RSTC = 0x2010001c;
    const int PM_WDOG = 0x20100024;
    const int PM_PASSWORD = 0x5a000000;
    const int PM_RSTC_WRCFG_FULL_RESET = 0x00000020;

    // timeout = 1/16th of a second? (whatever)
    PUT32(PM_WDOG, PM_PASSWORD | 1);
    PUT32(PM_RSTC, PM_PASSWORD | PM_RSTC_WRCFG_FULL_RESET);
    while (1)
        ;
}

void delay_ms(uint32_t ms)
{
    cycle_cnt_init();
    uint32_t start = cycle_cnt_read();
    uint32_t cycles_to_wait = ms * 700000; // Assuming a 700MHz CPU
    uint32_t deadline = start + cycles_to_wait;
    wait_till_deadline(deadline);
}

void delay_us(uint32_t us)
{
    uint32_t end_time = timer_get_usec_raw() + us;
    while (timer_get_usec_raw() < end_time)
    {
    }
}

void delay_cycles(uint32_t cycles)
{
    cycle_cnt_init();
    uint32_t start = cycle_cnt_read();
    uint32_t deadline = start + cycles;
    wait_till_deadline(deadline);
}

uint32_t timer_get_usec_raw(void)
{
    return GET32(0x20003004);
}

uint32_t timer_get_usec(void)
{
    return timer_get_usec_raw();
}

uint32_t timer_get_msec(void)
{
    uint64_t usec = (uint64_t)timer_get_usec_raw();
    usec |= ((uint64_t)GET32(0x20003008)) << 32; // high 32 bits
    return (uint32_t)(usec >> 10);               // divide by 1024
}

uint64_t timer_get_full_usec(void)
{
    uint64_t usec = (uint64_t)timer_get_usec_raw();
    usec |= ((uint64_t)GET32(0x20003008)) << 32; // high 32 bits
    return usec;
}

uint32_t read_cpsr(void)
{
    uint32_t cpsr;
    asm volatile(
        "mrs %0, cpsr"
        : "=r"(cpsr)
        :
        : "memory");
    return cpsr;
}

void write_cpsr(uint32_t cpsr)
{
    asm volatile(
        "msr cpsr, %0"
        :
        : "r"(cpsr)
        : "memory");
}