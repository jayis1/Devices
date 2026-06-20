/*
 * mesh_model.c — CalmGrid BLE mesh vendor model wrapper
 *
 * Provides a simplified interface for sending and receiving CalmGrid
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
#include "calm_protocol.h"
#include <string.h>

/* ---- Configuration ---- */
#define CALM_MESH_MAX_PAYLOAD  64

/* ---- RX callback ---- */
static calm_mesh_rx_cb_t g_rx_callback = NULL;

/* ---- TX transport (platform-specific) ---- */
/* In production: nRF52840 uses SoftDevice mesh vendor model.
 * On RP2040 hub: data arrives via UART from nRF52840 radio. */
static calm_mesh_tx_t g_tx_func = NULL;

void calm_mesh_set_tx(calm_mesh_tx_t tx_func)
{
    g_tx_func = tx_func;
}

void calm_mesh_set_rx_callback(calm_mesh_rx_cb_t cb)
{
    g_rx_callback = cb;
}

/*
 * Send a CalmGrid message over the mesh.
 * Packs CRC and calls the TX transport.
 */
int calm_mesh_send(uint8_t msg_type, uint8_t node_id,
                   const void *payload, size_t payload_len)
{
    uint8_t buf[CALM_MESH_MAX_PAYLOAD];
    if (payload_len + 4 > CALM_MESH_MAX_PAYLOAD) return -1;

    /* Frame: [type][node_id][seq][payload...][crc16] */
    static uint8_t seq = 0;
    buf[0] = msg_type;
    buf[1] = node_id;
    buf[2] = seq++;
    memcpy(&buf[3], payload, payload_len);

    /* Compute CRC over header + payload */
    uint16_t crc = calm_crc16(buf, 3 + payload_len);
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
void calm_mesh_on_rx(const uint8_t *data, size_t len)
{
    if (len < 5) return;  /* min: type + node_id + seq + crc16 */

    size_t payload_len = len - 5;  /* subtract header(3) + crc(2) */
    uint16_t received_crc = (uint16_t)data[3 + payload_len] |
                           ((uint16_t)data[4 + payload_len] << 8);
    if (calm_verify_crc(data, 3 + payload_len, received_crc) != 0)
        return;  /* CRC mismatch — drop */

    if (g_rx_callback)
        g_rx_callback(data[0], &data[3], payload_len);
}

/*
 * Broadcast a stress score to all mesh nodes.
 * Light nodes use this to autonomously adjust scenes.
 */
int calm_mesh_broadcast_stress_score(uint8_t stress, uint8_t burnout,
                                      uint8_t recovery)
{
    calm_stress_score_payload_t p;
    p.type = CALM_MSG_STRESS_SCORE;
    p.node_id = CALM_NODE_ID_HUB;
    p.seq = 0;  /* filled by calm_mesh_send */
    p.flags = 0;
    p.stress_score = stress;
    p.burnout_risk = burnout;
    p.recovery_score = recovery;
    calm_pack_crc(&p, sizeof(p) - 2);
    return calm_mesh_send(CALM_MSG_STRESS_SCORE, CALM_NODE_ID_HUB,
                          &p, sizeof(p) - 2);
}

/*
 * Send a lighting scene command to a specific light node.
 */
int calm_mesh_send_lighting(uint8_t scene, uint8_t brightness,
                             uint16_t warm_k, uint16_t cool_k)
{
    calm_lighting_payload_t p;
    p.type = CALM_MSG_LIGHTING;
    p.node_id = CALM_NODE_ID_HUB;
    p.seq = 0;
    p.flags = 0;
    p.scene = scene;
    p.brightness = brightness;
    p.warm_kelvin = warm_k;
    p.cool_kelvin = cool_k;
    p.ambient_lux = 0;
    calm_pack_crc(&p, sizeof(p) - 2);
    return calm_mesh_send(CALM_MSG_LIGHTING, CALM_NODE_ID_HUB,
                          &p, sizeof(p) - 2);
}

/*
 * Send an intervention command to mesh nodes.
 */
int calm_mesh_send_intervention(uint8_t intervention_id, uint8_t param1,
                                 uint8_t param2, uint16_t duration_s)
{
    calm_intervention_payload_t p;
    p.type = CALM_MSG_INTERVENTION;
    p.node_id = CALM_NODE_ID_HUB;
    p.seq = 0;
    p.flags = 0;
    p.intervention_id = intervention_id;
    p.param1 = param1;
    p.param2 = param2;
    p.duration_s = duration_s;
    calm_pack_crc(&p, sizeof(p) - 2);
    return calm_mesh_send(CALM_MSG_INTERVENTION, CALM_NODE_ID_HUB,
                          &p, sizeof(p) - 2);
}