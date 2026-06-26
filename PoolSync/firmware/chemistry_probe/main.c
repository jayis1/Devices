/**
 * @file main.c
 * @brief PoolSync Chemistry Probe firmware — STM32L476RG ultra-low-power
 *
 * Wake every 5 minutes, read all sensors sequentially, transmit, sleep.
 * Battery life target: 18 months on 3× AA LiFeS2.
 *
 * Sensor excitation sequence: pH → ORP → Cl → temp → conductivity → turbidity
 * Each sensor is powered on, stabilized, read, then powered down.
 */

#include "stm32l4xx_hal.h"
#include <string.h>
#include <stdio.h>
#include "psp_protocol.h"
#include "psp_radio.h"
#include "psp_sensor.h"

/* ============================================================
 * PIN DEFINITIONS — STM32L476RG
 * ============================================================ */

/* ADC channels for chemistry sensors */
#define PIN_PH_OUT           ADC_CHANNEL_1   /* PA0 — ISFET pH amplifier output */
#define PIN_ORP_OUT          ADC_CHANNEL_2   /* PA1 — ORP amplifier output */
#define PIN_CL_OUT           ADC_CHANNEL_3   /* PA2 — Amperometric Cl potentiostat */
#define PIN_COND_OUT         ADC_CHANNEL_4   /* PA3 — Conductivity receiver */
#define PIN_TURB_OUT         ADC_CHANNEL_5   /* PA4 — Turbidity (TSL2591) */
#define PIN_VBATT            ADC_CHANNEL_6   /* PA5 — Battery voltage divider */

/* GPIO outputs for sensor excitation */
#define PIN_PH_ENABLE        GPIO_PIN_6      /* PB6 — pH ISFET power enable */
#define PIN_ORP_ENABLE       GPIO_PIN_7      /* PB7 — ORP power enable */
#define PIN_CL_ENABLE        GPIO_PIN_8      /* PB8 — Cl potentiostat enable */
#define PIN_COND_ENABLE      GPIO_PIN_9      /* PB9 — Conductivity excitation enable */
#define PIN_TURB_IR_ENABLE   GPIO_PIN_10     /* PB10 — Turbidity IR LED enable */
#define PIN_TEMP_POWER       GPIO_PIN_11     /* PB11 — DS18B20 power enable */

/* DS18B20 OneWire */
#define PIN_DS18B20          GPIO_PIN_12     /* PB12 — OneWire data */

/* SX1262 SPI */
#define PIN_SX_SPI_SCK      GPIO_PIN_13     /* PB13 — SPI2 SCK */
#define PIN_SX_SPI_MISO     GPIO_PIN_14     /* PB14 — SPI2 MISO */
#define PIN_SX_SPI_MOSI     GPIO_PIN_15     /* PB15 — SPI2 MOSI */
#define PIN_SX_NSS           GPIO_PIN_0      /* PC0 — SPI2 NSS */
#define PIN_SX_DIO1          GPIO_PIN_1      /* PC1 — SX1262 DIO1 */
#define PIN_SX_BUSY          GPIO_PIN_2      /* PC2 — SX1262 BUSY */
#define PIN_SX_NRST          GPIO_PIN_3      /* PC3 — SX1262 NRST */

/* Status LED */
#define PIN_LED_STATUS       GPIO_PIN_4      /* PC4 — Status LED (active low) */

/* I2C for TSL2591 */
#define PIN_I2C_SDA          GPIO_PIN_7      /* PB7 — I2C1 SDA (shared port) */
#define PIN_I2C_SCL          GPIO_PIN_8      /* PB8 — I2C1 SCL */

/* ============================================================
 * CONSTANTS
 * ============================================================ */

#define WAKEUP_INTERVAL_SEC    300     /* 5 minutes between readings */
#define SENSOR_STABILIZE_MS    500     /* Wait 500ms after powering on sensor */
#define ADC_READINGS_AVG       16      /* Average 16 ADC readings per sensor */
#define BATTERY_LOW_MV         3500    /* 3.5V = low battery warning */
#define BATTERY_CRIT_MV         3000    /* 3.0V = critical, stop transmitting */

/* ============================================================
 * STATE
 * ============================================================ */

static ADC_HandleTypeDef hadc1;
static SPI_HandleTypeDef hspi2;
static I2C_HandleTypeDef hi2c1;
static RTC_HandleTypeDef hrtc;
static uint16_t self_addr = PSP_ADDR_CHEM_PROBE_BASE;  /* Default probe 1 */

/* ============================================================
 * SENSOR READ FUNCTIONS
 * ============================================================ */

/**
 * Read ADC channel with oversampling
 * Average ADC_READINGS_AVG samples to reduce noise
 */
