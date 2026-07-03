/**
 * DriveSync BLE UART Bridge — RP2040 to nRF52832
 *
 * The nRF52832 module runs a transparent UART-to-BLE bridge.
 * RP2040 sends/receives DriveSync protocol packets over UART.
 *
 * UART frame format:
 *   [START 0xAA][LEN][DATA...][CRC8]
 *
 * License: MIT
 */

#include "ble_uart.h"
#include "pico/stdlib.h"
#include "hardware/uart.h"
#include "crc8.h"
#include <string.h>

#define UART_ID    uart0
#define BAUD_RATE  115200
#define UART_TX_PIN 8
#define UART_RX_PIN 9

#define FRAME_START  0xAA
#define RX_BUF_SIZE  256

static ble_cmd_cb_t s_cmd_handler = NULL;
static bool s_connected = false;

static uint8_t s_rx_buffer[RX_BUF_SIZE];
static uint16_t s_rx_idx = 0;

void ble_uart_init(ble_cmd_cb_t cmd_handler)
{
    s_cmd_handler = cmd_handler;

    uart_init(UART_ID, BAUD_RATE);
    gpio_set_function(UART_TX_PIN, GPIO_FUNC_UART);
    gpio_set_function(UART_RX_PIN, GPIO_FUNC_UART);

    uart_set_hw_flow(UART_ID, false);
    uart_set_format(UART_ID, 8, 1, UART_PARITY_NONE);
    uart_set_fifo_enabled(UART_ID, true);

    /* Enable RX interrupt */
    uart_set_irq_enables(UART_ID, true, false);
}

void ble_uart_send(const uint8_t *data, uint8_t len)
{
    if (data == NULL || len == 0) return;

    /* Frame: [START][LEN][DATA...][CRC8] */
    uart_putc_raw(UART_ID, FRAME_START);
    uart_putc_raw(UART_ID, len);

    for (uint8_t i = 0; i < len; i++) {
        uart_putc_raw(UART_ID, data[i]);
    }

    uint8_t crc = crc8_compute(data, len);
    uart_putc_raw(UART_ID, crc);
}

void ble_uart_process(void)
{
    /* Read available UART data */
    while (uart_is_readable(UART_ID)) {
        uint8_t byte = uart_getc(UART_ID);

        if (s_rx_idx == 0 && byte == FRAME_START) {
            s_rx_idx = 1;
            continue;
        }

        if (s_rx_idx == 1) {
            /* This is the length byte */
            if (byte > DS_MAX_PACKET_LEN) {
                s_rx_idx = 0;  /* Invalid, reset */
                continue;
            }
            s_rx_buffer[0] = byte;  /* Store length */
            s_rx_idx = 2;
            continue;
        }

        if (s_rx_idx >= 2) {
            /* Data bytes */
            uint8_t expected_len = s_rx_buffer[0];
            if (s_rx_idx - 2 < expected_len) {
                s_rx_buffer[s_rx_idx - 1] = byte;
                s_rx_idx++;
            } else {
                /* This is the CRC byte */
                uint8_t received_crc = byte;
                uint8_t computed_crc = crc8_compute(&s_rx_buffer[1], expected_len);

                if (received_crc == computed_crc && s_cmd_handler) {
                    s_cmd_handler(&s_rx_buffer[1], expected_len);
                }
                s_rx_idx = 0;  /* Reset for next frame */
            }
        }
    }

    /* Check connection status (via GPIO from nRF52832) */
    /* For stub: assume connected after init */
    s_connected = true;
}

bool ble_uart_is_connected(void)
{
    return s_connected;
}