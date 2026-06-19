/*
 * rfid_pet.c — PawSync MFRC522 RFID pet identification
 *
 * Reads 13.56MHz RFID tags (ISO 14443A) to identify individual pets
 * in multi-pet households. Only dispenses food when the correct pet
 * is detected near the feeder.
 *
 * The MFRC522 communicates via SPI. Pet UIDs are registered during
 * the app's pairing process and stored in the feeder's flash.
 *
 * SPDX-License-Identifier: MIT
 */
#include <stdint.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_log.h"
#include "paw_protocol.h"

static const char *TAG = "rfid";

/* ---- MFRC522 registers ---- */
#define MFRC522_REG_VERSION   0x37
#define MFRC522_REG_COMMAND   0x01
#define MFRC522_REG_FIFO_DATA 0x09
#define MFRC522_REG_FIFO_LEVEL 0x0A
#define MFRC522_REG_BIT_FRAMING 0x0D
#define MFRC522_REG_TX_CONTROL 0x14
#define MFRC522_REG_COLL       0x0E

/* ---- MFRC522 commands ---- */
#define MFRC522_CMD_IDLE       0x00
#define MFRC522_CMD_TRANSMIT   0x03
#define MFRC522_CMD_RECEIVE    0x08
#define MFRC522_CMD_TRANSCEIVE 0x0C
#define MFRC522_CMD_SOFT_RESET 0x0F

/* ---- PICC commands (ISO 14443A) ---- */
#define PICC_CMD_REQA          0x26  /* REQA — Request Type A */
#define PICC_CMD_ANTICOLL      0x93  /* Anti-collision */

/* ---- SPI handle ---- */
static spi_device_handle_t spi;
static int cs_pin = -1;
static int rst_pin = -1;

/* ---- Registered pets ---- */
#define MAX_REGISTERED_PETS 4
typedef struct {
    uint8_t uid[10];
    uint8_t uid_len;
    uint8_t pet_id;
} registered_pet_t;

static registered_pet_t registered_pets[MAX_REGISTERED_PETS];
static int registered_count = 0;

/* ---- SPI write/register access ---- */
static void mfrc522_write_reg(uint8_t reg, uint8_t val)
{
    uint8_t tx[2] = { (reg << 1) & 0x7E, val };
    spi_transaction_t t = {
        .length = 16,
        .tx_buffer = tx,
    };
    gpio_set_level(cs_pin, 0);
    spi_device_polling_transmit(spi, &t);
    gpio_set_level(cs_pin, 1);
}

static uint8_t mfrc522_read_reg(uint8_t reg)
{
    uint8_t tx[2] = { ((reg << 1) & 0x7E) | 0x80, 0x00 };
    uint8_t rx[2] = {0};
    spi_transaction_t t = {
        .length = 16,
        .tx_buffer = tx,
        .rx_buffer = rx,
    };
    gpio_set_level(cs_pin, 0);
    spi_device_polling_transmit(spi, &t);
    gpio_set_level(cs_pin, 1);
    return rx[1];
}

/* ---- MFRC522 initialization ---- */
static void mfrc522_reset(void)
{
    mfrc522_write_reg(MFRC522_REG_COMMAND, MFRC522_CMD_SOFT_RESET);
    vTaskDelay(pdMS_TO_TICKS(50));
}

static void mfrc522_init(void)
{
    mfrc522_reset();

    /* Timer: TModeReg + TPrescalerReg */
    mfrc522_write_reg(0x2A, 0x8D);  /* TModeReg */
    mfrc522_write_reg(0x2B, 0x3E);  /* TPrescalerReg */
    mfrc522_write_reg(0x2D, 0x1E);  /* TReloadReg low */
    mfrc522_write_reg(0x2C, 0x00);  /* TReloadReg high */

    /* TX control: antenna on */
    mfrc522_write_reg(0x15, 0x40);  /* TxASKReg */
    mfrc522_write_reg(MFRC522_REG_TX_CONTROL, 0x83);  /* enable TX1+TX2 */

    /* Check version */
    uint8_t version = mfrc522_read_reg(MFRC522_REG_VERSION);
    ESP_LOGI(TAG, "MFRC522 version: 0x%02X", version);
}

/* ---- Detect card (REQA + anticollision) ---- */
static int mfrc522_request(uint8_t req_code, uint8_t *atqa)
{
    uint8_t tx_data[2] = { req_code, 0x00 };

    /* Clear FIFO */
    mfrc522_write_reg(MFRC522_REG_COMMAND, MFRC522_CMD_IDLE);
    mfrc522_write_reg(0x0A, 0x80);  /* flush FIFO */

    /* Write command to FIFO */
    for (int i = 0; i < 2; i++)
        mfrc522_write_reg(MFRC522_REG_FIFO_DATA, tx_data[i]);

    /* Set bit framing: 7 bits TX */
    mfrc522_write_reg(MFRC522_REG_BIT_FRAMING, 0x07);

    /* Execute transceive */
    mfrc522_write_reg(MFRC522_REG_COMMAND, MFRC522_CMD_TRANSCEIVE);
    vTaskDelay(pdMS_TO_TICKS(10));

    /* Check result */
    uint8_t fifo_level = mfrc522_read_reg(MFRC522_REG_FIFO_LEVEL);
    if (fifo_level > 0 && atqa) {
        atqa[0] = mfrc522_read_reg(MFRC522_REG_FIFO_DATA);
        return 1;
    }
    return 0;
}

