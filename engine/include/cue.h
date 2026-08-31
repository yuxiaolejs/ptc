#ifndef CUE_H
#define CUE_H

#include "device.h"
typedef struct transition
{
    uint32_t device_id;
    uint32_t dmx_property;
    uint32_t dmx_value;
} transition_t;

typedef struct transition_node
{
    transition_t transition; // Transition data
    struct list_head list; // Linked list node
} transition_node_t;

typedef struct cue
{
    uint32_t cue_number;     // Cue number
    uint32_t duration;      // Duration of the cue in milliseconds
    struct list_head head; // Transition list
} cue_t;

typedef struct cue_node
{
    cue_t cue;             // Cue data
    struct list_head list; // Linked list node
} cue_node_t;

typedef struct cue_list
{
    char name[32];           // Name of the cue list
    struct list_head head; // Head of the linked list
} cue_list_t;

int init_cue_list(cue_list_t *cue_list, const char *name);

int serialize_cue(cue_t *cue, uint8_t *buffer, size_t buffer_size);
int serialize_cue_list(cue_list_t *cue_list, uint8_t *buffer, size_t buffer_size);

int deserialize_cue(cue_t *cue, uint8_t *buffer, size_t buffer_size);
int deserialize_cue_list(cue_list_t *cue_list, uint8_t *buffer, size_t buffer_size);
#endif