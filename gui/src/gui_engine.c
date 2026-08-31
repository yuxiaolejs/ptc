#include "gui_engine.h"

void gui_render_queue_table(cue_list_t *cue_list, uint32_t current_cue_number)
{
    int y_offset = 10; // Starting y position for the first cue
    struct list_head *pos;
    cue_node_t *node;

    for (pos = cue_list->head.next; pos != &cue_list->head; pos = pos->next)
    {
        node = container_of(pos, cue_node_t, list);
        char cue_str[32];
        snprintf(cue_str, sizeof(cue_str), "Cue %u: Duration %u", node->cue.cue_number, node->cue.duration);
        if (node->cue.cue_number == current_cue_number)
        {
            // Highlight the current cue
            gui_draw_string(cue_str, 10, y_offset, 0xFFFF00); // Yellow color
        }
        else
        {
            gui_draw_string(cue_str, 10, y_offset, 0xFFFFFF); // White color
        }
        y_offset += 20; // Move down for the next cue
    }
}

void gui_render_cmdline(show_engine_t *engine)
{
    gui_draw_string(engine->cmdline_display, 10, 580, 0xFFFFFF); // White color
    gui_draw_string_align_right(engine->cmdline_tips, 1270, 580, 0xFF0000); // Red color for tips
}

void gui_render_engine_state(show_engine_t *engine)
{
    gui_cls(0x000000); // Clear the screen with black

    // Draw current cue number
    char cue_str[32];
    snprintf(cue_str, sizeof(cue_str), "Cue: %u", engine->current_cue_number);
    gui_draw_string(cue_str, 600, 10, 0xFFFFFF); // White color

    // Draw DMX values for the first 10 channels
    for (int i = 0; i < 10; i++)
    {
        char dmx_str[32];
        snprintf(dmx_str, sizeof(dmx_str), "DMX[%d]: %u", i + 1, engine->dmx_val[i]);
        gui_draw_string(dmx_str, 600, 30 + i * 20, 0xFFFFFF); // White color
    }

    // Draw the cue list
    gui_render_queue_table(&engine->cue_list_head, engine->current_cue_number);
    gui_render_cmdline(engine);
}