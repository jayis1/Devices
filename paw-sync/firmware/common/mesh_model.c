/*
 * mesh_model.c — PawSync BLE mesh vendor model wrapper
 *
 * Provides a simplified interface for sending and receiving PawSync
 * protocol messages over a BLE 5.3 mesh network. Wraps the nRF5 SDK
 * SoftDevice mesh vendor model API.
 *
 * In production, this links against the nRF5 SDK mesh stack. For
 * development/simulation, the functions can be stubbed to use a UART
 * transport instead (useful for RP2040 hub which receives mesh data
 * via UART from the nRF52840 radio).
 *
 * SPDX-License-Identifier: MIT
 */
#include "paw_protocol.h"
#include <string.h>

/* ---- Configuration ---- */
#define PAW_MESH_MAX_PAYLOAD  64

/* ---- RX callback ---- */
typedef void (*paw_mesh_rx_cb_t)(uint8_t type, const uint8_t *data, size_t len);
static paw_mesh_rx_cb_t g_rx_callback = NULL;

/* ---- TX transport (platform-specific) ---- */
/* In production: nRF52840 uses SoftDevice mesh vendor model.
 * On RP2040 hub: data arrives via UART from nRF52840 radio. */
typedef int (*paw_mesh_tx_t)(const uint8_t *data, size_t len);
static paw_mesh_tx_t g_tx_func = NULL;

void paw_mesh_set_tx(paw_mesh_tx_t tx_func)
{
    g_tx_func = tx_func;
}

void paw_mesh_set_rx_callback(paw_mesh_rx_cb_t cb)
{
    g_rx_callback = cb;
}

/*
 * Send a PawSync message over the mesh.
 * Packs CRC and calls the TX transport.
 */
int paw_mesh_send(uint8_t msg_type, uint8_t node_id, const void *payload, size_t payload_len)
{
    uint8_t buf[PAW_MESH_MAX_PAYLOAD];
    if (payload_len + 4 > PAW_MESH_MAX_PAYLOAD) return -1;

    /* Frame: [type][node_id][seq][payload...][crc16] */
    static uint8_t seq = 0;
    buf[0] = msg_type;
    buf[1] = node_id;
    buf[2] = seq++;
    memcpy(&buf[3], payload, payload_len);

    /* Compute CRC over header + payload */
    uint16_t crc = paw_crc16(buf, 3 + payload_len);
    buf[3 + payload_len]     = (uint8_t)(crc & 0xFF);
    buf[3 + payload_len + 1] = (uint8_t)((crc >> 8) & 0xFF);

    if (g_tx_func)
        return g_tx_func(buf, 3 + payload_len + 2);
    return -1;
}

/*
 * Process a received mesh frame.
 * Verifies CRC and dispatches to the registered callback.
 */
void paw_mesh_on_rx(const uint8_t *data, size_t len)
{
    if (len < 5) return;  /* min: type + node_id + seq + crc16 */

    size_t payload_len = len - 5;  /* subtract header(3) + crc(2) */
    uint16_t received_crc = (uint16_t)data[3 + payload_len] |
                           ((uint16_t)data[4 + payload_len] << 8);
    if (paw_verify_crc(data, 3 + payload_len, received_crc) != 0)
        return;  /* CRC mismatch — drop */

    uint8_t msg_type = data[0];
    if (g_rx_callback)
        g_rx_callback(msg_type, &data[3], payload_len);
}

/*
 * Broadcast a wellness score to all mesh nodes.
 * Used by the hub to distribute the current wellness score.
 */
int paw_mesh_broadcast_wellness(uint8_t wellness, uint8_t illness_risk,
                                 uint8_t anxiety_level)
{
    paw_wellness_payload_t wp = {0};
    wp.type          = PAW_MSG_WELLNESS;
    wp.node_id       = PAW_NODE_ID_HUB;
    wp.wellness_score = wellness;
    wp.illness_risk   = illness_risk;
    wp.anxiety_level  = anxiety_level;
    paw_pack_crc(&wp, sizeof(wp) - 2);
    return paw_mesh_send(PAW_MSG_WELLNESS, PAW_NODE_ID_HUB,
                        &wp, sizeof(wp) - 2);
}

/*
 * Send an enrichment command to a target node.
 */
int paw_mesh_send_enrichment(uint8_t target_node, uint8_t enrichment_type,
                              uint8_t intensity)
{
    paw_enrichment_payload_t ep = {0};
    ep.type            = PAW_MSG_ENRICHMENT;
    ep.node_id         = PAW_NODE_ID_HUB;
    ep.enrichment_type = enrichment_type;
    ep.target_node     = target_node;
    ep.intensity       = intensity;
    paw_pack_crc(&ep, sizeof(ep) - 2);
    return paw_mesh_send(PAW_MSG_ENRICHMENT, PAW_NODE_ID_HUB,
                         &ep, sizeof(ep) - 2);
}