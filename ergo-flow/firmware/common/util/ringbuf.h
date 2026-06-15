/*
 * ErgoFlow — Lock-Free Ring Buffer Header
 * Copyright (c) 2026 jayis1. MIT License.
 */

#ifndef RINGBUF_H
#define RINGBUF_H

#include <stdint.h>
#include <stdbool.h>

typedef struct {
    uint8_t *buffer;
    uint16_t size;
    volatile uint16_t head;
    volatile uint16_t tail;
    volatile uint16_t count;
} ringbuf_t;

/* Initialize ring buffer with given backing storage */
int ringbuf_init(ringbuf_t *rb, uint8_t *buffer, uint16_t size);

/* Write data into ring buffer, returns bytes actually written */
uint16_t ringbuf_put(ringbuf_t *rb, const uint8_t *data, uint16_t len);

/* Read data from ring buffer, returns bytes actually read */
uint16_t ringbuf_get(ringbuf_t *rb, uint8_t *data, uint16_t len);

/* Get number of free bytes available for writing */
uint16_t ringbuf_available(ringbuf_t *rb);

/* Get number of bytes currently in buffer */
uint16_t ringbuf_count(ringbuf_t *rb);

/* Check if buffer is empty */
bool ringbuf_empty(ringbuf_t *rb);

/* Check if buffer is full */
bool ringbuf_full(ringbuf_t *rb);

/* Flush all data from buffer */
void ringbuf_flush(ringbuf_t *rb);

#endif /* RINGBUF_H */