#include "gui_renderers.h"
#include "font.h"
#include <stdint.h>
#include "gui_config.h"

uint8_t font_rows = gui_font[0];
uint8_t font_cols = gui_font[1];

void gui_draw_char(char c, int x, int y, uint32_t color_code)
{
    const char *map = gui_font + (c - 0x20) * font_rows + 2; // skip metadata
    for (int i = 0; i < font_rows; i++)
        for (int j = 0; j < font_cols; j++)
            if (((map[i] >> ((font_cols - 1) - j)) & 1u))
                render_put_pixel(x + j, y + i, color_code);
}

void gui_draw_rect(int x, int y, int width, int height, uint32_t color_code)
{
    for (int i = 0; i < height; i++)
        for (int j = 0; j < width; j++)
            render_put_pixel(x + j, y + i, color_code);
}

void gui_cls(uint32_t color_code)
{
    gui_draw_rect(0, 0, RAW_FB_W, RAW_FB_H, color_code);
}

void gui_draw_string(const char *str, int x, int y, uint32_t color_code)
{
    while (*str)
    {
        gui_draw_char(*str, x, y, color_code);
        x += font_cols;
        str++;
    }
}

void gui_draw_string_align_right(const char *str, int x, int y, uint32_t color_code)
{
    int len = 0;
    const char *s = str;
    while (*s)
    {
        len++;
        s++;
    }
    x -= len * font_cols;
    while (*str)
    {
        gui_draw_char(*str, x, y, color_code);
        x += font_cols;
        str++;
    }
}