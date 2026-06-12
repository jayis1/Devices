/*
 * sensor_main.c — Aqua Guard Sensor Node (STM32L476RG)
 *
 * Submersible in-tank water quality monitor.
 * 8 Atlas Scientific EZO sensors via UART.
 * DS18B20 temperature via 1-Wire.
 * Turbidity via ADC.
 * Sub-GHz mesh client (SX1261).
 * Qi wireless charging.
 */

#include "stm32l4xx.h"
#include "stm32l4xx_hal.h"
#include <string.h>
#include <stdio.h>
#include <math.h>

#include "mesh_protocol.h"

/* ---- Configuration ---- */
#define NODE_ID         0x01   /* Configurable per node (01-07) */
#define READ_INTERVAL   60     /* Seconds between full sensor reads */
#define MESH_TX_SLOTS   1      /* Number of TX attempts per frame */

/* ---- EZO Sensor UART Interface ---- */

typedef struct {
    UART_HandleTypeDef *huart;
    char rx_buffer[64];
    uint8_t rx_idx;
    bool response_ready;
} ezo_sensor_t;

static ezo_sensor_t ezo_ph  = {&huart1, {0}, 0, false};  /* PA9/PA10 */
static ezo_sensor_t ezo_do  = {&huart2, {0}, 0, false};  /* PA2/PA3 */
static ezo_sensor_t ezo_ec  = {&huart3, {0}, 0, false};  /* PB10/PB11 */
static ezo_sensor_t ezo_nh3 = {&huart4, {0}, 0, false};  /* PC4/PC5 */
static ezo_sensor_t ezo_no2 = {&huart5, {0}, 0, false};  /* PD5/PD6 */
static ezo_sensor_t ezo_no3 = {&huart6, {0}, 0, false};  /* PE0/PE1 */

/* ---- Sensor Data ---- */
static sensor_data_payload_t current_data;
static bool data_ready = false;

/* ---- SX1261 Radio ---- */
static SPI_HandleTypeDef hspi1;  /* PB6=SCK, (MOSI/MISO on PB3/PB4 AF) */

static void sx1261_init(void)
{
    /* SPI1 for SX1261 — configure in HAL_MspInit */
    printf("[SX1261] Initialized\r\n");
    /* In production: set frequency, modulation, power, sync word */
}

static void sx1261_send(const uint8_t *data, uint16_t len)
{
    printf("[SX1261] TX %d bytes\r\n", len);
    /* In production: write FIFO, trigger TX, wait for TxDone */
}

/* ---- EZO Sensor Commands ---- */

static void ezo_send_cmd(ezo_sensor_t *sensor, const char *cmd)
{
    HAL_UART_Transmit(sensor->huart, (uint8_t *)cmd, strlen(cmd), 100);
    HAL_UART_Transmit(sensor->huart, (uint8_t *)"\r", 1, 100);
}

static float ezo_read_float(ezo_sensor_t *sensor)
{
    /* Atlas Scientific EZO response format: <status>?<value>\r */
    /* Wait 300ms-1000ms depending on command */
    HAL_Delay(600);

    /* Read response */
    uint8_t buf[32] = {0};
    HAL_UART_Receive(sensor->huart, buf, sizeof(buf) - 1, 1000);

    /* Parse: find '?' delimiter, then read float */
    for (int i = 0; i < 30; i++) {
        if (buf[i] == '?') {
            float val = 0.0f;
            if (sscanf((char *)&buf[i + 1], "%f", &val) == 1) {
                return val;
            }
        }
    }
    return -999.0f;  /* error sentinel */
}

/* ---- DS18B20 1-Wire Temperature (stub) ---- */

static float ds18b20_read(void)
{
    /* In production: bit-bang 1-Wire protocol on PA8 */
    /* Reset → Skip ROM → Convert T → Wait → Read Scratchpad */
    return current_data.temperature;  /* placeholder */
}

/* ---- Turbidity ADC ---- */

static float turbidity_read(void)
{
    ADC_ChannelConfTypeDef sConfig = {0};
    sConfig.Channel = ADC_CHANNEL_4;
    sConfig.Rank = 1;
    HAL_ADC_ConfigChannel(&hadc1, &sConfig);

    HAL_ADC_Start(&hadc1);
    HAL_ADC_PollForConversion(&hadc1, 100);
    uint32_t raw = HAL_ADC_GetValue(&hadc1);
    HAL_ADC_Stop(&hadc1);

    /* Convert ADC to NTU (calibration-dependent) */
    float voltage = (raw / 4096.0f) * 3.3f;
    float ntu = voltage * 1000.0f;  /* simplified calibration */
    return ntu;
}

/* ---- Read All Sensors ---- */

static void read_all_sensors(void)
{
    /* pH */
    ezo_send_cmd(&ezo_ph, "R");
    current_data.ph = ezo_read_float(&ezo_ph);

    /* Temperature (DS18B20) */
    current_data.temperature = ds18b20_read();

    /* Ammonia */
    ezo_send_cmd(&ezo_nh3, "R");
    current_data.ammonia = ezo_read_float(&ezo_nh3);

    /* Nitrite */
    ezo_send_cmd(&ezo_no2, "R");
    current_data.nitrite = ezo_read_float(&ezo_no2);

    /* Nitrate */
    ezo_send_cmd(&ezo_no3, "R");
    current_data.nitrate = ezo_read_float(&ezo_no3);

    /* Dissolved Oxygen */
    ezo_send_cmd(&ezo_do, "R");
    current_data.dissolved_o2 = ezo_read_float(&ezo_do);

    /* TDS/Conductivity */
    ezo_send_cmd(&ezo_ec, "R");
    current_data.tds = ezo_read_float(&ezo_ec);

    /* Turbidity */
    current_data.turbidity = turbidity_read();

    data_ready = true;
}

/* ---- Main ---- */

int main(void)
{
    HAL_Init();
    SystemClock_Config();

    /* Initialize UARTs, SPI, ADC, GPIO */
    /* (HAL_MspInit handles pin/clock configuration) */

    printf("=== Aqua Guard Sensor Node v1.0 ===\r\n");
    printf("Node ID: %d\r\n", NODE_ID);

    sx1261_init();

    uint32_t last_read = 0;
    uint32_t frame_count = 0;

    while (1) {
        uint32_t now = HAL_GetTick();

        /* Read sensors every READ_INTERVAL seconds */
        if (now - last_read >= READ_INTERVAL * 1000) {
            read_all_sensors();
            last_read = now;

            printf("pH:%.2f T:%.1f NH3:%.3f NO2:%.3f NO3:%.1f DO:%.1f TDS:%.0f NTU:%.0f\r\n",
                   current_data.ph, current_data.temperature, current_data.ammonia,
                   current_data.nitrite, current_data.nitrate, current_data.dissolved_o2,
                   current_data.tds, current_data.turbidity);
        }

        /* Transmit data in our TDMA slot (every 1 second frame) */
        if (data_ready) {
            mesh_packet_t tx_pkt;
            uint16_t pkt_len = mesh_build_packet(
                NODE_ID, NODE_ID_HUB, PKT_SENSOR_DATA,
                (uint8_t *)&current_data, sizeof(sensor_data_payload_t),
                &tx_pkt);
            sx1261_send((uint8_t *)&tx_pkt, pkt_len);
        }

        /* Deep sleep until next frame (TDMA timing by RTC) */
        HAL_Delay(TDMA_FRAME_MS);
        frame_count++;
    }
}