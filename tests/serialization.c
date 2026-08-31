#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "engine.h"

/* ------------------------------------------------------------------ */
/* tiny check harness                                                  */
/* ------------------------------------------------------------------ */

static int checks_run = 0;
static int checks_failed = 0;

#define CHECK(cond, ...)                                     \
    do {                                                     \
        checks_run++;                                        \
        if (!(cond)) {                                       \
            checks_failed++;                                 \
            printf("  [FAIL] %s:%d: ", __FILE__, __LINE__);  \
            printf(__VA_ARGS__);                             \
            printf("\n");                                    \
        }                                                    \
    } while (0)

static int list_count(struct list_head *head)
{
    struct list_head *pos;
    int n = 0;
    for (pos = head->next; pos != head; pos = pos->next)
        n++;
    return n;
}

/* ------------------------------------------------------------------ */
/* builders                                                            */
/* ------------------------------------------------------------------ */

device_t *add_device_to_engine(show_engine_t *engine, device_t *device) {
    device_node_t *new_node = (device_node_t *)malloc(sizeof(device_node_t));
    if (!new_node) {
        printf("Failed to allocate memory for new device node\n");
        return NULL;
    }
    memcpy(&new_node->device, device, sizeof(device_t));
    INIT_LIST_HEAD(&new_node->list);
    list_add_tail(&new_node->list, &engine->device_list_head.head);
    return &new_node->device;
}

/* Returns the cue that now lives in the engine.  The caller's cue_t is
 * copied by value, so its list_head cannot simply be memcpy'd: the copy
 * would still be pointed at by the original's neighbours.  Re-init the
 * copy's head and splice any transitions over, and hand back the stored
 * cue so transitions can be attached to the copy that is actually
 * serialized. */
cue_t *add_cue_to_engine(show_engine_t *engine, cue_t *cue) {
    cue_node_t *new_node = (cue_node_t *)malloc(sizeof(cue_node_t));
    if (!new_node) {
        printf("Failed to allocate memory for new cue node\n");
        return NULL;
    }
    memcpy(&new_node->cue, cue, sizeof(cue_t));
    INIT_LIST_HEAD(&new_node->cue.head);
    list_splice_tail(&cue->head, &new_node->cue.head);
    INIT_LIST_HEAD(&new_node->list);
    list_add_tail(&new_node->list, &engine->cue_list_head.head);
    return &new_node->cue;
}

void add_transition_to_cue(cue_t *cue, transition_t *transition) {
    transition_node_t *new_node = (transition_node_t *)malloc(sizeof(transition_node_t));
    if (!new_node) {
        printf("Failed to allocate memory for new transition node\n");
        return;
    }
    memcpy(&new_node->transition, transition, sizeof(transition_t));
    INIT_LIST_HEAD(&new_node->list);
    list_add_tail(&new_node->list, &cue->head);
}

/* Populate an engine with deterministic sample data. */
static void build_sample_engine(show_engine_t *engine)
{
    init_show_engine(engine);
    init_cue_list(&engine->cue_list_head, "Main Cue List");

    engine->current_cue_number = 1;
    for (int i = 0; i < 513; i++)
        engine->dmx_val[i] = (uint8_t)(i % 256);

    /* three devices, each claiming a block of channels */
    for (int d = 0; d < 3; d++) {
        device_t device;
        memset(&device, 0, sizeof(device)); /* padding too: it gets serialized */
        device.dmx_offset = 1 + d * 16;
        device.dmx_chan_count = PROPERTY_COUNT;
        for (int p = 0; p < PROPERTY_COUNT; p++) {
            device.dmx_chan[p] = device.dmx_offset + p;
            device.dmx_val[p] = (uint8_t)(d * 16 + p);
        }
        add_device_to_engine(engine, &device);
    }

    /* three cues, holding 1, 2 and 3 transitions respectively */
    for (int c = 0; c < 3; c++) {
        cue_t cue;
        cue.cue_number = (uint32_t)(c + 1);
        cue.duration = (uint32_t)((c + 1) * 1000);
        INIT_LIST_HEAD(&cue.head);

        cue_t *stored = add_cue_to_engine(engine, &cue);
        if (!stored)
            continue;

        for (int t = 0; t <= c; t++) {
            transition_t tr;
            tr.device_id = (uint32_t)t;
            tr.dmx_property = (uint32_t)(DIMMER + t);
            tr.dmx_value = (uint32_t)(255 - t * 10);
            add_transition_to_cue(stored, &tr);
        }
    }
}

