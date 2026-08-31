#include "engine.h"
#include "cue.h"
#include "device.h"

int init_show_engine(show_engine_t *engine)
{
    engine->current_cue_number = 0;
    memset(engine->dmx_val, 0, sizeof(engine->dmx_val));
    INIT_LIST_HEAD(&engine->cue_list_head.head);
    INIT_LIST_HEAD(&engine->device_list_head.head);
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