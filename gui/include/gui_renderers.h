#ifndef GUI_RENDERERS_H
#define GUI_RENDERERS_H
#include <stdint.h>
void gui_draw_char(char c, int x, int y, uint32_t color_code);
void gui_draw_rect(int x, int y, int width, int height, uint32_t color_code);
void gui_cls(uint32_t color_code);
void gui_draw_string(const char *str, int x, int y, uint32_t color_code);
void gui_draw_string_align_right(const char *str, int x, int y, uint32_t color_code);
#endif