static uint16_t adc_read_channel(uint32_t channel, uint32_t avg_count)
{
    ADC_ChannelConfTypeDef sConfig = {0};
    sConfig.Channel = channel;
    sConfig.Rank = ADC_REGULAR_RANK_1;
    sConfig.SamplingTime = ADC_SAMPLETIME_247CYCLES_5;  /* Long sampling for high impedance */
    HAL_ADC_ConfigChannel(&hadc1, &sConfig);

    uint32_t sum = 0;
    for (uint32_t i = 0; i < avg_count; i++) {
        HAL_ADC_Start(&hadc1);
        HAL_ADC_PollForConversion(&hadc1, 10);
        sum += HAL_ADC_GetValue(&hadc1);
    }
    HAL_ADC_Stop(&hadc1);

    return (uint16_t)(sum / avg_count);
}

/**
 * Convert ADC reading to voltage (3.3V reference, 12-bit ADC)
 */
static float adc_to_voltage(uint16_t adc_val)
{
    return (float)adc_val * 3.3f / 4096.0f;
}

/**
 * Convert voltage to pH using calibration constants
 * pH 7.0 buffer = 1.65V (midpoint), pH 4.0 buffer = 2.06V
 * Slope = (7.0 - 4.0) / (1.65 - 2.06) = -7.317
 * Offset = 7.0 - (-7.317 * 1.65) = 19.07
 */
static float voltage_to_ph(float voltage)
{
    /* Calibration: slope and offset determined during 2-point calibration */
    static float ph_slope = -7.317f;
    static float ph_offset = 19.07f;
    float ph = ph_slope * voltage + ph_offset;
    /* Clamp to valid range */
    if (ph < 0.0f) ph = 0.0f;
    if (ph > 14.0f) ph = 14.0f;
    return ph;
}

/**
 * Read ISFET pH sensor
 * Power on ISFET, wait for stabilization, read ADC, power off
 */
static float read_ph(void)
{
    HAL_GPIO_WritePin(GPIOB, PIN_PH_ENABLE, GPIO_PIN_SET);
    HAL_Delay(SENSOR_STABILIZE_MS);

    uint16_t raw = adc_read_channel(PIN_PH_OUT, ADC_READINGS_AVG);
    float voltage = adc_to_voltage(raw);
    float ph = voltage_to_ph(voltage);

    HAL_GPIO_WritePin(GPIOB, PIN_PH_ENABLE, GPIO_PIN_RESET);
    return ph;
}

/**
 * Read ORP (Oxidation-Reduction Potential)
 * Platinum electrode with Ag/AgCl reference
 */
static float read_orp(void)
{
    HAL_GPIO_WritePin(GPIOB, PIN_ORP_ENABLE, GPIO_PIN_SET);
    HAL_Delay(SENSOR_STABILIZE_MS);

    uint16_t raw = adc_read_channel(PIN_ORP_OUT, ADC_READINGS_AVG);
    float voltage = adc_to_voltage(raw);
    /* ORP = (Vref - Vmeasured) * 1000 / 1.0 + offset
     * Vref = 2.5V (virtual ground), offset = calibration constant */
    float orp_mv = (2.5f - voltage) * 1000.0f;

    HAL_GPIO_WritePin(GPIOB, PIN_ORP_ENABLE, GPIO_PIN_RESET);
    return orp_mv;
}

/**
 * Read amperometric free chlorine sensor
 * 3-electrode potentiostat: working, reference, counter
 * Output current proportional to free chlorine concentration
 */
static float read_free_chlorine(void)
{
    HAL_GPIO_WritePin(GPIOB, PIN_CL_ENABLE, GPIO_PIN_SET);
    /* Amperometric sensor needs longer stabilization */
    HAL_Delay(2000);  /* 2 seconds for electrode equilibrium */

    uint16_t raw = adc_read_channel(PIN_CL_OUT, ADC_READINGS_AVG);
    float voltage = adc_to_voltage(raw);
    /* Convert voltage to ppm using calibration curve */
    /* Typical: 0 ppm = 0.5V, 10 ppm = 2.5V */
    float cl_ppm = (voltage - 0.5f) * 10.0f / 2.0f;
    if (cl_ppm < 0.0f) cl_ppm = 0.0f;

    HAL_GPIO_WritePin(GPIOB, PIN_CL_ENABLE, GPIO_PIN_RESET);
    return cl_ppm;
}

/**
 * Read DS18B20 waterproof temperature sensor
 * OneWire protocol — 750ms conversion time
 */
