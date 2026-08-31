#include "device.h"

int serialize_device(device_t *device, uint8_t *buffer, size_t buffer_size)
{
    if (buffer_size < sizeof(device_t))
    {
        return -1; // Buffer too small
    }
    memcpy(buffer, device, sizeof(device_t));
    return sizeof(device_t);
}

int serialize_device_list(device_list_t *device_list, uint8_t *buffer, size_t buffer_size)
{
    size_t offset_bytes = 0;
    struct list_head *pos;
    device_node_t *node;

    if (buffer_size < sizeof(uint32_t))
        return -1;

    offset_bytes += sizeof(uint32_t); // Reserve space for the count of devices

    uint32_t device_count = 0;

    for (pos = device_list->head.next;
         pos != &device_list->head;
         pos = pos->next)
    {
        if (offset_bytes + sizeof(device_t) > buffer_size)
            return -1;
        node = container_of(pos, device_node_t, list);
        memcpy(buffer + offset_bytes, &node->device, sizeof(device_t));
        offset_bytes += sizeof(device_t);
        device_count++;
    }
    memcpy(buffer, &device_count, sizeof(uint32_t)); // Write the count of devices at the beginning
    return offset_bytes;                             // Return the number of bytes written
}

int deserialize_device(device_t *device, uint8_t *buffer, size_t buffer_size)
{
    if (buffer_size < sizeof(device_t))
    {
        return -1; // Buffer too small
    }
    memcpy(device, buffer, sizeof(device_t));
    return sizeof(device_t);
}

int deserialize_device_list(device_list_t *device_list, uint8_t *buffer, size_t buffer_size)
{
    size_t offset_bytes = 0;
    struct list_head *pos;
    device_node_t *node;

    if (buffer_size < sizeof(uint32_t))
        return -1;

    uint32_t device_count;
    memcpy(&device_count, buffer + offset_bytes, sizeof(uint32_t));
    offset_bytes += sizeof(uint32_t);

    for (uint32_t i = 0; i < device_count; i++)
    {
        if (offset_bytes + sizeof(device_t) > buffer_size)
            return -1;
        node = malloc(sizeof(device_node_t));
        if (!node)
            return -1; // Memory allocation failed
        memcpy(&node->device, buffer + offset_bytes, sizeof(device_t));
        offset_bytes += sizeof(device_t);
        list_add_tail(&node->list, &device_list->head);
    }
    return offset_bytes; // Return the number of bytes read
}