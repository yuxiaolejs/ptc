#ifndef REGS_H
#define REGS_H
#include "types.h"

#define MMIO_BASE 0x20000000

// Start interrupt registers
#define ADD_IRQ_BASIC_PENDING MMIO_BASE + 0xB200
#define PTR_IRQ_BASIC_PENDING ((volatile uint32_t *)(MMIO_BASE + 0xB200))
#define ADD_IRQ_PENDING1 MMIO_BASE + 0xB204
#define PTR_IRQ_PENDING1 ((volatile uint32_t *)(MMIO_BASE + 0xB204))
#define ADD_IRQ_PENDING2 MMIO_BASE + 0xB208
#define PTR_IRQ_PENDING2 ((volatile uint32_t *)(MMIO_BASE + 0xB208))
#define ADD_IRQ_FIQ_CONTROL MMIO_BASE + 0xB20C
#define PTR_IRQ_FIQ_CONTROL ((volatile uint32_t *)(MMIO_BASE + 0xB20C))
#define ADD_IRQ_ENABLE1 MMIO_BASE + 0xB210
#define PTR_IRQ_ENABLE1 ((volatile uint32_t *)(MMIO_BASE + 0xB210))
#define ADD_IRQ_ENABLE2 MMIO_BASE + 0xB214
#define PTR_IRQ_ENABLE2 ((volatile uint32_t *)(MMIO_BASE + 0xB214))
#define ADD_IRQ_ENABLE_BASIC MMIO_BASE + 0xB218
#define PTR_IRQ_ENABLE_BASIC ((volatile uint32_t *)(MMIO_BASE + 0xB218))
#define ADD_IRQ_DISABLE1 MMIO_BASE + 0xB21C
#define PTR_IRQ_DISABLE1 ((volatile uint32_t *)(MMIO_BASE + 0xB21C))
#define ADD_IRQ_DISABLE2 MMIO_BASE + 0xB220
#define PTR_IRQ_DISABLE2 ((volatile uint32_t *)(MMIO_BASE + 0xB220))
#define ADD_IRQ_DISABLE_BASIC MMIO_BASE + 0xB224
#define PTR_IRQ_DISABLE_BASIC ((volatile uint32_t *)(MMIO_BASE + 0xB224))
// End interrupt registers

// Start timer registers
#define ADD_TIMER_LOAD MMIO_BASE + 0xB400
#define PTR_TIMER_LOAD ((volatile uint32_t *)(MMIO_BASE + 0xB400))
#define ADD_TIMER_VALUE MMIO_BASE + 0xB404
#define PTR_TIMER_VALUE ((volatile uint32_t *)(MMIO_BASE + 0xB404))
#define ADD_TIMER_CONTROL MMIO_BASE + 0xB408
#define PTR_TIMER_CONTROL ((volatile uint32_t *)(MMIO_BASE + 0xB408))
#define ADD_TIMER_IRQ_CLEAR MMIO_BASE + 0xB40C
#define PTR_TIMER_IRQ_CLEAR ((volatile uint32_t *)(MMIO_BASE + 0xB40C))
#define ADD_TIMER_RAW_IRQ MMIO_BASE + 0xB410
#define PTR_TIMER_RAW_IRQ ((volatile uint32_t *)(MMIO_BASE + 0xB410))
#define ADD_TIMER_MASKED_IRQ MMIO_BASE + 0xB414
#define PTR_TIMER_MASKED_IRQ ((volatile uint32_t *)(MMIO_BASE + 0xB414))
#define ADD_TIMER_RELOAD MMIO_BASE + 0xB418
#define PTR_TIMER_RELOAD ((volatile uint32_t *)(MMIO_BASE + 0xB418))
#define ADD_TIMER_PRE_DIVIDER MMIO_BASE + 0xB41C
#define PTR_TIMER_PRE_DIVIDER ((volatile uint32_t *)(MMIO_BASE + 0xB41C))
#define ADD_TIMER_FREE_RUNNING_COUNTER MMIO_BASE + 0xB420
#define PTR_TIMER_FREE_RUNNING_COUNTER ((volatile uint32_t *)(MMIO_BASE + 0xB420))
// End timer registers

