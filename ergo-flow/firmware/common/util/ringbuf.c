/*
 * ErgoFlow — Lock-Free Ring Buffer
 * Single-producer, single-consumer ring buffer for sensor data streaming
 *
 * Copyright (c) 2026 jayis1. MIT License.
 */

#include "ringbuf.h"
#include <string.h>

int ringbuf_init(ringbuf_t *rb, uint8_t *buffer, uint16_t size)
{
    rb->buffer = buffer;
    rb->size = size;
    rb->head = 0;
    rb->tail = 0;
    rb->count = 0;
    return 0;
}

uint16_t ringbuf_put(ringbuf_t *rb, const uint8_t *data, uint16_t len)
{
    uint16_t available = rb->size - rb->count;
    uint16_t to_write = (len < available) ? len : available;

    for (uint16_t i = 0; i < to_write; i++) {
        rb->buffer[rb->head] = data[i];
        rb->head = (rb->head + 1) % rb->size;
        rb->count++;
    }

    return to_write;
}

uint16_t ringbuf_get(ringbuf_t *rb, uint8_t *data, uint16_t len)
{
    uint16_t to_read = (len < rb->count) ? len : rb->count;

    for (uint16_t i = 0; i < to_read; i++) {
        data[i] = rb->buffer[rb->tail];
        rb->tail = (rb->tail + 1) % rb->size;
        rb->count--;
    }

    return to_read;
}

uint16_t ringbuf_available(ringbuf_t *rb)
{
    return rb->size - rb->count;
}

uint16_t ringbuf_count(ringbuf_t *rb)
{
    return rb->count;
}

bool ringbuf_empty(ringbuf_t *rb)
{
    return rb->count == 0;
}

bool ringbuf_full(ringbuf_t *rb)
{
    return rb->count == rb->size;
}

void ringbuf_flush(ringbuf_t *rb)
{
    rb->head = 0;
    rb->tail = 0;
    rb->count = 0;
}