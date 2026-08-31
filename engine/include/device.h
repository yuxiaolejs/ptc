#ifndef DEVICE_H
#define DEVICE_H
#include <stdint.h>
#include "linkedlist.h"
typedef enum
{
    DIMMER = 0,
    PAN,
    TILT,
    COLOR,
    GOBOS,
    FOCUS,
    SHUTTER,
    APERTURE,
    TEMPERATURE,
    C_RED,
    C_GREEN,
    C_BLUE,
    C_WHITE,
    PROPERTY_COUNT
} device_property_t;

typedef struct device
{
    uint32_t dmx_offset;               // DMX address offset for the device
    uint32_t dmx_chan_count;           // Number of DMX channels used by the device
    uint32_t dmx_chan[PROPERTY_COUNT]; // DMX channel mapping for each property
    uint8_t dmx_val[PROPERTY_COUNT];   // Current DMX value for each property
} device_t;

typedef struct device_node
{
    struct list_head list; // Linked list node
    device_t device;       // Device data
} device_node_t;

typedef struct device_list
{
    struct list_head head; // Head of the linked list
} device_list_t;

#endif