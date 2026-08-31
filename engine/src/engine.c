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