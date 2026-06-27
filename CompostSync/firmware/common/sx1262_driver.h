/*
 * SX1262 LoRa Radio Driver (HAL abstraction)
 * sx1262_driver.h
 */
#ifndef SX1262_DRIVER_H
#define SX1262_DRIVER_H

#include <stdint.h>
#include <stdbool.h>
#include "csp_protocol.h"

/* SX1262 register addresses (subset) */
#define SX1262_REG_PKT_TYPE      0x880
#define SX1262_REG_RF_FREQ       0x884
#define SX1262_REG_TX_PARAMS     0x895
#define SX1262_REG_MOD_CFG1      0x898
#define SX1262_REG_MOD_CFG2      0x899
#define SX1262_REG_PKT_PARAMS    0x8AF

/* Commands */
#define SX1262_CMD_SET_STANDBY   0x80
#define SX1262_CMD_SET_PKT_TYPE  0x8A
#define SX1262_CMD_SET_RF_FREQ   0x86
#define SX1262_CMD_SET_TX_PARAMS 0x8D
#define SX1262_CMD_SET_MOD_CFG   0x8B
#define SX1262_CMD_SET_PKT_PARAM 0x8C
#define SX1262_CMD_SET_TX        0x83
#define SX1262_CMD_SET_RX        0x82
#define SX1262_CMD_WRITE_BUF     0x0D
#define SX1262_CMD_READ_BUF      0x1D
#define SX1262_CMD_CLEAR_IRQ     0x02
#define SX1262_CMD_GET_IRQ       0x12
#define SX1262_CMD_GET_RX_STATUS 0x13
#define SX1262_CMD_SET_CAD       0x88
#define SX1262_CMD_SET_PA_CFG    0x95

/* Packet types */
#define SX1262_PKT_TYPE_LORA   0x01

/* IRQ flags */
#define SX1262_IRQ_TX_DONE     0x0001
#define SX1262_IRQ_RX_DONE     0x0002
#define SX1262_IRQ_PREAMBLE    0x0004
#define SX1262_IRQ_SYNC        0x0008
#define SX1262_IRQ_HEADER      0x0010
#define SX1262_IRQ_CRC_ERR     0x0020
#define SX1262_IRQ_CAD_DONE    0x0040
#define SX1262_IRQ_CAD_DETECTED 0x0080
#define SX1262_IRQ_TIMEOUT     0x0100

/* HAL interface — platform must implement these */
typedef struct {
    void (*spi_transfer)(uint8_t *tx, uint8_t *rx, size_t len);
    void (*cs_select)(void);
    void (*cs_deselect)(void);
    void (*reset)(bool state);   /* true = assert reset */
    void (*wait_busy)(void);     /* wait until BUSY pin is low */
    bool (*get_dio1)(void);      /* read DIO1 IRQ pin */
    uint32_t (*millis)(void);
} sx1262_hal_t;

typedef struct {
    sx1262_hal_t hal;
    uint8_t seq_num;
    uint8_t aes_key[AES_KEY_SIZE];
} sx1262_t;

int sx1262_init(sx1262_t *radio);
int sx1262_tx(sx1262_t *radio, uint16_t src, uint16_t dst,
              uint8_t msg_type, const uint8_t *payload, uint8_t len);
int sx1262_rx(sx1262_t *radio, uint32_t timeout_ms, csp_header_t *hdr,
              uint8_t *payload, uint8_t *payload_len);
int sx1262_set_frequency(sx1262_t *radio, uint32_t freq_hz);
void sx1262_set_key(sx1262_t *radio, const uint8_t *key);

#endif /* SX1262_DRIVER_H */