/**
 * FreshKeep — Fridge Node Firmware (STM32L476RG)
 * 
 * In-fridge monitoring: dual cameras, gas sensors (VOC, CO2, ethylene),
 * weight shelves, temperature/humidity. Detects spoilage, tracks inventory,
 * captures images on door-open events.
 * 
 * TDMA mesh: transmits in Slot 1 (100ms window every 500ms)
 */

#include <stdio.h>
#include <string.h>
#include "stm32l4xx.h"
#include "stm32l4xx_hal.h"

/* ── Pin Definitions (STM32L476RG) ────────────────────────────────── */
#define PIN_CAM1_EN       GPIO_PIN_1     /* PA1 — Camera 1 power enable */
#define PIN_CAM2_EN       GPIO_PIN_0     /* PB0 — Camera 2 power enable */
#define PIN_LED_STRIP     GPIO_PIN_1     /* PB1 — Fridge interior LED PWM */
#define PIN_QI_CHG        GPIO_PIN_6     /* PC6 — Qi charging status */
#define PIN_VBAT_SENSE    GPIO_PIN_0     /* PB0 — Battery voltage ADC */

/* ── I2C Devices ───────────────────────────────────────────────────── */
#define SGP40_ADDR        0x59
#define SHT40_ADDR        0x44
#define SX1261_ADDR       0x00  /* SPI device */

/* ── SCD30 CO2 Sensor (UART) ──────────────────────────────────────── */
#define SCD30_UART        UART2
#define SCD30_BAUD        19200

/* ── Constants ──────────────────────────────────────────────────────── */
#define READ_INTERVAL_MS  60000   /* Read all sensors every 60 seconds */
#define DOOR_OPEN_BURST   5       /* Capture 5 photos on door open */
#define TX_SLOT_MS        100     /* TDMA slot = 100ms */
#define FRAME_MS          500     /* TDMA frame = 500ms */
#define SPOILAGE_THRESHOLD 80     /* Trigger alert at 80% spoilage */
#define MAX_WEIGHT_KG     5.0f    /* Max weight per shelf */

/* ── Sensor Data ───────────────────────────────────────────────────── */
typedef struct {
    /* Gas sensors */
    uint16_t voc_index;       /* SGP40: 0-500 VOC index */
    uint16_t co2_ppm;        /* SCD30: 400-10000 ppm */
    uint16_t ethylene_raw;   /* MQ-3: ADC raw (0-4095) */
    
    /* Temp/humidity */
    int16_t  temp_c_x10;     /* SHT40: temperature × 10 */
    uint16_t humidity_x10;   /* SHT40: humidity × 10 */
    
    /* Weight sensors */
    uint32_t weight_mg[4];   /* 4 shelves, milligrams */
    
    /* Environment */
    uint8_t  door_state;     /* 0=closed, 1=open (from light sensor) */
    uint16_t light_lux_x10;  /* TSL2591: ambient light × 10 */
    
    /* Derived */
    uint8_t  spoilage_score; /* 0-100, computed from gas + temp + time */
    uint8_t  image_ready;    /* 0=no new image, 1=new image available */
    uint8_t  battery_pct;
    
    /* Internal state */
    uint32_t last_reading_ms;
    uint32_t door_open_count;
    uint8_t  camera_buf[320*240*2]; /* QVGA RGB565 image buffer */
} fridge_sensor_t;

static fridge_sensor_t g_sensor;

