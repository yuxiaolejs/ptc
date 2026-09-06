#include <stdint.h>

#include "fb.h"
#include <stdarg.h>
#include "serial.h"
#include "mem.h"

#define PERIPH_BASE 0x20000000
#define MBOX_BASE (PERIPH_BASE + 0xB880)

#define MBOX_READ (*(volatile uint32_t *)(MBOX_BASE + 0x00))
#define MBOX_STATUS (*(volatile uint32_t *)(MBOX_BASE + 0x18))
#define MBOX_WRITE (*(volatile uint32_t *)(MBOX_BASE + 0x20))

#define MBOX_EMPTY 0x40000000
#define MBOX_FULL 0x80000000
#define MBOX_CH_PROP 8
#define ARM_TO_GPU_BUS(addr) ((addr) | 0x40000000)

__attribute__((aligned(16))) volatile uint32_t mbox[36];

static int mailbox_call(uint8_t channel)
{
    uint32_t addr = (uint32_t)(uintptr_t)mbox;

    // must be 16-byte aligned
    if (addr & 0xF)
    {
        return 0;
    }

    uint32_t msg = (ARM_TO_GPU_BUS(addr) & ~0xF) | (channel & 0xF);

    while (MBOX_STATUS & MBOX_FULL)
        ;
    MBOX_WRITE = msg;

    while (1)
    {
        while (MBOX_STATUS & MBOX_EMPTY)
            ;
        uint32_t r = MBOX_READ;
        if (r == msg)
            return 1;
    }
}

volatile uint32_t *fb;
uint32_t pitch;

volatile uint32_t *framebuffer_init()
{

    mbox[0] = 30 * 4;
    mbox[1] = 0;

    mbox[2] = 0x48003; // set physical size
    mbox[3] = 8;
    mbox[4] = 8;
    mbox[5] = 1280;
    mbox[6] = 720;

    mbox[7] = 0x48004; // set virtual size
    mbox[8] = 8;
    mbox[9] = 8;
    mbox[10] = 1280;
    mbox[11] = 720;

    mbox[12] = 0x48005; // set depth
    mbox[13] = 4;
    mbox[14] = 4;
    mbox[15] = 32;

    mbox[16] = 0x48006; // set pixel order
    mbox[17] = 4;
    mbox[18] = 4;
    mbox[19] = 1; // RGB

    mbox[20] = 0x40001; // allocate framebuffer
    mbox[21] = 8;
    mbox[22] = 4;
    mbox[23] = 16;
    mbox[24] = 0;

    mbox[25] = 0x40008; // get pitch
    mbox[26] = 4;
    mbox[27] = 0;
    mbox[28] = 0;

    mbox[29] = 0;

    mailbox_call(MBOX_CH_PROP);

    fb = (volatile uint32_t *)(mbox[23] & 0x3FFFFFFF);
    pitch = mbox[28];
    return fb;
}

void framebuffer_free(void)
{
    mbox[0] = 8 * 4;
    mbox[1] = 0;

    mbox[2] = 0x48001; // release framebuffer
    mbox[3] = 0;
    mbox[4] = 0;

    mbox[5] = 0; // end tag

    // padding to 16-byte align (already fine at 8 words but just in case)
    mbox[6] = 0;
    mbox[7] = 0;

    mailbox_call(MBOX_CH_PROP);

    fb = NULL;
    pitch = 0;
}