static void free_engine(show_engine_t *engine)
{
    struct list_head *pos;

    pos = engine->cue_list_head.head.next;
    while (pos != &engine->cue_list_head.head) {
        cue_node_t *node = container_of(pos, cue_node_t, list);
        struct list_head *tpos = node->cue.head.next;
        while (tpos != &node->cue.head) {
            transition_node_t *tnode = container_of(tpos, transition_node_t, list);
            tpos = tpos->next;
            free(tnode);
        }
        pos = pos->next;
        free(node);
    }
    INIT_LIST_HEAD(&engine->cue_list_head.head);

    pos = engine->device_list_head.head.next;
    while (pos != &engine->device_list_head.head) {
        device_node_t *node = container_of(pos, device_node_t, list);
        pos = pos->next;
        free(node);
    }
    INIT_LIST_HEAD(&engine->device_list_head.head);
}

/* ------------------------------------------------------------------ */
/* comparison                                                          */
/* ------------------------------------------------------------------ */

static void compare_cue(cue_t *a, cue_t *b, const char *label, int idx)
{
    CHECK(a->cue_number == b->cue_number,
          "%s cue[%d].cue_number: %u != %u", label, idx, a->cue_number, b->cue_number);
    CHECK(a->duration == b->duration,
          "%s cue[%d].duration: %u != %u", label, idx, a->duration, b->duration);

    int na = list_count(&a->head);
    int nb = list_count(&b->head);
    CHECK(na == nb, "%s cue[%d] transition count: %d != %d", label, idx, na, nb);
    if (na != nb)
        return;

    struct list_head *pa = a->head.next;
    struct list_head *pb = b->head.next;
    for (int i = 0; pa != &a->head; pa = pa->next, pb = pb->next, i++) {
        transition_t *ta = &container_of(pa, transition_node_t, list)->transition;
        transition_t *tb = &container_of(pb, transition_node_t, list)->transition;
        CHECK(ta->device_id == tb->device_id,
              "%s cue[%d].transition[%d].device_id: %u != %u", label, idx, i,
              ta->device_id, tb->device_id);
        CHECK(ta->dmx_property == tb->dmx_property,
              "%s cue[%d].transition[%d].dmx_property: %u != %u", label, idx, i,
              ta->dmx_property, tb->dmx_property);
        CHECK(ta->dmx_value == tb->dmx_value,
              "%s cue[%d].transition[%d].dmx_value: %u != %u", label, idx, i,
              ta->dmx_value, tb->dmx_value);
    }
}

static void compare_engines(show_engine_t *a, show_engine_t *b, const char *label)
{
    printf("Comparing engines (%s)...\n", label);

    CHECK(a->current_cue_number == b->current_cue_number,
          "%s current_cue_number: %u != %u", label,
          a->current_cue_number, b->current_cue_number);
    CHECK(memcmp(a->dmx_val, b->dmx_val, sizeof(a->dmx_val)) == 0,
          "%s dmx_val differs", label);
    CHECK(memcmp(a->cue_list_head.name, b->cue_list_head.name,
                 sizeof(a->cue_list_head.name)) == 0,
          "%s cue list name: \"%.32s\" != \"%.32s\"", label,
          a->cue_list_head.name, b->cue_list_head.name);

    int nca = list_count(&a->cue_list_head.head);
    int ncb = list_count(&b->cue_list_head.head);
    CHECK(nca == ncb, "%s cue count: %d != %d", label, nca, ncb);
    if (nca == ncb) {
        struct list_head *pa = a->cue_list_head.head.next;
        struct list_head *pb = b->cue_list_head.head.next;
        for (int i = 0; pa != &a->cue_list_head.head; pa = pa->next, pb = pb->next, i++)
            compare_cue(&container_of(pa, cue_node_t, list)->cue,
                        &container_of(pb, cue_node_t, list)->cue, label, i);
    }

    int nda = list_count(&a->device_list_head.head);
    int ndb = list_count(&b->device_list_head.head);
    CHECK(nda == ndb, "%s device count: %d != %d", label, nda, ndb);
    if (nda == ndb) {
        struct list_head *pa = a->device_list_head.head.next;
        struct list_head *pb = b->device_list_head.head.next;
        for (int i = 0; pa != &a->device_list_head.head; pa = pa->next, pb = pb->next, i++) {
            device_t *da = &container_of(pa, device_node_t, list)->device;
            device_t *db = &container_of(pb, device_node_t, list)->device;
            CHECK(da->dmx_offset == db->dmx_offset,
                  "%s device[%d].dmx_offset: %u != %u", label, i,
                  da->dmx_offset, db->dmx_offset);
            CHECK(da->dmx_chan_count == db->dmx_chan_count,
                  "%s device[%d].dmx_chan_count: %u != %u", label, i,
                  da->dmx_chan_count, db->dmx_chan_count);
            CHECK(memcmp(da->dmx_chan, db->dmx_chan, sizeof(da->dmx_chan)) == 0,
                  "%s device[%d].dmx_chan differs", label, i);
            CHECK(memcmp(da->dmx_val, db->dmx_val, sizeof(da->dmx_val)) == 0,
                  "%s device[%d].dmx_val differs", label, i);
        }
    }
}