/* ── HX711 Load Cell Interface ─────────────────────────────────────── */
static uint32_t hx711_read(uint8_t clk_pin_port, uint16_t clk_pin,
                            uint8_t dat_pin_port, uint16_t dat_pin) {
    uint32_t value = 0;
    
    /* Wait for data ready (DOUT goes low) */
    uint32_t timeout = HAL_GetTick() + 100;
    while (HAL_GPIO_ReadPin((GPIO_TypeDef*)dat_pin_port, dat_pin)) {
        if (HAL_GetTick() > timeout) return 0;
    }
    
    /* Pulse CLK 24 times to read 24 bits */
    for (int i = 0; i < 24; i++) {
        HAL_GPIO_WritePin((GPIO_TypeDef*)clk_pin_port, clk_pin, GPIO_PIN_SET);
        /* ~1us delay */
        for (volatile int d = 0; d < 10; d++);
        HAL_GPIO_WritePin((GPIO_TypeDef*)clk_pin_port, clk_pin, GPIO_PIN_RESET);
        value = (value << 1) | HAL_GPIO_ReadPin((GPIO_TypeDef*)dat_pin_port, dat_pin);
    }
    
    /* 25th pulse sets gain (128 for channel A) */
    HAL_GPIO_WritePin((GPIO_TypeDef*)clk_pin_port, clk_pin, GPIO_PIN_SET);
    for (volatile int d = 0; d < 10; d++);
    HAL_GPIO_WritePin((GPIO_TypeDef*)clk_pin_port, clk_pin, GPIO_PIN_RESET);
    
    return value;
}

/* ── SGP40 VOC Sensor ──────────────────────────────────────────────── */
static HAL_StatusTypeDef sgp40_measure(I2C_HandleTypeDef *hi2c, uint16_t *voc_index) {
    uint8_t cmd[2] = {0x26, 0x0B}; /* Measure raw signal */
    HAL_StatusTypeDef status;
    
    /* Send measure command */
    status = HAL_I2C_Master_Transmit(hi2c, SGP40_ADDR << 1, cmd, 2, 100);
    if (status != HAL_OK) return status;
    
    /* Wait for measurement (50ms max) */
    HAL_Delay(50);
    
    /* Read result: 2 data bytes + 1 CRC per word */
    uint8_t data[3];
    status = HAL_I2C_Master_Receive(hi2c, SGP40_ADDR << 1, data, 3, 100);
    if (status != HAL_OK) return status;
    
    *voc_index = (data[0] << 8) | data[1];
    return HAL_OK;
}

/* ── SHT40 Temp/Humidity ───────────────────────────────────────────── */
static HAL_StatusTypeDef sht40_measure(I2C_HandleTypeDef *hi2c,
                                         int16_t *temp_c_x10,
                                         uint16_t *humidity_x10) {
    uint8_t cmd[2] = {0xFD, 0x00}; /* High precision measurement */
    HAL_StatusTypeDef status;
    
    status = HAL_I2C_Master_Transmit(hi2c, SHT40_ADDR << 1, cmd, 2, 100);
    if (status != HAL_OK) return status;
    
    HAL_Delay(10);
    
    uint8_t data[6];
    status = HAL_I2C_Master_Receive(hi2c, SHT40_ADDR << 1, data, 6, 100);
    if (status != HAL_OK) return status;
    
    /* Convert raw to temperature and humidity */
    uint16_t temp_raw = (data[0] << 8) | data[1];
    uint16_t humid_raw = (data[3] << 8) | data[4];
    
    *temp_c_x10 = (int16_t)((175.0f * temp_raw / 65535.0f - 45.0f) * 10);
    *humidity_x10 = (uint16_t)(1000.0f * humid_raw / 65535.0f); /* ×10% */
    
    return HAL_OK;
}

/* ── SCD30 CO2 Sensor (UART) ───────────────────────────────────────── */
static HAL_StatusTypeDef scd30_read_co2(UART_HandleTypeDef *huart, uint16_t *co2_ppm) {
    uint8_t cmd[] = {0x61, 0x06, 0x00, 0x03, 0xF0, 0x44}; /* Read measurement */
    uint8_t response[18];
    
    HAL_UART_Transmit(huart, cmd, sizeof(cmd), 100);
    HAL_Delay(100);
    HAL_UART_Receive(huart, response, 18, 200);
    
    /* CO2 is in bytes 0-3 as IEEE 754 float */
    uint8_t co2_bytes[4] = {response[0], response[1], response[2], response[3]};
    float co2_float;
    memcpy(&co2_float, co2_bytes, 4);
    *co2_ppm = (uint16_t)co2_float;
    
    return HAL_OK;
}