// Start AUX registers
#define PTR_AUX_IRQ ((volatile uint32_t *)(MMIO_BASE + 0x215000))
#define PTR_AUX_ENABLES ((volatile uint32_t *)(MMIO_BASE + 0x215004))
#define PTR_AUX_MU_IO_REG ((volatile uint32_t *)(MMIO_BASE + 0x215040))
#define PTR_AUX_MU_IER_REG ((volatile uint32_t *)(MMIO_BASE + 0x215044))
#define PTR_AUX_MU_IIR_REG ((volatile uint32_t *)(MMIO_BASE + 0x215048))
#define PTR_AUX_MU_LCR_REG ((volatile uint32_t *)(MMIO_BASE + 0x21504C))
#define PTR_AUX_MU_MCR_REG ((volatile uint32_t *)(MMIO_BASE + 0x215050))
#define PTR_AUX_MU_LSR_REG ((volatile uint32_t *)(MMIO_BASE + 0x215054))
#define PTR_AUX_MU_MSR_REG ((volatile uint32_t *)(MMIO_BASE + 0x215058))
#define PTR_AUX_MU_SCRATCH ((volatile uint32_t *)(MMIO_BASE + 0x21505C))
#define PTR_AUX_MU_CNTL_REG ((volatile uint32_t *)(MMIO_BASE + 0x215060))
#define PTR_AUX_MU_STAT_REG ((volatile uint32_t *)(MMIO_BASE + 0x215064))
#define PTR_AUX_MU_BAUD_REG ((volatile uint32_t *)(MMIO_BASE + 0x215068))
#define PTR_AUX_SPI0_CNTL0_REG ((volatile uint32_t *)(MMIO_BASE + 0x215080))
#define PTR_AUX_SPI0_CNTL1_REG ((volatile uint32_t *)(MMIO_BASE + 0x215084))
#define PTR_AUX_SPI0_STAT_REG ((volatile uint32_t *)(MMIO_BASE + 0x215088))
#define PTR_AUX_SPI0_IO_REG ((volatile uint32_t *)(MMIO_BASE + 0x215090))
#define PTR_AUX_SPI0_PEEK_REG ((volatile uint32_t *)(MMIO_BASE + 0x215094))
#define PTR_AUX_SPI1_CNTL0_REG ((volatile uint32_t *)(MMIO_BASE + 0x2150C0))
#define PTR_AUX_SPI1_CNTL1_REG ((volatile uint32_t *)(MMIO_BASE + 0x2150C4))
#define PTR_AUX_SPI1_STAT_REG ((volatile uint32_t *)(MMIO_BASE + 0x2150C8))
#define PTR_AUX_SPI1_IO_REG ((volatile uint32_t *)(MMIO_BASE + 0x2150D0))
#define PTR_AUX_SPI1_PEEK_REG ((volatile uint32_t *)(MMIO_BASE + 0x2150D4))

#define ADD_AUX_IRQ MMIO_BASE + 0x215000
#define ADD_AUX_ENABLES MMIO_BASE + 0x215004
#define ADD_AUX_MU_IO_REG MMIO_BASE + 0x215040
#define ADD_AUX_MU_IER_REG MMIO_BASE + 0x215044
#define ADD_AUX_MU_IIR_REG MMIO_BASE + 0x215048
#define ADD_AUX_MU_LCR_REG MMIO_BASE + 0x21504C
#define ADD_AUX_MU_MCR_REG MMIO_BASE + 0x215050
#define ADD_AUX_MU_LSR_REG MMIO_BASE + 0x215054
#define ADD_AUX_MU_MSR_REG MMIO_BASE + 0x215058
#define ADD_AUX_MU_SCRATCH MMIO_BASE + 0x21505C
#define ADD_AUX_MU_CNTL_REG MMIO_BASE + 0x215060
#define ADD_AUX_MU_STAT_REG MMIO_BASE + 0x215064
#define ADD_AUX_MU_BAUD_REG MMIO_BASE + 0x215068
#define ADD_AUX_SPI0_CNTL0_REG MMIO_BASE + 0x215080
#define ADD_AUX_SPI0_CNTL1_REG MMIO_BASE + 0x215084
#define ADD_AUX_SPI0_STAT_REG MMIO_BASE + 0x215088
#define ADD_AUX_SPI0_IO_REG MMIO_BASE + 0x215090
#define ADD_AUX_SPI0_PEEK_REG MMIO_BASE + 0x215094
#define ADD_AUX_SPI1_CNTL0_REG MMIO_BASE + 0x2150C0
#define ADD_AUX_SPI1_CNTL1_REG MMIO_BASE + 0x2150C4
#define ADD_AUX_SPI1_STAT_REG MMIO_BASE + 0x2150C8
#define ADD_AUX_SPI1_IO_REG MMIO_BASE + 0x2150D0
#define ADD_AUX_SPI1_PEEK_REG MMIO_BASE + 0x2150D4
// End AUX registers

