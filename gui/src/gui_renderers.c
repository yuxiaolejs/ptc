#include "gui_renderers.h"
#include "font.h"
#include <stdint.h>

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