/*
 * ErgoFlow — BLE Mesh Message Handler
 * Dispatches incoming mesh messages to node-specific handlers
 *
 * Copyright (c) 2026 jayis1. MIT License.
 */

#include "mesh_handler.h"
#include "protocol.h"
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/bluetooth/mesh/msg.h>

LOG_MODULE_REGISTER(mesh_handler, CONFIG_ERGO_LOG_LEVEL);

/* Mesh model opcode lists */
static const bt_mesh_model_op_t ergo_opcodes[] = {
    { ERGO_OP_PRESSURE_MAP,    17, mesh_handler_on_pressure_map },
    { ERGO_OP_IMU_ORIENTATION, 18, mesh_handler_on_imu_orientation },
    { ERGO_OP_HEART_RATE,       2, mesh_handler_on_heart_rate },
    { ERGO_OP_DESK_COMMAND,     4, mesh_handler_on_desk_command },
    { ERGO_OP_DESK_STATUS,      5, mesh_handler_on_desk_status },
    { ERGO_OP_AMBIENT_READING,  10, mesh_handler_on_ambient_reading },
    { ERGO_OP_POSTURE_SCORE,    4, mesh_handler_on_posture_score },
    { ERGO_OP_BREAK_REMINDER,   3,  mesh_handler_on_break_reminder },
    { ERGO_OP_LIGHTING_CMD,     6, mesh_handler_on_lighting_cmd },
    { ERGO_OP_MONITOR_TILT,     2, mesh_handler_on_monitor_tilt },
    { ERGO_OP_OTA_AVAILABLE,   20,  mesh_handler_on_ota_available },
    { ERGO_OP_OTA_DATA,        18,  mesh_handler_on_ota_data },
    { ERGO_OP_NODE_HEARTBEAT,  4,   mesh_handler_on_node_heartbeat },
    { ERGO_OP_CALIBRATION,     9,   mesh_handler_on_calibration },
    { ERGO_OP_FACTORY_RESET,   0,   mesh_handler_on_factory_reset },
    BT_MESH_MODEL_OP_END,
};

/* Message callback registry */
static ergo_msg_callback_t msg_callbacks[ERGO_MAX_CALLBACKS];
static int callback_count;

int mesh_handler_init(void)
{
    callback_count = 0;
    memset(msg_callbacks, 0, sizeof(msg_callbacks));
    LOG_INF("Mesh handler initialized");
    return 0;
}

int mesh_handler_register_callback(uint16_t opcode, ergo_msg_callback_t cb, void *user_data)
{
    if (callback_count >= ERGO_MAX_CALLBACKS) {
        LOG_ERR("Callback registry full");
        return -ENOMEM;
    }
    msg_callbacks[callback_count].opcode = opcode;
    msg_callbacks[callback_count].cb = cb;
    msg_callbacks[callback_count].user_data = user_data;
    callback_count++;
    return 0;
}

/* Dispatch incoming messages to registered callbacks */
static void dispatch_message(uint16_t opcode, const uint8_t *data, uint16_t len,
                              uint16_t src_addr)
{
    for (int i = 0; i < callback_count; i++) {
        if (msg_callbacks[i].opcode == opcode || msg_callbacks[i].opcode == 0xFFFF) {
            msg_callbacks[i].cb(opcode, data, len, src_addr, msg_callbacks[i].user_data);
        }
    }
}

/* Mesh message handlers — parse and dispatch */
void mesh_handler_on_pressure_map(struct bt_mesh_model *model,
                                   struct bt_mesh_msg_ctx *ctx,
                                   struct net_buf_simple *buf)
{
    if (buf->len < 17) {
        LOG_WRN("Pressure map too short: %d", buf->len);
        return;
    }
    dispatch_message(ERGO_OP_PRESSURE_MAP, buf->data, buf->len, ctx->addr);
}

void mesh_handler_on_imu_orientation(struct bt_mesh_model *model,
                                       struct bt_mesh_msg_ctx *ctx,
                                       struct net_buf_simple *buf)
{
    if (buf->len < 18) {
        LOG_WRN("IMU orientation too short: %d", buf->len);
        return;
    }
    dispatch_message(ERGO_OP_IMU_ORIENTATION, buf->data, buf->len, ctx->addr);
}

void mesh_handler_on_heart_rate(struct bt_mesh_model *model,
                                 struct bt_mesh_msg_ctx *ctx,
                                 struct net_buf_simple *buf)
{
    if (buf->len < 2) {
        LOG_WRN("Heart rate too short: %d", buf->len);
        return;
    }
    dispatch_message(ERGO_OP_HEART_RATE, buf->data, buf->len, ctx->addr);
}