/* ── Spoilage Score Calculator ──────────────────────────────────────── */
static uint8_t calculate_spoilage(const fridge_sensor_t *s) {
    /* Multi-factor spoilage estimation:
     * - VOC index (higher = more spoilage gases)
     * - CO2 (higher = more microbial respiration)
     * - Ethylene (higher = more fruit ripening)
     * - Temperature (higher = faster spoilage)
     * - Time since last door open (longer = more gas buildup)
     */
    float score = 0.0f;
    
    /* VOC contribution (0-30 points) */
    if (s->voc_index > 300) score += 30.0f;
    else if (s->voc_index > 200) score += 20.0f;
    else if (s->voc_index > 100) score += 10.0f;
    else score += s->voc_index / 10.0f;
    
    /* CO2 contribution (0-25 points) */
    if (s->co2_ppm > 2000) score += 25.0f;
    else if (s->co2_ppm > 1000) score += 15.0f;
    else if (s->co2_ppm > 600) score += 5.0f;
    
    /* Ethylene contribution (0-25 points) */
    float eth_pct = (float)s->ethylene_raw / 4095.0f;
    score += eth_pct * 25.0f;
    
    /* Temperature contribution (0-20 points) */
    float temp_c = s->temp_c_x10 / 10.0f;
    if (temp_c > 10.0f) score += 20.0f;      /* Way too warm */
    else if (temp_c > 7.0f) score += 15.0f;  /* Warm */
    else if (temp_c > 5.0f) score += 5.0f;   /* Slightly warm */
    
    /* Clamp to 0-100 */
    if (score > 100.0f) score = 100.0f;
    if (score < 0.0f) score = 0.0f;
    
    return (uint8_t)score;
}

/* ── Camera Capture ─────────────────────────────────────────────────── */
static void capture_photo(uint8_t camera_id) {
    /* Enable camera */
    if (camera_id == 1) {
        HAL_GPIO_WritePin(GPIOA, PIN_CAM1_EN, GPIO_PIN_SET);
    } else {
        HAL_GPIO_WritePin(GPIOB, PIN_CAM2_EN, GPIO_PIN_SET);
    }
    HAL_Delay(500); /* Camera warm-up */
    
    /* Capture QVGA RGB565 image into g_sensor.camera_buf */
    /* (DCMI/DMA interface to OV5640 — implementation depends on HAL config) */
    /* For brevity, we flag that an image is ready for hub to request */
    g_sensor.image_ready = 1;
    
    /* Disable camera to save power */
    if (camera_id == 1) {
        HAL_GPIO_WritePin(GPIOA, PIN_CAM1_EN, GPIO_PIN_RESET);
    } else {
        HAL_GPIO_WritePin(GPIOB, PIN_CAM2_EN, GPIO_PIN_RESET);
    }
}

/* ── Door Open Detection ────────────────────────────────────────────── */
static uint8_t check_door_state(void) {
    /* Use TSL2591 light sensor: high light = door open */
    if (g_sensor.light_lux_x10 > 100) { /* > 10 lux = door open */
        return 1;
    }
    return 0;
}