// Start GPIO registers
#define ADD_GPFSEL MMIO_BASE + 0x200000
#define PTR_GPFSEL0 ((volatile uint32_t *)(MMIO_BASE + 0x200000))
#define PTR_GPFSEL1 ((volatile uint32_t *)(MMIO_BASE + 0x200004))
#define PTR_GPFSEL2 ((volatile uint32_t *)(MMIO_BASE + 0x200008))
#define PTR_GPFSEL3 ((volatile uint32_t *)(MMIO_BASE + 0x20000C))
#define PTR_GPFSEL4 ((volatile uint32_t *)(MMIO_BASE + 0x200010))
#define PTR_GPFSEL5 ((volatile uint32_t *)(MMIO_BASE + 0x200014))

#define ADD_GPSET MMIO_BASE + 0x20001C
#define PTR_GPSET0 ((volatile uint32_t *)(MMIO_BASE + 0x20001C))
#define PTR_GPSET1 ((volatile uint32_t *)(MMIO_BASE + 0x200020))

#define ADD_GPCLR MMIO_BASE + 0x200028
#define PTR_GPCLR0 ((volatile uint32_t *)(MMIO_BASE + 0x200028))
#define PTR_GPCLR1 ((volatile uint32_t *)(MMIO_BASE + 0x20002C))

#define ADD_GPLEV MMIO_BASE + 0x200034
#define PTR_GPLEV0 ((volatile uint32_t *)(MMIO_BASE + 0x200034))
#define PTR_GPLEV1 ((volatile uint32_t *)(MMIO_BASE + 0x200038))

#define ADD_GPEDS MMIO_BASE + 0x200040
#define PTR_GPEDS0 ((volatile uint32_t *)(MMIO_BASE + 0x200040))
#define PTR_GPEDS1 ((volatile uint32_t *)(MMIO_BASE + 0x200044))

#define ADD_GPREN MMIO_BASE + 0x20004C
#define PTR_GPREN0 ((volatile uint32_t *)(MMIO_BASE + 0x20004C))
#define PTR_GPREN1 ((volatile uint32_t *)(MMIO_BASE + 0x200050))

#define ADD_GPFEN MMIO_BASE + 0x200058
#define PTR_GPFEN0 ((volatile uint32_t *)(MMIO_BASE + 0x200058))
#define PTR_GPFEN1 ((volatile uint32_t *)(MMIO_BASE + 0x20005C))

#define ADD_GPHEN MMIO_BASE + 0x200064
#define PTR_GPHEN0 ((volatile uint32_t *)(MMIO_BASE + 0x200064))
#define PTR_GPHEN1 ((volatile uint32_t *)(MMIO_BASE + 0x200068))

#define ADD_GPLEN MMIO_BASE + 0x200070
#define PTR_GPLEN0 ((volatile uint32_t *)(MMIO_BASE + 0x200070))
#define PTR_GPLEN1 ((volatile uint32_t *)(MMIO_BASE + 0x200074))

#define ADD_GPAREN MMIO_BASE + 0x20007C
#define PTR_GPAREN0 ((volatile uint32_t *)(MMIO_BASE + 0x20007C))
#define PTR_GPAREN1 ((volatile uint32_t *)(MMIO_BASE + 0x200080))

#define ADD_GPAFEN MMIO_BASE + 0x200088
#define PTR_GPAFEN0 ((volatile uint32_t *)(MMIO_BASE + 0x200088))
#define PTR_GPAFEN1 ((volatile uint32_t *)(MMIO_BASE + 0x20008C))

#define ADD_GPPUD MMIO_BASE + 0x200094
#define PTR_GPPUD ((volatile uint32_t *)(MMIO_BASE + 0x200094))

#define ADD_GPPUDCLK MMIO_BASE + 0x200098
#define PTR_GPPUDCLK0 ((volatile uint32_t *)(MMIO_BASE + 0x200098))
#define PTR_GPPUDCLK1 ((volatile uint32_t *)(MMIO_BASE + 0x20009C))
#define GPIO_INPUT 0
#define GPIO_OUTPUT 1
#define GPIO_ALT0 4
#define GPIO_ALT1 5
#define GPIO_ALT2 6
#define GPIO_ALT3 7
#define GPIO_ALT4 3
#define GPIO_ALT5 2
// End GPIO registers

void PUT32(uint32_t addr, uint32_t value);
void put32(uint32_t *addr, uint32_t value);
uint32_t GET32(uint32_t addr);
uint32_t get32(uint32_t *addr);

#endif