#ifndef ENGINE_H
#define ENGINE_H

#include "cue.h"
#include "device.h"
#include "live.h"

typedef struct show_engine
{
    uint32_t current_cue_number; // Current cue number
    uint8_t dmx_val[513];        // DMX values for all channels (1-512)

    cue_list_t cue_list_head;       // Head of the cue list
    device_list_t device_list_head; // Head of the device list

    // following are local execution state, not serialized
    char cmdline_buffer[256];   // Buffer for command line input
    char cmdline_display[1024]; // Echo back of cmdline input, computed to human-readable
    char cmdline_tips[1024];    // Tips for early error detection, computed to human-readable, todo

    uint32_t current_tick;
    live_transition_list_t active_transitions_head; // Head of the active transitions list
} show_engine_t;

int init_show_engine(show_engine_t *engine);
int serialize_show_engine(show_engine_t *engine, uint8_t *buffer, size_t buffer_size);
int deserialize_show_engine(show_engine_t *engine, uint8_t *buffer, size_t buffer_size);

void engine_cmdline_key(show_engine_t *engine, int keycode, int is_shifted);
void engine_tick(show_engine_t *engine);
#endif