/* ── Main Sensor Reading Loop ───────────────────────────────────────── */
static void read_all_sensors(I2C_HandleTypeDef *hi2c, UART_HandleTypeDef *huart_sc,
                              ADC_HandleTypeDef *hadc) {
    /* Read gas sensors */
    sgp40_measure(hi2c, &g_sensor.voc_index);
    scd30_read_co2(huart_sc, &g_sensor.co2_ppm);
    
    /* Read temp/humidity */
    sht40_measure(hi2c, &g_sensor.temp_c_x10, &g_sensor.humidity_x10);
    
    /* Read ethylene (analog MQ-3) */
    HAL_ADC_Start(hadc);
    HAL_ADC_PollForConversion(hadc, 100);
    g_sensor.ethylene_raw = HAL_ADC_GetValue(hadc);
    HAL_ADC_Stop(hadc);
    
    /* Read weight sensors (4 shelves) */
    for (int i = 0; i < 4; i++) {
        uint32_t raw = hx711_read(GPIOC_BASE, 0x08 << i, GPIOC_BASE, 0x10 << i);
        /* Convert raw HX711 reading to milligrams */
        /* Calibration: raw 0 ≈ 0mg, raw 8388608 ≈ 5000000mg (5kg) */
        g_sensor.weight_mg[i] = (uint32_t)((float)raw * 5000000.0f / 8388608.0f);
    }
    
    /* Check door state */
    g_sensor.door_state = check_door_state();
    
    /* Calculate spoilage score */
    g_sensor.spoilage_score = calculate_spoilage(&g_sensor);
    
    /* Read battery voltage */
    /* (ADC on battery sense pin — simplified) */
    g_sensor.battery_pct = 100; /* Placeholder — calculated from ADC */
    
    /* If door just opened, capture photo burst */
    if (g_sensor.door_state && !(g_sensor.last_reading_ms > 0)) {
        g_sensor.door_open_count++;
        for (int i = 0; i < DOOR_OPEN_BURST; i++) {
            capture_photo(1); /* Top camera */
        }
        capture_photo(2); /* Shelf camera */
    }
    
    g_sensor.last_reading_ms = HAL_GetTick();
}

/* ── TDMA Transmission ──────────────────────────────────────────────── */
static radio_handle_t g_radio = {
    .config = RADIO_CONFIG_DEFAULT,
    .pin_nss = 17,  /* PB6 as NSS */
    .pin_busy = 14,
    .pin_irq = 15,
    .pin_reset = 16,
};

static void transmit_fridge_data(void) {
    packet_t pkt;
    pkt_init(&pkt, ADDR_FRIDGE, ADDR_HUB, PKT_FRIDGE_DATA);
    pkt_add_payload(&pkt, (uint8_t*)&g_sensor, sizeof(fridge_data_t));
    pkt_finalize(&pkt);
    radio_send(&g_radio, pkt.data, pkt.len);
}

/* ── Main ───────────────────────────────────────────────────────────── */
int main(void) {
    HAL_Init();
    SystemClock_Config();
    
    /* Initialize peripherals */
    /* (HAL init code — I2C, SPI, UART, ADC, GPIO — omitted for brevity) */
    
    /* Initialize radio */
    radio_init(&g_radio, &RADIO_CONFIG_DEFAULT);
    
    memset(&g_sensor, 0, sizeof(g_sensor));
    
    uint32_t last_read_time = 0;
    uint32_t last_tx_time = 0;
    
    while (1) {
        uint32_t now = HAL_GetTick();
        
        /* Read sensors every 60 seconds */
        if (now - last_read_time > READ_INTERVAL_MS) {
            /* Enter low-power: wake, read, transmit, sleep */
            read_all_sensors(NULL, NULL, NULL); /* Pass real HAL handles */
            last_read_time = now;
        }
        
        /* Transmit in TDMA slot 1 */
        uint32_t slot_time = now % FRAME_MS;
        if (slot_time >= (1 * SLOT_DURATION_MS) && slot_time < (2 * SLOT_DURATION_MS)) {
            if (now - last_tx_time > FRAME_MS) {
                transmit_fridge_data();
                last_tx_time = now;
            }
        }
        
        /* Deep sleep between readings to save battery */
        if (now - last_read_time > 5000 && now - last_tx_time > 1000) {
            /* Enter STOP mode for power saving */
            HAL_PWR_EnterSTOPMode(PWR_LOWPOWERREGULATOR_ON, PWR_STOPENTRY_WFI);
        }
    }
    
    return 0;
}