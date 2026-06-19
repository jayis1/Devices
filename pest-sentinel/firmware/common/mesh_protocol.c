/*
 * mesh_protocol.c — Shared Sub-GHz LoRa mesh protocol implementation
 *
 * Used by all PestSentinel field nodes (hub, trap, field-sensor, dispenser).
 *
 * CRC-16/CCITT, packet serialize/parse, and helper functions.
 */

#include "mesh_protocol.h"
#include <string.h>

/* ---- CRC-16/CCITT-FALSE (poly 0x1021, init 0xFFFF) ---- */
uint16_t mesh_crc16(const uint8_t *data, uint8_t len)
{
    uint16_t crc = 0xFFFF;
    uint8_t i;
    while (len--) {
        crc ^= (uint16_t)(*data++ << 8);
        for (i = 0; i < 8; i++) {
            if (crc & 0x8000)
                crc = (crc << 1) ^ 0x1021;
            else
                crc <<= 1;
        }
    }
    return crc;
}

/* ---- Initialize a packet for TX ---- */
void mesh_packet_init(mesh_packet_t *pkt, uint8_t src_id, uint8_t dst_id,
                      uint8_t pkt_type, uint8_t sf)
{
    memset(pkt, 0, sizeof(*pkt));
    pkt->preamble[0] = 0xAA;
    pkt->preamble[1] = 0xAA;
    pkt->preamble[2] = 0xAA;
    pkt->preamble[3] = 0xAA;
    pkt->sync = MESH_SYNC_WORD;
    pkt->src_id = src_id;
    pkt->dst_id = dst_id;
    pkt->pkt_type = pkt_type;
    pkt->sf_used = sf;
    pkt->snr_db = 0;
    pkt->rssi_dbm = 0;
    pkt->len = 0;
}

/* ---- Serialize packet to raw TX bytes (returns total byte count) ---- */
uint8_t mesh_packet_serialize(const mesh_packet_t *pkt, uint8_t *out, uint8_t max_len)
{
    /* Layout: preamble(4) sync(2) len(1) src(1) dst(1) type(1) sf(1) snr(1) rssi(1)
     *         payload[len] crc(2) */
    uint8_t total = 4 + 2 + 1 + 1 + 1 + 1 + 1 + 1 + 1 + pkt->len + 2;
    if (total > max_len || pkt->len > MESH_MAX_PAYLOAD)
        return 0;

    uint8_t idx = 0;
    memcpy(&out[idx], pkt->preamble, 4); idx += 4;
    out[idx++] = (uint8_t)(pkt->sync & 0xFF);
    out[idx++] = (uint8_t)((pkt->sync >> 8) & 0xFF);
    out[idx++] = pkt->len;
    out[idx++] = pkt->src_id;
    out[idx++] = pkt->dst_id;
    out[idx++] = pkt->pkt_type;
    out[idx++] = pkt->sf_used;
    out[idx++] = (uint8_t)pkt->snr_db;
    out[idx++] = (uint8_t)pkt->rssi_dbm;
    memcpy(&out[idx], pkt->payload, pkt->len);
    idx += pkt->len;

    /* CRC over len+src+dst+type+sf+payload (not preamble/sync/snr/rssi/crc) */
    uint8_t crc_buf[2 + 1 + 1 + 1 + 1 + MESH_MAX_PAYLOAD];
    uint8_t ci = 0;
    crc_buf[ci++] = pkt->len;
    crc_buf[ci++] = pkt->src_id;
    crc_buf[ci++] = pkt->dst_id;
    crc_buf[ci++] = pkt->pkt_type;
    crc_buf[ci++] = pkt->sf_used;
    memcpy(&crc_buf[ci], pkt->payload, pkt->len);
    ci += pkt->len;
    uint16_t crc = mesh_crc16(crc_buf, ci);
    out[idx++] = (uint8_t)(crc & 0xFF);
    out[idx++] = (uint8_t)((crc >> 8) & 0xFF);

    return total;
}

/* ---- Parse raw RX bytes into packet (returns 0 on success, -1 on error) ---- */
int mesh_packet_parse(const uint8_t *raw, uint8_t raw_len, mesh_packet_t *pkt)
{
    if (raw_len < 4 + 2 + 1 + 1 + 1 + 1 + 1 + 1 + 1 + 2)
        return -1;  /* too short */

    uint8_t idx = 0;
    memcpy(pkt->preamble, &raw[idx], 4); idx += 4;
    if (pkt->preamble[0] != 0xAA || pkt->preamble[3] != 0xAA)
        return -1;

    pkt->sync = (uint16_t)raw[idx] | ((uint16_t)raw[idx + 1] << 8);
    idx += 2;
    if (pkt->sync != MESH_SYNC_WORD)
        return -1;

    pkt->len = raw[idx++];
    pkt->src_id = raw[idx++];
    pkt->dst_id = raw[idx++];
    pkt->pkt_type = raw[idx++];
    pkt->sf_used = raw[idx++];
    pkt->snr_db = (int8_t)raw[idx++];
    pkt->rssi_dbm = (int8_t)raw[idx++];

    if (pkt->len > MESH_MAX_PAYLOAD)
        return -1;
    if (raw_len < idx + pkt->len + 2)
        return -1;

    memcpy(pkt->payload, &raw[idx], pkt->len);
    idx += pkt->len;

    /* Verify CRC */
    uint8_t crc_buf[2 + 1 + 1 + 1 + 1 + MESH_MAX_PAYLOAD];
    uint8_t ci = 0;
    crc_buf[ci++] = pkt->len;
    crc_buf[ci++] = pkt->src_id;
    crc_buf[ci++] = pkt->dst_id;
    crc_buf[ci++] = pkt->pkt_type;
    crc_buf[ci++] = pkt->sf_used;
    memcpy(&crc_buf[ci], pkt->payload, pkt->len);
    ci += pkt->len;
    uint16_t crc_calc = mesh_crc16(crc_buf, ci);

    uint16_t crc_rx = (uint16_t)raw[idx] | ((uint16_t)raw[idx + 1] << 8);
    if (crc_calc != crc_rx)
        return -2;  /* CRC mismatch */

    pkt->crc16 = crc_rx;
    return 0;
}

/* ---- Check if a packet is addressed to this node ---- */
static inline int mesh_is_for_me(const mesh_packet_t *pkt, uint8_t my_id)
{
    return (pkt->dst_id == NODE_ID_BROADCAST) || (pkt->dst_id == my_id);
}

/* ---- Get payload as a typed pointer (caller knows pkt_type) ---- */
static inline const void *mesh_payload(const mesh_packet_t *pkt)
{
    return (const void *)pkt->payload;
}