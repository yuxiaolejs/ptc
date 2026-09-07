#include "types.h"
#include "regs.h"
#include "cstr.h"
#include "dmx.h"

__attribute__((aligned(32))) uint8_t dmx_frame_buffer[DMX_SLOTS + 400];
uint32_t dmx_fb_write_offset = 0;

void write_fb_bit(uint8_t bit)
{
    uint32_t byte_idx = dmx_fb_write_offset / 8;
    uint32_t bit_idx = dmx_fb_write_offset % 8;
    uint32_t arr_idx = (byte_idx & ~3u) | (3u - (byte_idx & 3u));

    // Assume MSB
    dmx_frame_buffer[arr_idx] &= ~(1 << (7 - bit_idx));
    dmx_frame_buffer[arr_idx] |= (bit << (7 - bit_idx));
    dmx_fb_write_offset++;
}

uint32_t dmx_render_frame(uint8_t *frame)
{
    dmx_fb_write_offset = 0;
    for (uint16_t b = 0; b < DMX_BREAK_CELLS; b++)
        write_fb_bit(0);
    for (uint16_t b = 0; b < DMX_MAB_CELLS; b++)
        write_fb_bit(1);
    for (int s = 0; s < DMX_SLOTS; s++)
    {
        uint32_t b = frame[s];

        write_fb_bit(0);

        for (int i = 0; i < 8; i++)
        {
            write_fb_bit(b & 1u);
            b >>= 1;
        }

        write_fb_bit(1);
        write_fb_bit(1);
    }
    printk("DMX frame rendered, %u bits\n", dmx_fb_write_offset);
    while (dmx_fb_write_offset < sizeof(dmx_frame_buffer) * 8 - 16)
        write_fb_bit(1);

    while (dmx_fb_write_offset % 32 != 0)
        write_fb_bit(1);
    return dmx_fb_write_offset;
}

void dmx_gpio_init(void)
{
    uint32_t v = GET32(GPFSEL2);

    v &= ~(7u << 3);
    v |= (1u << 3);
    PUT32(GPFSEL2, v);
    PUT32(GPSET0, DMX_MASK);
}