#include "cue.h"

int init_cue_list(cue_list_t *cue_list, const char *name)
{
    strncpy(cue_list->name, name, sizeof(cue_list->name) - 1);
    cue_list->name[sizeof(cue_list->name) - 1] = '\0'; // Ensure null-termination
    INIT_LIST_HEAD(&cue_list->head);
    return 0;
}

int serialize_cue(cue_t *cue, uint8_t *buffer, size_t buffer_size)
{
    size_t offset_bytes = 0;
    struct list_head *pos;
    transition_node_t *node;

    memcpy(buffer + offset_bytes, &cue->cue_number, sizeof(cue->cue_number));
    offset_bytes += sizeof(cue->cue_number);
    memcpy(buffer + offset_bytes, &cue->duration, sizeof(cue->duration));
    offset_bytes += sizeof(cue->duration);

    if (buffer_size + offset_bytes < sizeof(uint32_t))
        return -1;
    uint32_t count_offset = offset_bytes;
    offset_bytes += sizeof(uint32_t); // Reserve space for the count of devices

    uint32_t transition_count = 0;

    for (pos = cue->head.next;
         pos != &cue->head;
         pos = pos->next)
    {
        if (offset_bytes + sizeof(transition_t) > buffer_size)
            return -1;
        node = container_of(pos, transition_node_t, list);
        memcpy(buffer + offset_bytes, &node->transition, sizeof(transition_t));
        offset_bytes += sizeof(transition_t);
        transition_count++;
    }
    memcpy(buffer + count_offset, &transition_count, sizeof(uint32_t)); // Write the count of transitions at the reserved space
    return offset_bytes;                                                // Return the number of bytes written
}
int serialize_cue_list(cue_list_t *cue_list, uint8_t *buffer, size_t buffer_size)
{
    size_t offset_bytes = 0;
    struct list_head *pos;
    cue_t *cue;

    memcpy(buffer + offset_bytes, cue_list->name, sizeof(cue_list->name));
    offset_bytes += sizeof(cue_list->name);

    if (buffer_size + offset_bytes < sizeof(uint32_t))
        return -1;

    uint32_t count_offset = offset_bytes;
    offset_bytes += sizeof(uint32_t); // Reserve space for the count of cues

    uint32_t cue_count = 0;

    for (pos = cue_list->head.next;
         pos != &cue_list->head;
         pos = pos->next)
    {
        if (offset_bytes + sizeof(cue_t) > buffer_size)
            return -1;
        cue = &container_of(pos, cue_node_t, list)->cue;
        int bytes_written = serialize_cue(cue, buffer + offset_bytes, buffer_size - offset_bytes);
        if (bytes_written < 0)
            return -1; // Error during serialization
        offset_bytes += bytes_written;
        cue_count++;
    }
    memcpy(buffer + count_offset, &cue_count, sizeof(uint32_t)); // Write the count of cues at the beginning
    return offset_bytes;                          // Return the number of bytes written
}

int deserialize_cue(cue_t *cue, uint8_t *buffer, size_t buffer_size)
{
    size_t offset_bytes = 0;
    struct list_head *pos;
    transition_node_t *node;

    memcpy(&cue->cue_number, buffer + offset_bytes, sizeof(cue->cue_number));
    offset_bytes += sizeof(cue->cue_number);
    memcpy(&cue->duration, buffer + offset_bytes, sizeof(cue->duration));
    offset_bytes += sizeof(cue->duration);

    if (buffer_size + offset_bytes < sizeof(uint32_t))
        return -1;

    uint32_t transition_count;
    memcpy(&transition_count, buffer + offset_bytes, sizeof(uint32_t));
    offset_bytes += sizeof(uint32_t);

    for (uint32_t i = 0; i < transition_count; i++)
    {
        if (offset_bytes + sizeof(transition_t) > buffer_size)
            return -1;
        node = malloc(sizeof(transition_node_t));
        if (!node)
            return -1; // Memory allocation failed
        memcpy(&node->transition, buffer + offset_bytes, sizeof(transition_t));
        offset_bytes += sizeof(transition_t);
        list_add_tail(&node->list, &cue->head);
    }
    return offset_bytes; // Return the number of bytes read
}
int deserialize_cue_list(cue_list_t *cue_list, uint8_t *buffer, size_t buffer_size)
{
    size_t offset_bytes = 0;
    struct list_head *pos;
    cue_node_t *node;

    memcpy(cue_list->name, buffer + offset_bytes, sizeof(cue_list->name));
    offset_bytes += sizeof(cue_list->name);

    if (buffer_size + offset_bytes < sizeof(uint32_t))
        return -1;

    uint32_t cue_count;
    memcpy(&cue_count, buffer + offset_bytes, sizeof(uint32_t));
    offset_bytes += sizeof(uint32_t);

    for (uint32_t i = 0; i < cue_count; i++)
    {
        if (offset_bytes + sizeof(cue_t) > buffer_size)
            return -1;
        node = malloc(sizeof(cue_node_t));
        if (!node)
            return -1; // Memory allocation failed
        INIT_LIST_HEAD(&node->cue.head);
        int bytes_read = deserialize_cue(&node->cue, buffer + offset_bytes, buffer_size - offset_bytes);
        if (bytes_read < 0)
            return -1; // Error during deserialization
        offset_bytes += bytes_read;
        list_add_tail(&node->list, &cue_list->head);
    }
    return offset_bytes; // Return the number of bytes read
}