static float read_temperature(void)
{
    HAL_GPIO_WritePin(GPIOB, PIN_TEMP_POWER, GPIO_PIN_SET);
    HAL_Delay(50);  /* Power-up stabilization */

    /* OneWire reset + skip ROM + convert T command */
    /* Simplified — real implementation uses HAL OneWire driver */
    HAL_Delay(750);  /* Wait for temperature conversion */

    /* Read scratchpad: would use OneWire read command */
    float temp_c = 25.0f;  /* Placeholder — actual OneWire read */

    HAL_GPIO_WritePin(GPIOB, PIN_TEMP_POWER, GPIO_PIN_RESET);
    return temp_c;
}

/**
 * Read inductive toroidal conductivity sensor
 * Excitation coil driven by PWM, receive coil measured
 */
static float read_conductivity(void)
{
    HAL_GPIO_WritePin(GPIOB, PIN_COND_ENABLE, GPIO_PIN_SET);
    HAL_Delay(SENSOR_STABILIZE_MS);

    uint16_t raw = adc_read_channel(PIN_COND_OUT, ADC_READINGS_AVG);
    float voltage = adc_to_voltage(raw);
    /* Convert to µS/cm using calibration constant */
    /* Typical pool water: 800–2000 µS/cm */
    float conductivity = voltage * 30000.0f / 3.3f;  /* Simplified linear calibration */

    HAL_GPIO_WritePin(GPIOB, PIN_COND_ENABLE, GPIO_PIN_RESET);
    return conductivity;
}

/**
 * Read turbidity using TSL2591 light sensor + IR LED
 * IR LED on one side of pool, TSL2591 on other
 * Turbidity proportional to 1 - (received_light / reference_light)
 */
static float read_turbidity(void)
{
    /* Power on IR LED */
    HAL_GPIO_WritePin(GPIOB, PIN_TURB_IR_ENABLE, GPIO_PIN_SET);
    HAL_Delay(100);

    /* Read TSL2591 over I2C (both channels: visible + IR) */
    /* Simplified — actual implementation uses TSL2591 driver */
    uint16_t ch0 = 15000;  /* Full-spectrum (visible + IR) */
    uint16_t ch1 = 8000;   /* IR only */

    /* Calculate turbidity */
    float ir_ratio = (float)ch1 / (float)ch0;
    /* Clear water: ir_ratio ≈ 0.53, turbid water: ir_ratio decreases */
    /* NTU = f(ir_ratio) — calibrated with Formazin standard */
    float turbidity_ntu = (0.53f - ir_ratio) * 2000.0f;
    if (turbidity_ntu < 0.0f) turbidity_ntu = 0.0f;

    HAL_GPIO_WritePin(GPIOB, PIN_TURB_IR_ENABLE, GPIO_PIN_RESET);
    return turbidity_ntu;
}

/**
 * Read battery voltage via voltage divider
 * 3× AA LiFeS2 ≈ 4.5V nominal, divider to get within ADC range
 */
static uint16_t read_battery_mv(void)
{
    uint16_t raw = adc_read_channel(PIN_VBATT, 4);
    /* Voltage divider: Vbatt/2 → ADC. Full-scale 3.3V = 6.6V */
    return (uint16_t)(adc_to_voltage(raw) * 2000.0f);
}

/* ============================================================
 * MAIN — Low-power sensor cycle
 * ============================================================ */

