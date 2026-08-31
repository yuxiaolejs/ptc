#ifndef ENGINE_H
#define ENGINE_H

#include "cue.h"
#include "device.h"

typedef struct show_engine
{
    uint32_t current_cue_number; // Current cue number
    uint8_t dmx_val[513];        // DMX values for all channels (1-512)

    cue_list_t cue_list_head;       // Head of the cue list
    device_list_t device_list_head; // Head of the device list
} show_engine_t;

int init_show_engine(show_engine_t *engine);
int serialize_show_engine(show_engine_t *engine, uint8_t *buffer, size_t buffer_size);
int deserialize_show_engine(show_engine_t *engine, uint8_t *buffer, size_t buffer_size);

#endif