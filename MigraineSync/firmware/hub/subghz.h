/**
 * MigraineSync — Hub Sub-GHz Driver Header
 * ========================================
 * License: MIT
 */

#ifndef HUB_SUBGHZ_H
#define HUB_SUBGHZ_H

#include <stdint.h>
#include <stddef.h>

/**
 * Initialize SX1262 Sub-GHz transceiver.
 * Returns 0 on success, -1 on failure.
 */
int subghz_init(void);

/**
 * Send a packet via Sub-GHz.
 * Returns bytes sent, or -1 on error.
 */
int subghz_send(const uint8_t *data, size_t len);

/**
 * Receive a packet via Sub-GHz (blocking with timeout).
 * Returns bytes received, 0 on timeout, -1 on error.
 */
int subghz_recv(uint8_t *buf, size_t max_len, uint32_t timeout_ms);

#endif /* HUB_SUBGHZ_H */