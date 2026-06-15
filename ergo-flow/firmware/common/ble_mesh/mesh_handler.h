/*
 * ErgoFlow — BLE Mesh Message Handler Header
 * Copyright (c) 2026 jayis1. MIT License.
 */

#ifndef MESH_HANDLER_H
#define MESH_HANDLER_H

#include <stdint.h>
#include <zephyr/bluetooth/mesh.h>
#include "mesh_config.h"

#define ERGO_MAX_CALLBACKS 16

/* Message callback type */
typedef void (*ergo_msg_callback_t)(uint16_t opcode, const uint8_t *data,
                                     uint16_t len, uint16_t src_addr,
                                     void *user_data);

/* Initialize mesh handler */
int mesh_handler_init(void);

/* Register a callback for a specific opcode (0xFFFF for all opcodes) */
int mesh_handler_register_callback(uint16_t opcode, ergo_msg_callback_t cb,
                                   void *user_data);

/* Send a message over BLE mesh */
int mesh_handler_send(uint16_t opcode, const uint8_t *data, uint16_t len,
                      uint16_t dst_addr);

/* Mesh model message handlers (called by Zephyr mesh stack) */
void mesh_handler_on_pressure_map(struct bt_mesh_model *model,
                                  struct bt_mesh_msg_ctx *ctx,
                                  struct net_buf_simple *buf);
void mesh_handler_on_imu_orientation(struct bt_mesh_model *model,
                                      struct bt_mesh_msg_ctx *ctx,
                                      struct net_buf_simple *buf);
void mesh_handler_on_heart_rate(struct bt_mesh_model *model,
                                 struct bt_mesh_msg_ctx *ctx,
                                 struct net_buf_simple *buf);
void mesh_handler_on_desk_command(struct bt_mesh_model *model,
                                  struct bt_mesh_msg_ctx *ctx,
                                  struct net_buf_simple *buf);
void mesh_handler_on_desk_status(struct bt_mesh_model *model,
                                  struct bt_mesh_msg_ctx *ctx,
                                  struct net_buf_simple *buf);
void mesh_handler_on_ambient_reading(struct bt_mesh_model *model,
                                      struct bt_mesh_msg_ctx *ctx,
                                      struct net_buf_simple *buf);
void mesh_handler_on_posture_score(struct bt_mesh_model *model,
                                    struct bt_mesh_msg_ctx *ctx,
                                    struct net_buf_simple *buf);
void mesh_handler_on_break_reminder(struct bt_mesh_model *model,
                                     struct bt_mesh_msg_ctx *ctx,
                                     struct net_buf_simple *buf);
void mesh_handler_on_lighting_cmd(struct bt_mesh_model *model,
                                   struct bt_mesh_msg_ctx *ctx,
                                   struct net_buf_simple *buf);
void mesh_handler_on_monitor_tilt(struct bt_mesh_model *model,
                                   struct bt_mesh_msg_ctx *ctx,
                                   struct net_buf_simple *buf);
void mesh_handler_on_ota_available(struct bt_mesh_model *model,
                                    struct bt_mesh_msg_ctx *ctx,
                                    struct net_buf_simple *buf);
void mesh_handler_on_ota_data(struct bt_mesh_model *model,
                               struct bt_mesh_msg_ctx *ctx,
                               struct net_buf_simple *buf);
void mesh_handler_on_node_heartbeat(struct bt_mesh_model *model,
                                     struct bt_mesh_msg_ctx *ctx,
                                     struct net_buf_simple *buf);
void mesh_handler_on_calibration(struct bt_mesh_model *model,
                                  struct bt_mesh_msg_ctx *ctx,
                                  struct net_buf_simple *buf);
void mesh_handler_on_factory_reset(struct bt_mesh_model *model,
                                    struct bt_mesh_msg_ctx *ctx,
                                    struct net_buf_simple *buf);

#endif /* MESH_HANDLER_H */