static int mfrc522_anticoll(uint8_t *uid, int max_len)
{
    mfrc522_write_reg(MFRC522_REG_COMMAND, MFRC522_CMD_IDLE);
    mfrc522_write_reg(0x0A, 0x80);  /* flush FIFO */

    /* Anti-collision command */
    mfrc522_write_reg(MFRC522_REG_FIFO_DATA, PICC_CMD_ANTICOLL);
    mfrc522_write_reg(MFRC522_REG_FIFO_DATA, 0x20);

    mfrc522_write_reg(MFRC522_REG_BIT_FRAMING, 0x00);
    mfrc522_write_reg(MFRC522_REG_COMMAND, MFRC522_CMD_TRANSCEIVE);
    vTaskDelay(pdMS_TO_TICKS(10));

    uint8_t fifo_level = mfrc522_read_reg(MFRC522_REG_FIFO_LEVEL);
    if (fifo_level >= 5 && max_len >= 5) {
        for (int i = 0; i < 5; i++)
            uid[i] = mfrc522_read_reg(MFRC522_REG_FIFO_DATA);
        /* Verify BCC (checksum) */
        if ((uid[0] ^ uid[1] ^ uid[2] ^ uid[3]) == uid[4])
            return 5;  /* 4-byte UID + BCC */
    }
    return 0;
}

/* ---- Public API ---- */
void rfid_init(int sck, int miso, int mosi, int cs, int rst)
{
    cs_pin = cs;
    rst_pin = rst;

    /* Init CS and RST pins */
    gpio_set_direction(cs, GPIO_MODE_OUTPUT);
    gpio_set_direction(rst, GPIO_MODE_OUTPUT);
    gpio_set_level(cs, 1);
    gpio_set_level(rst, 1);
    vTaskDelay(pdMS_TO_TICKS(50));
    gpio_set_level(rst, 0);
    vTaskDelay(pdMS_TO_TICKS(50));
    gpio_set_level(rst, 1);
    vTaskDelay(pdMS_TO_TICKS(50));

    /* Init SPI bus */
    spi_bus_config_t buscfg = {
        .mosi_io_num = mosi,
        .miso_io_num = miso,
        .sclk_io_num = sck,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
    };
    spi_bus_initialize(SPI2_HOST, &buscfg, 0);

    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = 5000000,  /* 5MHz */
        .mode = 0,
        .spics_io_num = -1,  /* manual CS control */
        .queue_size = 1,
    };
    spi_bus_add_device(SPI2_HOST, &devcfg, &spi);

    mfrc522_init();
    ESP_LOGI(TAG, "MFRC522 initialized");
}

int rfid_read_uid(uint8_t *uid, int max_len)
{
    uint8_t atqa[2] = {0};
    if (mfrc522_request(PICC_CMD_REQA, atqa)) {
        /* Card detected — get UID */
        int uid_len = mfrc522_anticoll(uid, max_len);
        if (uid_len > 0) {
            ESP_LOGI(TAG, "RFID UID: %02X%02X%02X%02X",
                     uid[0], uid[1], uid[2], uid[3]);
            return uid_len;
        }
    }
    return 0;
}

int rfid_find_pet(const uint8_t *uid, int uid_len)
{
    for (int i = 0; i < registered_count; i++) {
        if (registered_pets[i].uid_len == uid_len &&
            memcmp(registered_pets[i].uid, uid, uid_len) == 0) {
            ESP_LOGI(TAG, "Pet identified: ID %u", registered_pets[i].pet_id);
            return registered_pets[i].pet_id;
        }
    }
    ESP_LOGW(TAG, "Unknown RFID tag");
    return -1;
}

/* ---- Register a new pet ---- */
int rfid_register_pet(const uint8_t *uid, int uid_len, uint8_t pet_id)
{
    if (registered_count >= MAX_REGISTERED_PETS) return -1;
    if (uid_len > 10) return -1;

    memcpy(registered_pets[registered_count].uid, uid, uid_len);
    registered_pets[registered_count].uid_len = uid_len;
    registered_pets[registered_count].pet_id = pet_id;
    registered_count++;

    ESP_LOGI(TAG, "Registered pet %u with UID %02X%02X%02X%02X",
             pet_id, uid[0], uid[1], uid[2], uid[3]);
    return 0;
}