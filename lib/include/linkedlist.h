#ifndef LINKEDLISTS_H
#define LINKEDLISTS_H
#include <stdint.h>
#include <stddef.h>
#ifndef offsetof
#define offsetof(type, member) \
    ((size_t)&(((type *)0)->member))
#endif

#define container_of(ptr, type, member) \
    ((type *)((char *)(ptr) - offsetof(type, member)))

struct list_head
{
    struct list_head *next;
    struct list_head *prev;
};

static inline void INIT_LIST_HEAD(struct list_head *list)
{
    // printk("INIT_LIST_HEAD called on %p\n", list);
    list->next = list;
    list->prev = list;
}

static inline void __list_add(struct list_head *add,
                              struct list_head *prev,
                              struct list_head *next)
{
    next->prev = add;
    add->next = next;
    add->prev = prev;
    prev->next = add;
}

static inline void __list_del(struct list_head *prev,
                              struct list_head *next)
{
    next->prev = prev;
    prev->next = next;
}

static inline void list_add(struct list_head *add,
                            struct list_head *head)
{
    __list_add(add, head, head->next);
}

static inline void list_add_tail(struct list_head *add,
                                 struct list_head *head)
{
    __list_add(add, head->prev, head);
}

static inline void list_del(struct list_head *entry)
{
    __list_del(entry->prev, entry->next);
    entry->next = entry->prev = 0;
}

static inline void list_splice_tail(struct list_head *list,
                                         struct list_head *head)
{
    if (list->next == list)
        return;

    struct list_head *first = list->next;
    struct list_head *last  = list->prev;
    struct list_head *tail  = head->prev;

    first->prev = tail;
    tail->next  = first;

    last->next = head;
    head->prev = last;

    list->next = list;
    list->prev = list;
}

#endif