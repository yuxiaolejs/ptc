#ifndef LIVE_H
#define LIVE_H
#include "cue.h"

typedef struct live_transition
{
    transition_t * transition; // the transition to apply
    uint32_t start_tick; // the tick at which the transition started
    uint32_t end_tick; // the tick at which the transition should end
    uint32_t start_dmx_value; // the DMX value at the start of the transition
} live_transition_t;

typedef struct live_transition_node
{
    live_transition_t live_transition; // the live transition data
    struct list_head list; // linked list node
} live_transition_node_t;

typedef struct live_transition_list
{
    struct list_head head; // head of the linked list
} live_transition_list_t;

#endif