int main(void)
{
    HAL_Init();

    /* Clock configuration: MSI 4 MHz for low power */
    /* (RCC config would go here — HAL_RCC_OscConfig, HAL_RCC_ClockConfig) */

    /* Initialize peripherals */
    /* ADC init */
    hadc1.Instance = ADC1;
    hadc1.Init.ClockPrescaler = ADC_CLOCK_ASYNC_DIV4;
    hadc1.Init.Resolution = ADC_RESOLUTION_12B;
    hadc1.Init.ScanConvMode = DISABLE;
    hadc1.Init.ContinuousConvMode = DISABLE;
    hadc1.Init.DiscontinuousConvMode = DISABLE;
    hadc1.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
    hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
    hadc1.Init.NbrOfConversion = 1;
    HAL_ADC_Init(&hadc1);

    /* SPI2 for SX1262 */
    hspi2.Instance = SPI2;
    hspi2.Init.Mode = SPI_MODE_MASTER;
    hspi2.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_32;  /* 4 MHz */
    hspi2.Init.Direction = SPI_DIRECTION_2LINES;
    hspi2.Init.DataSize = SPI_DATASIZE_8BIT;
    hspi2.Init.CLKPolarity = SPI_POLARITY_LOW;
    hspi2.Init.CLKPhase = SPI_PHASE_1EDGE;
    hspi2.Init.NSS = SPI_NSS_SOFT;
    hspi2.Init.FirstBit = SPI_FIRSTBIT_MSB;
    HAL_SPI_Init(&hspi2);

    /* I2C1 for TSL2591 */
    hi2c1.Instance = I2C1;
    hi2c1.Init.Timing = 0x00506C82;  /* 100 kHz */
    hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
    HAL_I2C_Init(&hi2c1);

    /* GPIO init */
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();
    GPIO_InitTypeDef gpio = {0};
    gpio.Mode = GPIO_MODE_OUTPUT_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;

    /* Sensor enable pins */
    gpio.Pin = PIN_PH_ENABLE | PIN_ORP_ENABLE | PIN_CL_ENABLE |
               PIN_COND_ENABLE | PIN_TURB_IR_ENABLE | PIN_TEMP_POWER;
    HAL_GPIO_Init(GPIOB, &gpio);

    /* LED */
    gpio.Pin = PIN_LED_STATUS;
    HAL_GPIO_Init(GPIOC, &gpio);

    /* Radio init */
    psp_radio_init();

    /* Set node address from config (hardcoded for now) */
    self_addr = PSP_ADDR_CHEM_PROBE_BASE + 0;

    /* ===== MAIN LOOP — Wake, read, transmit, sleep ===== */
    while (1) {
        /* Blink status LED */
        HAL_GPIO_WritePin(GPIOC, PIN_LED_STATUS, GPIO_PIN_SET);

        /* 1. Read all chemistry sensors */
        psp_chem_reading_t reading;
        memset(&reading, 0, sizeof(reading));

        reading.ph = read_ph();
        reading.valid_mask |= PSP_CHEM_VALID_PH;

        reading.orp_mv = read_orp();
        reading.valid_mask |= PSP_CHEM_VALID_ORP;

        reading.free_cl_ppm = read_free_chlorine();
        reading.valid_mask |= PSP_CHEM_VALID_CL;

        reading.temperature_c = read_temperature();
        reading.valid_mask |= PSP_CHEM_VALID_TEMP;

        reading.conductivity_us = read_conductivity();
        reading.valid_mask |= PSP_CHEM_VALID_CONDUCTIVITY;

        reading.turbidity_ntu = read_turbidity();
        reading.valid_mask |= PSP_CHEM_VALID_TURBIDITY;

        reading.timestamp_ms = HAL_GetTick();
        reading.battery_mv = read_battery_mv();

        /* 2. Build PSP frame */
        psp_chem_data_t payload;
        payload.ph = reading.ph;
        payload.orp_mv = reading.orp_mv;
        payload.free_cl_ppm = reading.free_cl_ppm;
        payload.temperature_c = reading.temperature_c;
        payload.conductivity_us = reading.conductivity_us;
        payload.turbidity_ntu = reading.turbidity_ntu;
        payload.battery_mv = reading.battery_mv;
        payload.rssi_dbm = psp_radio_get_rssi();

        psp_header_t header = {
            .preamble = PSP_PREAMBLE,
            .sync_word = PSP_SYNC_WORD,
            .src_addr = self_addr,
            .dst_addr = PSP_ADDR_HUB,
            .msg_type = PSP_MSG_CHEM_DATA
        };

        psp_frame_t frame;
        uint16_t frame_len = psp_encode(&header, (uint8_t *)&payload,
                                         sizeof(payload), (uint8_t *)&frame, sizeof(frame));

        /* 3. Transmit in assigned TDMA slot */
        if (frame_len > 0) {
            psp_radio_wake();
            psp_radio_send(&frame);
            psp_radio_sleep();
        }

        /* 4. Check for any incoming commands (dosing, calibration, OTA) */
        psp_frame_t rx_frame;
        if (psp_radio_recv(&rx_frame, 2000) == 0) {
            if (rx_frame.header.msg_type == PSP_MSG_CHEM_CALIBRATE) {
                /* Handle 2-point pH calibration request */
            }
        }

        /* 5. Turn off LED and enter deep sleep */
        HAL_GPIO_WritePin(GPIOC, PIN_LED_STATUS, GPIO_PIN_RESET);

        /* Configure RTC wakeup timer for next reading */
        /* Wake up after WAKEUP_INTERVAL_SEC seconds */
        /* (RTC configuration would go here) */

        /* Enter STOP2 mode for lowest power consumption */
        /* HAL_PWR_EnterSTOPMode(PWR_LOWPOWERREGULATOR_ON, PWR_STOPENTRY_WFI); */

        /* After wakeup, system clock needs re-initialization */
        /* For development: simple HAL_Delay instead */
        HAL_Delay(WAKEUP_INTERVAL_SEC * 1000);
    }
}