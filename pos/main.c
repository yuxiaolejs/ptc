#include "types.h"

#include <stdint.h>
#include "regs.h"
#include "cstr.h"
#include "pictl.h"
#include "pcm.h"
#include "dmx.h"
#include "engine.h"

static void dmx_build_frame(uint8_t *frame)
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

    frame[9] = 0x00;
    frame[10] = 0x00;
    frame[11] = 0x00;
    frame[12] = 255 - tm % 256;
    frame[13] = 0;
    frame[14] = (tm) % 256;
    frame[15] = 0x00;
    frame[16] = (tm) % 140;
    frame[17] = 10;
    frame[18] = 0;
    frame[19] = 255;
    for (i = 20; i < DMX_SLOTS; i++)
        frame[i] = 0;
}

show_engine_t *main_engine;

void notmain(void)
{
    uint32_t frames = 0;
    uint8_t dmx_frame[DMX_SLOTS];

    kalloc_init();

    dmx_gpio_init();
    pcm_clock_init();
    delay_us(1000);
    printk("READY TO BUILD DMX FRAME\n");
    dmx_build_frame(dmx_frame);
    uint32_t dmx_bits = dmx_render_frame(dmx_frame);

    uint32_t bytes = dmx_bits / 8;
    uint32_t words = bytes / 4;

    pcm_dma_start(dmx_frame_buffer, bytes);

    // engine init
    main_engine = (show_engine_t *)malloc(sizeof(show_engine_t));
    memset(main_engine, 0, sizeof(show_engine_t));
    init_show_engine(main_engine);

    while (1)
    {
        delay_us(10000);
        // engine render loop
        dmx_fb_write_offset = dmx_render_frame(main_engine->dmx_val);
    }
}

void render_put_pixel(int x, int y, uint32_t color)
{
}
