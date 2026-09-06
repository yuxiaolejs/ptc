#include "types.h"

#include <stdint.h>
#include "pcm.h"

#define GPFSEL2 0x20200008u 
#define GPSET0 0x2020001Cu  
#define GPCLR0 0x20200028u  
#define SYS_CLO 0x20003004u 

#define DMX_PIN 21
#define DMX_MASK (1u << DMX_PIN)

#define USE_DMA 1

#define BIT_US 4
#define BREAK_CELLS 44
#define MAB_CELLS 3
#define DMX_SLOTS 513 
#define MTBF_US 100   

#define DMX_ALL_MAX 0

static inline void PUT32(uint32_t addr, uint32_t val)
{
    *(volatile uint32_t *)addr = val;
}

static inline uint32_t GET32(uint32_t addr)
{
    return *(volatile uint32_t *)addr;
}

static inline void wait_until(uint32_t deadline)
{
    while ((int32_t)(GET32(SYS_CLO) - deadline) < 0)
        ;
}

static void delay_us(uint32_t us)
{
    wait_until(GET32(SYS_CLO) + us);
}

static uint8_t frame[DMX_SLOTS];

static void dmx_build_frame(void)
{
    int i;
    uint32_t tm = GET32(SYS_CLO) >> 17;

    frame[0] = 0x00; 
    frame[1] = tm % 256;    
    frame[2] = 0; 
    frame[3] = (tm) % 256; 
    frame[4] = 0x00; 
    frame[5] = (tm) % 140;    
    frame[6] = 10;    
    frame[7] = 0;    
    frame[8] = 255;  

#if DMX_ALL_MAX
    for (i = 1; i < DMX_SLOTS; i++)
        frame[i] = 255;
#else
    for (i = 9; i < DMX_SLOTS; i++) 
        frame[i] = 0;
#endif
}

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

static void dmx_render_frame(void)
{
    dmx_fb_write_offset = 0;
    for (uint16_t b = 0; b < BREAK_CELLS; b++)
        write_fb_bit(0);
    for (uint16_t b = 0; b < MAB_CELLS; b++)
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
}
static void dmx_send_frame(void)
{
    uint32_t d;
    int s, i;

    asm volatile("cpsid i" ::: "memory");

    d = GET32(SYS_CLO);

    PUT32(GPCLR0, DMX_MASK); 
    d += BREAK_CELLS * BIT_US;
    wait_until(d);

    PUT32(GPSET0, DMX_MASK); 
    d += MAB_CELLS * BIT_US;
    wait_until(d);

    for (s = 0; s < DMX_SLOTS; s++)
    {
        uint32_t b = frame[s];

        PUT32(GPCLR0, DMX_MASK); 
        d += BIT_US;
        wait_until(d);

        for (i = 0; i < 8; i++)
        { 
            PUT32((b & 1u) ? GPSET0 : GPCLR0, DMX_MASK);
            b >>= 1;
            d += BIT_US;
            wait_until(d);
        }

        PUT32(GPSET0, DMX_MASK); 
        d += 2 * BIT_US;
        wait_until(d);
    }

    d += MTBF_US;
    wait_until(d);

    asm volatile("cpsie i" ::: "memory");
}

static void dmx_gpio_init(void)
{
    uint32_t v = GET32(GPFSEL2);

    v &= ~(7u << 3);
    v |= (1u << 3); 
    PUT32(GPFSEL2, v);
}

void notmain(void)
{
    uint32_t frames = 0;

    dmx_gpio_init();

    PUT32(GPSET0, DMX_MASK);
    if (!USE_DMA)
        pcm_dmx_init();
    else
        pcm_clock_init();
    delay_us(1000);

    dmx_build_frame();
    dmx_render_frame();

    uint32_t bytes = dmx_fb_write_offset / 8;
    uint32_t words = bytes / 4; 

    if (!USE_DMA)
        while (1)
        {

            uint32_t t0 = GET32(SYS_CLO);
            uint32_t i;

            printk("xfer\n");
            if (USE_DMA)
                pcm_dma_start(dmx_frame_buffer, bytes);
            else
                for (i = 0; i < words; i++)
                    pcm_write_word(((const uint32_t *)dmx_frame_buffer)[i]);

            uint32_t dt = GET32(SYS_CLO) - t0;

            if (!USE_DMA)
                pcm_fill_mark();

            if (frames++ % 44 == 0)
                printk("frame %u us  %u bytes  bits/byte x100 = %u  underrun %u\n",
                       dt, bytes, (dt * 25u) / bytes, pcm_underran());
        }
    else
        pcm_dma_start(dmx_frame_buffer, bytes);
    while (1)
    {
        delay_us(10000);
        dmx_build_frame();
        dmx_render_frame();
    }
}

void render_put_pixel(int x, int y, uint32_t color)
{
}
