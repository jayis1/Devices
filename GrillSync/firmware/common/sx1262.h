/*
 * GrillSync — SX1262 Sub-GHz Radio Driver Header
 */
#ifndef GRILLSYNC_SX1262_H
#define GRILLSYNC_SX1262_H

#include <stdint.h>
#include <stddef.h>

/* SX1262 register addresses */
#define SX1262_REG_WHITENING_INIT_MSB   0x02
#define SX1262_REG_WHITENING_INIT_LSB   0x03
#define SX1262_REG_CRC_INITIAL_MSB      0x04
#define SX1262_REG_CRC_INITIAL_LSB      0x05
#define SX1262_REG_SYNC_WORD_0          0x06
#define SX1262_REG_SYNC_WORD_1          0x07
#define SX1262_REG_SYNC_WORD_2          0x08
#define SX1262_REG_SYNC_WORD_3          0x09
#define SX1262_REG_SYNC_WORD_4          0x0A
#define SX1262_REG_SYNC_WORD_5          0x0B
#define SX1262_REG_SYNC_WORD_6          0x0C
#define SX1262_REG_SYNC_WORD_7          0x0D

/* SX1262 commands */
#define SX1262_CMD_SET_SLEEP              0x84
#define SX1262_CMD_SET_STANDBY            0x80
#define SX1262_CMD_SET_FS                 0xC0
#define SX1262_CMD_SET_TX                  0x83
#define SX1262_CMD_SET_RX                  0x82
#define SX1262_CMD_STOP_TIMER_ON_PREAMBLE 0x9F
#define SX1262_CMD_SET_RX_DUTY_CYCLE      0x94
#define SX1262_CMD_SET_CAD                 0xC5
#define SX1262_CMD_SET_TX_CONTINUOUS_WAVE 0xD1
#define SX1262_CMD_SET_TX_INFINITE_PREAMBLE 0xD2
#define SX1262_CMD_SET_REGULATOR_MODE     0x96
#define SX1262_CMD_CALIBRATE               0x89
#define SX1262_CMD_CALIBRATE_IMAGE         0x98
#define SX1262_CMD_SET_PA_CONFIG           0x95
#define SX1262_CMD_SET_RX_TX_FALLBACK_MODE 0x93
#define SX1262_CMD_WRITE_REGISTER          0x0D
#define SX1262_CMD_READ_REGISTER           0x1D
#define SX1262_CMD_WRITE_BUFFER           0x0E
#define SX1262_CMD_READ_BUFFER             0x1E
#define SX1262_CMD_SET_DIO_IRQ_PARAMS     0x08
#define SX1262_CMD_GET_IRQ_STATUS          0x12
#define SX1262_CMD_CLEAR_IRQ_STATUS        0x02
#define SX1262_CMD_SET_DIO2_AS_RF_SWITCH   0x9D
#define SX1262_CMD_SET_DIO3_AS_TCXO_CTRL  0x97
#define SX1262_CMD_SET_RF_FREQUENCY        0x86
#define SX1262_CMD_SET_PACKET_TYPE         0x8A
#define SX1262_CMD_GET_PACKET_TYPE         0x1A
#define SX1262_CMD_SET_TX_PARAMS          0x8E
#define SX1262_CMD_SET_MODULATION_PARAMS   0x8B
#define SX1262_CMD_SET_PACKET_PARAMS      0x8C
#define SX1262_CMD_SET_CAD_PARAMS          0x88
#define SX1262_CMD_SET_BUFFER_BASE_ADDR   0x8F
#define SX1262_CMD_SET_LORA_SYMB_TIMEOUT  0x9B
#define SX1262_CMD_GET_STATUS              0x22
#define SX1262_CMD_GET_RX_BUFFER_STATUS    0x13
#define SX1262_CMD_GET_PACKET_STATUS      0x14
#define SX1262_CMD_GET_RSSI_INST           0x15
#define SX1262_CMD_GET_STATS                0x16

/* IRQ masks */
#define SX1262_IRQ_TX_DONE      0x0001
#define SX1262_IRQ_RX_DONE      0x0002
#define SX1262_IRQ_PREAMBLE_DET 0x0004
#define SX1262_IRQ_SYNC_DET    0x0008
#define SX1262_IRQ_HEADER_DET  0x0010
#define SX1262_IRQ_CRC_ERR      0x0020
#define SX1262_IRQ_CAD_DONE     0x0040
#define SX1262_IRQ_CAD_DET     0x0080
#define SX1262_IRQ_TIMEOUT      0x0100
#define SX1262_IRQ_ALL          0x03FF

/* Packet types */
#define SX1262_PKT_TYPE_LORA    0x01
#define SX1262_PKT_TYPE_FSK     0x02

/* Standby modes */
#define SX1262_STDBY_RC   0x00
#define SX1262_STDBY_XOSC 0x01

/* SPI interface abstraction */
typedef struct {
    void (*init)(void);
    void (*cs_select)(void);
    void (*cs_release)(void);
    uint8_t (*transfer)(uint8_t byte);
    void (*reset)(uint8_t assert);
    void (*delay_ms)(uint32_t ms);
    int  (*dio1_read)(void);
    void (*dio1_irq_enable)(int enable);
} gs_spi_interface_t;

/* Radio configuration */
typedef struct {
    uint32_t frequency;
    uint32_t bandwidth;
    uint8_t  spreading_factor;
    uint8_t  coding_rate;
    uint8_t  preamble_len;
    int8_t   tx_power_dbm;
    uint32_t rx_timeout_ms;
} gs_radio_config_t;

/* Radio context */
typedef struct {
    const gs_spi_interface_t *spi;
    gs_radio_config_t config;
    uint8_t dio1_irq_status;
} gs_radio_ctx_t;

/* API */
int gs_radio_init(gs_radio_ctx_t *ctx, const gs_spi_interface_t *spi,
                  const gs_radio_config_t *config);
int gs_radio_tx(gs_radio_ctx_t *ctx, const uint8_t *data, uint8_t len);
int gs_radio_rx(gs_radio_ctx_t *ctx, uint8_t *data, uint8_t max_len,
                uint32_t timeout_ms);
int gs_radio_set_tx_power(gs_radio_ctx_t *ctx, int8_t power_dbm);
int gs_radio_set_frequency(gs_radio_ctx_t *ctx, uint32_t freq_hz);
int8_t gs_radio_get_rssi(gs_radio_ctx_t *ctx);
void gs_radio_reset(gs_radio_ctx_t *ctx);

#endif /* GRILLSYNC_SX1262_H */