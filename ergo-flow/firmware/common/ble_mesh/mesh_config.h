/*
 * ErgoFlow — BLE Mesh Network Configuration
 * Common header for all mesh nodes
 *
 * Copyright (c) 2026 jayis1. MIT License.
 */

#ifndef MESH_CONFIG_H
#define MESH_CONFIG_H

#include <stdint.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/mesh.h>

/* Network configuration */
#define ERGO_MESH_NET_IDX        0x0000
#define ERGO_MESH_APP_IDX_CTRL   0x0000   /* Control messages */
#define ERGO_MESH_APP_IDX_TELEM  0x0001   /* Telemetry messages */

/* Node addresses */
#define ERGO_ADDR_HUB            0x0001
#define ERGO_ADDR_CHAIR          0x0002
#define ERGO_ADDR_DESK           0x0003
#define ERGO_ADDR_TAG_1          0x0004
#define ERGO_ADDR_TAG_2          0x0005

/* Mesh parameters */
#define ERGO_MESH_TTL            7
#define ERGO_MESH_RETRANSMIT_CNT 3
#define ERGO_MESH_RETRANSMIT_INT 100     /* ms */

/* Provisioning */
#define ERGO_OOB_TYPE            BT_MESH_OOB_TYPE_NUMBER
#define ERGO_OOB_SIZE            4        /* 4-digit numeric */

/* Message opcodes */
#define ERGO_OP_PRESSURE_MAP    0xC001
#define ERGO_OP_IMU_ORIENTATION 0xC002
#define ERGO_OP_HEART_RATE      0xC003
#define ERGO_OP_DESK_COMMAND    0xC004
#define ERGO_OP_DESK_STATUS     0xC005
#define ERGO_OP_AMBIENT_READING 0xC006
#define ERGO_OP_POSTURE_SCORE   0xC007
#define ERGO_OP_BREAK_REMINDER  0xC008
#define ERGO_OP_LIGHTING_CMD    0xC009
#define ERGO_OP_MONITOR_TILT    0xC00A
#define ERGO_OP_OTA_AVAILABLE   0xC00B
#define ERGO_OP_OTA_DATA        0xC00C
#define ERGO_OP_NODE_HEARTBEAT  0xC00D
#define ERGO_OP_CALIBRATION     0xC00E
#define ERGO_OP_FACTORY_RESET   0xC00F

/* Transmission intervals */
#define ERGO_TX_INTERVAL_PRESSURE  500    /* ms - chair pad */
#define ERGO_TX_INTERVAL_IMU       500    /* ms - wearable tag */
#define ERGO_TX_INTERVAL_HR        60000  /* ms - wearable tag */
#define ERGO_TX_INTERVAL_DESK      2000   /* ms - desk controller */
#define ERGO_TX_INTERVAL_AMBIENT   5000   /* ms - hub */
#define ERGO_TX_INTERVAL_HEARTBEAT 60000 /* ms - all nodes */

/* Node states */
#define ERGO_STATE_INIT           0
#define ERGO_STATE_PAIRING        1
#define ERGO_STATE_CALIBRATING    2
#define ERGO_STATE_RUNNING        3
#define ERGO_STATE_LOW_POWER      4
#define ERGO_STATE_ERROR          5

/* Posture classes */
#define ERGO_POSTURE_GOOD         0
#define ERGO_POSTURE_SLOUCH       1
#define ERGO_POSTURE_LEAN_LEFT    2
#define ERGO_POSTURE_LEAN_RIGHT   3
#define ERGO_POSTURE_HUNCH        4

/* Activity classes */
#define ERGO_ACTIVITY_TYPING       0
#define ERGO_ACTIVITY_MOUSE        1
#define ERGO_ACTIVITY_PHONE        2
#define ERGO_ACTIVITY_IDLE         3
#define ERGO_ACTIVITY_STRETCH      4
#define ERGO_ACTIVITY_WALK         5

/* Break types */
#define ERGO_BREAK_STRETCH         0
#define ERGO_BREAK_WALK            1
#define ERGO_BREAK_LOOK_AWAY       2

/* Desk commands */
#define ERGO_DESK_CMD_HEIGHT       0x01
#define ERGO_DESK_CMD_PRESET       0x02
#define ERGO_DESK_CMD_STOP         0x03

/* Desk presets */
#define ERGO_DESK_PRESET_SIT       0x01
#define ERGO_DESK_PRESET_STAND     0x02
#define ERGO_DESK_PRESET_CUSTOM    0x03

#endif /* MESH_CONFIG_H */