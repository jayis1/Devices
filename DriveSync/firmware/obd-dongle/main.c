/**
 * DriveSync OBD-II Dongle — Main Firmware
 *
 * RP2040 + MCP2515 CAN controller + nRF52832 BLE bridge
 * Plugs into vehicle OBD-II port. Reads vehicle telemetry at 10 Hz.
 * Sends data to Hub via BLE (through nRF52832 UART bridge).
 *
 * License: MIT
 */

#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "pico/binary_info.h"
#include "hardware/spi.h"
#include "hardware/uart.h"
#include "hardware/timer.h"
#include "hardware/irq.h"
#include "pico/multicore.h"

#include "protocol.h"
#include "can_driver.h"
#include "ble_uart.h"

/* ── OBD-II PIDs ─────────────────────────────────────────────────── */

#define PID_SUPPORTED          0x00
#define PID_ENGINE_RPM         0x0C
#define PID_VEHICLE_SPEED      0x0D
#define PID_THROTTLE_POS       0x11
#define PID_ENGINE_LOAD        0x04
#define PID_COOLANT_TEMP       0x05
#define PID_FUEL_LEVEL         0x2F

/* ── State ───────────────────────────────────────────────────────── */

typedef struct {
    uint16_t speed_kmh;
    uint16_t rpm;
    uint8_t  throttle_pct;
    uint8_t  engine_load;
    int16_t  coolant_temp_c;
    uint8_t  fuel_level;
    uint8_t  pid_flags;
    uint16_t seq_counter;
} obd_state_t;

static obd_state_t g_state = {0};

/* ── OBD-II PID Query ────────────────────────────────────────────── */

/**
 * Send OBD-II PID request via CAN and parse response.
 * Returns true if response received.
 */
static bool obd_query_pid(uint8_t pid, uint8_t *response, uint8_t *response_len)
{
    /* Build OBD-II request CAN frame:
     * CAN ID: 0x7DF (broadcast to all ECUs)
     * Data: [0x02, 0x01, PID, 0x00, 0x00, 0x00, 0x00, 0x00]
     */
    uint8_t request[8] = {0x02, 0x01, pid, 0x00, 0x00, 0x00, 0x00, 0x00};
    can_frame_t frame = {
        .id = 0x7DF,
        .dlc = 8,
    };
    memcpy(frame.data, request, 8);

    can_send(&frame);

    /* Wait for response (100 ms timeout) */
    can_frame_t response_frame;
    uint32_t start = time_us_32();
    while (time_us_32() - start < 100000) {
        if (can_receive(&response_frame)) {
            /* Parse response: check if it matches our PID */
            if (response_frame.data[1] == 0x41 && response_frame.data[2] == pid) {
                uint8_t num_bytes = response_frame.data[0] - 2;
                if (num_bytes > 0 && num_bytes <= 6) {
                    memcpy(response, &response_frame.data[3], num_bytes);
                    *response_len = num_bytes;
                    return true;
                }
            }
        }
    }
    return false;
}

/* ── Query All PIDs (10 Hz) ───────────────────────────────────────── */

static void query_all_pids(void)
{
    uint8_t response[8];
    uint8_t resp_len;

    /* Engine RPM (PID 0x0C) — A*256 + B, / 4 */
    if (obd_query_pid(PID_ENGINE_RPM, response, &resp_len) && resp_len >= 2) {
        g_state.rpm = ((uint16_t)response[0] * 256 + response[1]) / 4;
    }

    /* Vehicle speed (PID 0x0D) — A km/h */
    if (obd_query_pid(PID_VEHICLE_SPEED, response, &resp_len) && resp_len >= 1) {
        g_state.speed_kmh = response[0];
    }

    /* Throttle position (PID 0x11) — A*100/255 % */
    if (obd_query_pid(PID_THROTTLE_POS, response, &resp_len) && resp_len >= 1) {
        g_state.throttle_pct = (uint8_t)((uint16_t)response[0] * 100 / 255);
    }

    /* Engine load (PID 0x04) — A*100/255 % */
    if (obd_query_pid(PID_ENGINE_LOAD, response, &resp_len) && resp_len >= 1) {
        g_state.engine_load = (uint8_t)((uint16_t)response[0] * 100 / 255);
    }

    /* Coolant temp (PID 0x05) — A - 40 °C */
    if (obd_query_pid(PID_COOLANT_TEMP, response, &resp_len) && resp_len >= 1) {
        g_state.coolant_temp_c = (int16_t)(response[0] - 40) * 100;  /* centi-degrees */
    }

    /* Fuel level (PID 0x2F) — A*100/255 % */
    if (obd_query_pid(PID_FUEL_LEVEL, response, &resp_len) && resp_len >= 1) {
        g_state.fuel_level = (uint8_t)((uint16_t)response[0] * 100 / 255);
        g_state.pid_flags |= 0x20;
    }
}