void mesh_handler_on_desk_command(struct bt_mesh_model *model,
                                   struct bt_mesh_msg_ctx *ctx,
                                   struct net_buf_simple *buf)
{
    if (buf->len < 4) {
        LOG_WRN("Desk command too short: %d", buf->len);
        return;
    }
    dispatch_message(ERGO_OP_DESK_COMMAND, buf->data, buf->len, ctx->addr);
}

void mesh_handler_on_desk_status(struct bt_mesh_model *model,
                                   struct bt_mesh_msg_ctx *ctx,
                                   struct net_buf_simple *buf)
{
    dispatch_message(ERGO_OP_DESK_STATUS, buf->data, buf->len, ctx->addr);
}

void mesh_handler_on_ambient_reading(struct bt_mesh_model *model,
                                       struct bt_mesh_msg_ctx *ctx,
                                       struct net_buf_simple *buf)
{
    dispatch_message(ERGO_OP_AMBIENT_READING, buf->data, buf->len, ctx->addr);
}

void mesh_handler_on_posture_score(struct bt_mesh_model *model,
                                    struct bt_mesh_msg_ctx *ctx,
                                    struct net_buf_simple *buf)
{
    dispatch_message(ERGO_OP_POSTURE_SCORE, buf->data, buf->len, ctx->addr);
}

void mesh_handler_on_break_reminder(struct bt_mesh_model *model,
                                     struct bt_mesh_msg_ctx *ctx,
                                     struct net_buf_simple *buf)
{
    dispatch_message(ERGO_OP_BREAK_REMINDER, buf->data, buf->len, ctx->addr);
}

void mesh_handler_on_lighting_cmd(struct bt_mesh_model *model,
                                    struct bt_mesh_msg_ctx *ctx,
                                    struct net_buf_simple *buf)
{
    dispatch_message(ERGO_OP_LIGHTING_CMD, buf->data, buf->len, ctx->addr);
}

void mesh_handler_on_monitor_tilt(struct bt_mesh_model *model,
                                   struct bt_mesh_msg_ctx *ctx,
                                   struct net_buf_simple *buf)
{
    dispatch_message(ERGO_OP_MONITOR_TILT, buf->data, buf->len, ctx->addr);
}

void mesh_handler_on_ota_available(struct bt_mesh_model *model,
                                    struct bt_mesh_msg_ctx *ctx,
                                    struct net_buf_simple *buf)
{
    dispatch_message(ERGO_OP_OTA_AVAILABLE, buf->data, buf->len, ctx->addr);
}

void mesh_handler_on_ota_data(struct bt_mesh_model *model,
                                struct bt_mesh_msg_ctx *ctx,
                                struct net_buf_simple *buf)
{
    dispatch_message(ERGO_OP_OTA_DATA, buf->data, buf->len, ctx->addr);
}

void mesh_handler_on_node_heartbeat(struct bt_mesh_model *model,
                                     struct bt_mesh_msg_ctx *ctx,
                                     struct net_buf_simple *buf)
{
    dispatch_message(ERGO_OP_NODE_HEARTBEAT, buf->data, buf->len, ctx->addr);
}

void mesh_handler_on_calibration(struct bt_mesh_model *model,
                                  struct bt_mesh_msg_ctx *ctx,
                                  struct net_buf_simple *buf)
{
    dispatch_message(ERGO_OP_CALIBRATION, buf->data, buf->len, ctx->addr);
}

void mesh_handler_on_factory_reset(struct bt_mesh_model *model,
                                    struct bt_mesh_msg_ctx *ctx,
                                    struct net_buf_simple *buf)
{
    dispatch_message(ERGO_OP_FACTORY_RESET, buf->data, buf->len, ctx->addr);
}

/* Transmit a message over BLE mesh */
int mesh_handler_send(uint16_t opcode, const uint8_t *data, uint16_t len, uint16_t dst_addr)
{
    struct bt_mesh_msg_ctx ctx = {
        .app_idx = ERGO_MESH_APP_IDX_TELEM,
        .addr = dst_addr,
        .send_ttl = ERGO_MESH_TTL,
    };
    NET_BUF_SIMPLE_DEFINE(msg, len + 4);
    net_buf_simple_add_u8(&msg, 0);  /* will be overwritten */
    net_buf_simple_add_u8(&msg, 0);
    net_buf_simple_add_mem(&msg, data, len);

    int err = bt_mesh_model_send(NULL, &ctx, &msg, NULL, NULL);
    if (err) {
        LOG_ERR("Mesh send failed: %d", err);
    }
    return err;
}