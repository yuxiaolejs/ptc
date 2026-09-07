#ifndef DMX_H
#define DMX_H

#include <stdint.h>

#define GPFSEL2 0x20200008u
#define GPSET0 0x2020001Cu
#define GPCLR0 0x20200028u
#define SYS_CLO 0x20003004u

#define DMX_PIN 21
#define DMX_MASK (1u << DMX_PIN)

#define DMX_USE_DMA 1

#define DMX_BIT_US 4
#define DMX_BREAK_CELLS 44
#define DMX_MAB_CELLS 3
#define DMX_SLOTS 513
#define DMX_MTBF_US 100

#define DMX_ALL_MAX 0

extern uint8_t dmx_frame_buffer[DMX_SLOTS + 400];
extern uint32_t dmx_fb_write_offset;

void dmx_gpio_init(void);
void write_fb_bit(uint8_t bit);
uint32_t dmx_render_frame(uint8_t *frame);

#endif