/* ── Send Payload to Hub (via BLE UART bridge) ─────────────────────── */

static void send_to_hub(void)
{
    payload_obd_t payload = {0};
    payload.speed_kmh = g_state.speed_kmh;
    payload.rpm = g_state.rpm;
    payload.throttle_pct = g_state.throttle_pct;
    payload.engine_load = g_state.engine_load;
    payload.coolant_temp_c = g_state.coolant_temp_c;
    payload.fuel_level = g_state.fuel_level;
    payload.obd_pid_flags = g_state.pid_flags;
    payload.timestamp = time_us_32() / 1000;

    uint8_t packet[DS_MAX_PACKET_LEN];
    uint8_t len = drivesync_encode(packet, sizeof(packet),
                                    MSG_TYPE_DATA_OBD,
                                    DS_OBD_ID_BASE,  /* 0x0300 */
                                    g_state.seq_counter++,
                                    0,
                                    (uint8_t *)&payload, sizeof(payload));

    ble_uart_send(packet, len);
}

/* ── BLE Command Handler ──────────────────────────────────────────── */

static void ble_cmd_handler(const uint8_t *data, uint8_t len)
{
    drivesync_header_t header;
    const uint8_t *payload;

    if (!drivesync_decode(data, len, &header, &payload)) return;

    switch (header.msg_type) {
    case MSG_TYPE_CMD_MODE:
        /* Handle mode change (e.g., sleep when engine off) */
        break;

    case MSG_TYPE_HEARTBEAT:
        break;

    default:
        break;
    }
}

/* ── Core 1: BLE UART bridge ──────────────────────────────────────── */

static void core1_entry(void)
{
    ble_uart_init(ble_cmd_handler);

    while (true) {
        ble_uart_process();
        sleep_ms(1);
    }
}

/* ── Main (Core 0: OBD-II polling) ────────────────────────────────── */

int main(void)
{
    stdio_init_all();

    printf("DriveSync OBD-II Dongle starting...\r\n");

    /* Initialize CAN controller (MCP2515 on SPI0) */
    can_init();

    /* Initialize state */
    g_state.seq_counter = 0;
    g_state.pid_flags = 0;

    /* Check which PIDs are supported */
    uint8_t response[8];
    uint8_t resp_len;
    if (obd_query_pid(PID_SUPPORTED, response, &resp_len) && resp_len >= 4) {
        g_state.pid_flags = response[0];  /* First 8 PIDs */
    }

    /* Launch Core 1 for BLE UART bridge */
    multicore_launch_core1(core1_entry);

    printf("OBD-II Dongle ready\r\n");

    /* Main loop: query PIDs at 10 Hz and send to Hub */
    uint32_t last_query = 0;
    while (true) {
        uint32_t now = time_us_32() / 1000;  /* ms */

        if (now - last_query >= 100) {  /* 10 Hz */
            query_all_pids();
            send_to_hub();
            last_query = now;

            printf("Speed: %d km/h, RPM: %d, Throttle: %d%%\r\n",
                   g_state.speed_kmh, g_state.rpm, g_state.throttle_pct);
        }

        sleep_ms(10);
    }
}