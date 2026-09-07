#include "engine.h"
#include "cue.h"
#include "device.h"
#include "pictl.h"

int init_show_engine(show_engine_t *engine)
{
    engine->current_cue_number = 0;
    memset(engine->dmx_val, 0, sizeof(engine->dmx_val));
    INIT_LIST_HEAD(&engine->cue_list_head.head);
    INIT_LIST_HEAD(&engine->device_list_head.head);
    INIT_LIST_HEAD(&engine->active_transitions_head.head);
    return 0;
}

int serialize_show_engine(show_engine_t *engine, uint8_t *buffer, size_t buffer_size)
{
    uint32_t offset_bytes = 0;
    memcpy(buffer + offset_bytes, &engine->current_cue_number, sizeof(engine->current_cue_number));
    offset_bytes += sizeof(engine->current_cue_number);
    memcpy(buffer + offset_bytes, engine->dmx_val, sizeof(engine->dmx_val));
    offset_bytes += sizeof(engine->dmx_val);

    offset_bytes += serialize_cue_list(&engine->cue_list_head, buffer + offset_bytes, buffer_size - offset_bytes);
    offset_bytes += serialize_device_list(&engine->device_list_head, buffer + offset_bytes, buffer_size - offset_bytes);
    printf("Serialized show engine: %u bytes\n", offset_bytes);
    return offset_bytes;
}

int deserialize_show_engine(show_engine_t *engine, uint8_t *buffer, size_t buffer_size)
{
    uint32_t offset_bytes = 0;
    memcpy(&engine->current_cue_number, buffer + offset_bytes, sizeof(engine->current_cue_number));
    offset_bytes += sizeof(engine->current_cue_number);
    memcpy(engine->dmx_val, buffer + offset_bytes, sizeof(engine->dmx_val));
    offset_bytes += sizeof(engine->dmx_val);

    INIT_LIST_HEAD(&engine->cue_list_head.head);
    INIT_LIST_HEAD(&engine->device_list_head.head);

    offset_bytes += deserialize_cue_list(&engine->cue_list_head, buffer + offset_bytes, buffer_size - offset_bytes);
    offset_bytes += deserialize_device_list(&engine->device_list_head, buffer + offset_bytes, buffer_size - offset_bytes);
    printf("Deserialized show engine: %u bytes\n", offset_bytes);
    return offset_bytes;
}

void engine_update_cmdline_display(show_engine_t *engine)
{
    // run the parsing logic for viz
    char *input = engine->cmdline_buffer;
    char *display = engine->cmdline_display;
    char *tips = engine->cmdline_tips;
    engine->cmdline_tips[0] = '?';
    engine->cmdline_tips[1] = '\0';
    while (*input)
    {
        switch (*input)
        {
        case 'q':
            sprintf(display, "Cue ");
            display += strlen(display);
            break;
        case 'g':
            sprintf(display, "Go To Cue ");
            display += strlen(display);
            break;
        case 'u':
            sprintf(display, "Update ");
            display += strlen(display);
            break;
        case 'r':
            sprintf(display, "Record ");
            display += strlen(display);
            break;
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '0':
        case '9':
            sprintf(display, "%c", *input);
            display += strlen(display);
            break;
        }
        input++;
    }
}

void engine_cmdline_key(show_engine_t *engine, int keycode, int is_shifted)
{
    printf("engine_cmdline_key: keycode=%d, is_shifted=%d\n", keycode, is_shifted);
    if (keycode == 13)
    {
        // Enter = submit command
        printf("Command submitted: %s\n", engine->cmdline_buffer);
        printf("Parsed command submitted: %s\n", engine->cmdline_display);
        engine->cmdline_buffer[0] = '\0';
        engine->cmdline_display[0] = '\0';
        engine->cmdline_tips[0] = '\0';
    }
    else
    {
        char c = (char)keycode;
        uint32_t idx = strlen(engine->cmdline_buffer);
        engine->cmdline_buffer[idx] = c;
        engine->cmdline_buffer[idx + 1] = '\0';
    }
    engine_update_cmdline_display(engine);
}

device_t *find_device_by_index(device_list_t *device_list, uint32_t index)
{
    struct list_head *pos;
    device_node_t *node;
    uint32_t current_index = 0;

    for (pos = device_list->head.next; pos != &device_list->head; pos = pos->next)
    {
        node = container_of(pos, device_node_t, list);
        if (current_index == index)
            return &node->device;
        current_index++;
    }
    return NULL; // Device not found
}

void engine_tick(show_engine_t *engine)
{
    // A tick taken by the engine.
    uint32_t tick = timer_get_msec();
    if (tick == engine->current_tick)
        return;
    printk("processing engine tick: %u -> %u\n", engine->current_tick, tick);
    engine->current_tick = tick;
    // Engine state update: loop active_transitions_head, and update accordingly
    for (struct list_head *pos = engine->active_transitions_head.head.next; pos != &engine->active_transitions_head.head;)
    {
        live_transition_node_t *node = container_of(pos, live_transition_node_t, list);
        pos = pos->next; // Move to next before potentially removing the current node

        if (engine->current_tick >= node->live_transition.end_tick)
        {
            // Transition has ended, apply final state and remove from active transitions
            transition_t *transition = node->live_transition.transition;
            device_t *device = find_device_by_index(&engine->device_list_head, transition->device_id);
            if (!device)
            {
                printk("Warning: skipping transition for non-existent device_id %u\n", transition->device_id);
                continue;
            }
            uint32_t dmx_chann = device->dmx_chan[transition->dmx_property];
            uint32_t dmx_value = transition->dmx_value;
            device->dmx_val[transition->dmx_property] = dmx_value;
            // Apply final state logic here (not implemented)
            list_del(&node->list);
            kfree(node);
        }
        else
        {
            // Transition is still active, apply intermediate state
            transition_t *transition = node->live_transition.transition;
            uint32_t total_duration = node->live_transition.end_tick - node->live_transition.start_tick;
            uint32_t elapsed_time = engine->current_tick - node->live_transition.start_tick;
            // progress normalized to 8 bits
            uint32_t progress = (elapsed_time * 255) / total_duration;
            device_t *device = find_device_by_index(&engine->device_list_head, transition->device_id);
            if (!device)
            {
                printk("Warning: skipping transition for non-existent device_id %u\n", transition->device_id);
                continue;
            }
            uint32_t dmx_chann = device->dmx_chan[transition->dmx_property];
            uint32_t dmx_value = node->live_transition.start_dmx_value * (255 - progress) / 255 + transition->dmx_value * progress / 255;
            device->dmx_val[transition->dmx_property] = dmx_value;
        }
    }
    // done with transition application, now update the engine's dmx_val array - loop over devices
    for (struct list_head *pos = engine->device_list_head.head.next; pos != &engine->device_list_head.head; pos = pos->next)
    {
        device_node_t *node = container_of(pos, device_node_t, list);
        device_t *device = &node->device;
        for (uint32_t prop = 0; prop < PROPERTY_COUNT; prop++)
        {
            uint32_t dmx_chann = device->dmx_chan[prop];
            if (dmx_chann > 0 && dmx_chann <= 512)
            {
                engine->dmx_val[dmx_chann + device->dmx_offset] = device->dmx_val[prop];
            }
        }
    }
    // engine internal state update ok. maybe also GUI-related states? for later!
}