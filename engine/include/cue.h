#ifndef CUE_H
#define CUE_H

#include "device.h"
typedef struct transition
{
    uint32_t dmx_property;
    uint32_t dmx_value;
} transition_t;

typedef struct transition_node
{
    struct list_head list; // Linked list node
    transition_t transition; // Transition data
} transition_node_t;

typedef struct cue
{
    uint32_t cue_number;     // Cue number
    struct list_head head; // Transition list
    uint32_t duration;      // Duration of the cue in milliseconds
} cue_t;

typedef struct cue_node
{
    struct list_head list; // Linked list node
    cue_t cue;             // Cue data
} cue_node_t;

typedef struct cue_list
{
    char name[32];           // Name of the cue list
    struct list_head head; // Head of the linked list
} cue_list_t;
#endif