/* ------------------------------------------------------------------ */

#define BUF_SIZE 4096

int main(void)
{
    printf("=== show engine serialization round-trip test ===\n\n");

    printf("Initializing show engine...\n");
    show_engine_t engine;
    build_sample_engine(&engine);
    printf("Sample engine: %d cues, %d devices\n",
           list_count(&engine.cue_list_head.head),
           list_count(&engine.device_list_head.head));

    /* ---- round 1: serialize ---- */
    uint8_t buffer[BUF_SIZE];
    memset(buffer, 0xAA, sizeof(buffer));
    int bytes_written = serialize_show_engine(&engine, buffer, sizeof(buffer));
    printf("Serialized %d bytes\n", bytes_written);
    CHECK(bytes_written > 0, "serialize_show_engine failed (returned %d)", bytes_written);
    if (bytes_written <= 0)
        goto done;

    /* ---- round 1: deserialize ---- */
    printf("Deserializing show engine...\n");
    show_engine_t new_engine;
    int bytes_read = deserialize_show_engine(&new_engine, buffer, (size_t)bytes_written);
    printf("Deserialized %d bytes\n", bytes_read);
    CHECK(bytes_read == bytes_written,
          "byte count mismatch: wrote %d, read %d", bytes_written, bytes_read);

    printf("Current cue number: %u\n", new_engine.current_cue_number);
    printf("DMX values: ");
    for (int i = 0; i < 10; i++)
        printf("%u ", new_engine.dmx_val[i]);
    printf("\n");

    compare_engines(&engine, &new_engine, "round 1");

    /* ---- round 2: re-serialize and compare the raw bytes ---- */
    uint8_t buffer2[BUF_SIZE];
    memset(buffer2, 0x55, sizeof(buffer2));
    int bytes_written2 = serialize_show_engine(&new_engine, buffer2, sizeof(buffer2));
    printf("Re-serialized %d bytes\n", bytes_written2);
    CHECK(bytes_written2 == bytes_written,
          "round 2 length: %d != %d", bytes_written2, bytes_written);
    if (bytes_written2 == bytes_written)
        CHECK(memcmp(buffer, buffer2, (size_t)bytes_written) == 0,
              "round 2 produced different bytes than round 1");

    show_engine_t third_engine;
    int bytes_read2 = deserialize_show_engine(&third_engine, buffer2, (size_t)bytes_written2);
    CHECK(bytes_read2 == bytes_written2,
          "round 2 byte count mismatch: wrote %d, read %d", bytes_written2, bytes_read2);
    compare_engines(&engine, &third_engine, "round 2");

    free_engine(&third_engine);
    free_engine(&new_engine);

done:
    free_engine(&engine);

    printf("\n%d checks run, %d failed\n", checks_run, checks_failed);
    if (checks_failed == 0) {
        printf("PASS\n");
        return 0;
    }
    printf("FAIL\n");
    return 1;
}

void render_put_pixel(int x, int y, uint32_t color)
{
}
