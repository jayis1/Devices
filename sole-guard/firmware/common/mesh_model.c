/*
 * mesh_model.c — SoleGuard SIG Mesh vendor model registration
 *
 * Wraps the Zephyr Bluetooth Mesh API to register a vendor model that
 * publishes/subscribes sole_payload_t messages between body-worn nodes.
 *
 * SPDX-License-Identifier: MIT
 */
#include <zephyr/bluetooth/mesh.h>
#include "sole_protocol.h"

#define SOLE_MODEL_OPC(publish) \
    BT_MESH_MODEL_VND_CB(SOLE_VENDOR_ID, SOLE_MODEL_ID_PRESSURE, \
                         NULL, NULL, &sole_model_cb, publish)

static void sole_model_rx(struct bt_mesh_model *model,
                          struct bt_mesh_msg_ctx *ctx,
                          struct net_buf_simple *buf)
{
    /* buf->data contains a sole_*_payload_t (without mesh opcode). */
    if (buf->len < 2) return;

    uint8_t type = buf->data[0];
    /* Dispatch to the node-local handler registered via sole_set_rx_handler */
    extern void sole_on_mesh_rx(uint8_t type, const uint8_t *data, size_t len);
    sole_on_mesh_rx(type, buf->data, (size_t)buf->len);
}

static const struct bt_mesh_model_cb sole_model_cb = {
    .start  = NULL,
    .reset  = NULL,
    .recv   = sole_model_rx,
};

/* Publish a payload on the pressure vendor model.
 * Returns 0 on success, negative errno on failure. */
int sole_mesh_publish(struct bt_mesh_model *model, const uint8_t *data, size_t len)
{
    NET_BUF_SIMPLE_DEFINE(msg, 32);
    net_buf_simple_add_mem(&msg, data, len);
    return bt_mesh_model_publish(model, &msg);
}