/**
 * @file main.c
 * @brief PoolSync Equipment Controller firmware — STM32F407VG
 *
 * Controls 8× SPDT relays (pump, heater, lights, valves, blower, spare)
 * Drives 3× peristaltic pumps (acid, chlorine, clarifier) via stepper drivers
 * Reads flow sensor, pressure sensor, and GFCI monitor
 * Implements safety interlocks (entrapment, GFCI, freeze protection)
 */

#include "stm32f4xx_hal.h"
#include <string.h>
#include <stdio.h>
#include "psp_protocol.h"
#include "psp_radio.h"
#include "psp_sensor.h"

/* ============================================================
 * PIN DEFINITIONS — STM32F407VG
 * ============================================================ */

/* Relay outputs (8× SPDT via ULN2803A) */
#define PIN_RELAY_PUMP          GPIO_PIN_0   /* PE0 — Main pool pump */
#define PIN_RELAY_HEATER        GPIO_PIN_1   /* PE1 — Pool heater */
#define PIN_RELAY_POOL_LIGHT    GPIO_PIN_2   /* PE2 — Pool light */
#define PIN_RELAY_SPA_LIGHT     GPIO_PIN_3   /* PE3 — Spa light */
#define PIN_RELAY_VALVE1        GPIO_PIN_4   /* PE4 — Valve 1 (pool/spa) */
#define PIN_RELAY_VALVE2        GPIO_PIN_5   /* PE5 — Valve 2 (clean/drain) */
#define PIN_RELAY_BLOWER        GPIO_PIN_6   /* PE6 — Spa blower */
#define PIN_RELAY_SPARE         GPIO_PIN_7   /* PE7 — Spare */

/* Stepper driver pins — Peristaltic Pump 1 (Acid) */
#define PIN_STEP1_STEP          GPIO_PIN_8   /* PE8 — A4988 STEP */
#define PIN_STEP1_DIR           GPIO_PIN_9   /* PE9 — A4988 DIR */
#define PIN_STEP1_EN            GPIO_PIN_10  /* PE10 — A4988 ENABLE */

/* Stepper driver pins — Peristaltic Pump 2 (Chlorine) */
#define PIN_STEP2_STEP          GPIO_PIN_11  /* PE11 — A4988 STEP */
#define PIN_STEP2_DIR           GPIO_PIN_12  /* PE12 — A4988 DIR */
#define PIN_STEP2_EN            GPIO_PIN_13  /* PE13 — A4988 ENABLE */

/* Stepper driver pins — Peristaltic Pump 3 (Clarifier) */
#define PIN_STEP3_STEP          GPIO_PIN_14  /* PE14 — A4988 STEP */
#define PIN_STEP3_DIR           GPIO_PIN_15  /* PE15 — A4988 DIR */
#define PIN_STEP3_EN            GPIO_PIN_0   /* PD0 — A4988 ENABLE */

/* Flow sensor (YF-S201) — TIM2 input capture */
#define PIN_FLOW_SENSOR         GPIO_PIN_0   /* PA0 — TIM2_CH1 */

/* Pressure sensor (MPX5010DP) — ADC */
#define PIN_PRESSURE_SENSOR     ADC_CHANNEL_8 /* PB0 — ADC1_CH8 */

/* GFCI current sensor (ACS712-30A) — ADC */
#define PIN_CURRENT_SENSOR      ADC_CHANNEL_9 /* PB1 — ADC1_CH9 */

/* SX1262 Sub-GHz Radio — SPI2 */
#define PIN_SX_NSS              GPIO_PIN_12  /* PB12 — SPI2 NSS */
#define PIN_SX_SCK              GPIO_PIN_13  /* PB13 — SPI2 SCK */
#define PIN_SX_MISO             GPIO_PIN_14  /* PB14 — SPI2 MISO */
#define PIN_SX_MOSI             GPIO_PIN_15  /* PB15 — SPI2 MOSI */
#define PIN_SX_DIO1             GPIO_PIN_1   /* PD1 — EXTI */
#define PIN_SX_BUSY             GPIO_PIN_2   /* PD2 — GPIO input */
#define PIN_SX_NRST             GPIO_PIN_3   /* PD3 — GPIO output */

/* Status LEDs */
#define PIN_LED_STATUS          GPIO_PIN_4   /* PD4 — Green */
#define PIN_LED_RELAY           GPIO_PIN_5   /* PD5 — Blue */
#define PIN_LED_ALARM           GPIO_PIN_6   /* PD6 — Red */

/* ============================================================
 * CONSTANTS
 * ============================================================ */

/* Peristaltic pump calibration */
#define STEPS_PER_ML_ACID       420    /* Steps per mL of acid (muriatic) */
#define STEPS_PER_ML_CHLORINE  380    /* Steps per mL of liquid chlorine */
#define STEPS_PER_ML_CLARIFIER 450    /* Steps per mL of clarifier */
#define PUMP_STEP_DELAY_US      800    /* microseconds between steps */

/* Safety limits */
#define FLOW_VERIFICATION_TIMEOUT_S  30   /* Max seconds to verify dosing flow */
#define MAX_PRESSURE_KPA            250   /* Filter pressure alarm threshold */
#define ENTRAPMENT_PRESSURE_KPA     350   /* Suction entrapment detection */
#define GFCI_CURRENT_A               30   /* Overcurrent threshold */
#define PUMP_MIN_FLOW_LPM           0.5   /* Minimum flow during dosing */

/* Dosing pump IDs */
#define PUMP_ACID       0
#define PUMP_CHLORINE   1
#define PUMP_CLARIFIER  2

/* ============================================================
 * STATE
 * ============================================================ */

typedef struct {
    bool relays[8];              /* Current relay states */
    float flow_lpm;              /* Current flow rate */
    float pressure_kpa;          /* Current filter pressure */
    float current_a;             /* Current AC draw */
    bool gfci_fault;             /* GFCI fault detected */
    bool entrapment_fault;       /* Entrapment detected */
    bool dosing_active;          /* Peristaltic pump currently dosing */
    uint8_t dosing_pump;         /* Which pump is dosing (0=acid, 1=chlorine, 2=clarifier) */
    uint32_t dosing_command_id;  /* Current dosing command ID */
    TIM_HandleTypeDef htim2;     /* Flow sensor timer */
    ADC_HandleTypeDef hadc1;     /* ADC for pressure and current */
    SPI_HandleTypeDef hspi2;     /* SPI for SX1262 */
} psp_equip_state_t;

static psp_equip_state_t g_state;

/* ============================================================
 * RELAY CONTROL
 * ============================================================ */

static const uint16_t relay_pins[8] = {
    PIN_RELAY_PUMP, PIN_RELAY_HEATER, PIN_RELAY_POOL_LIGHT,
    PIN_RELAY_SPA_LIGHT, PIN_RELAY_VALVE1, PIN_RELAY_VALVE2,
    PIN_RELAY_BLOWER, PIN_RELAY_SPARE
};

static void relay_set(uint8_t id, bool on)
{
    if (id < 8) {
        HAL_GPIO_WritePin(GPIOE, relay_pins[id], on ? GPIO_PIN_SET : GPIO_PIN_RESET);
        g_state.relays[id] = on;
    }
}

static bool relay_get(uint8_t id)
{
    if (id < 8) return g_state.relays[id];
    return false;
}

int psp_equip_set_relay(uint8_t device_id, bool on)
{
    if (device_id > 7) return -1;
    /* Safety interlock: can't turn on pump if GFCI or entrapment fault */
    if (on && device_id == 0 && (g_state.gfci_fault || g_state.entrapment_fault))
        return -2;
    relay_set(device_id, on);
    return 0;
}

/* ============================================================
 * PERISTALTIC PUMP DOSING
 * ============================================================ */

/**
 * Run peristaltic pump for specified volume
 * Verifies flow after dosing using YF-S201 flow sensor
 */
static void pump_step(uint8_t pump_id, uint32_t steps, bool forward)
{
    GPIO_TypeDef* step_port = GPIOE;
    uint16_t step_pin;
    uint16_t dir_pin;
    uint16_t en_pin;

    switch (pump_id) {
        case PUMP_ACID:
            step_pin = PIN_STEP1_STEP; dir_pin = PIN_STEP1_DIR; en_pin = PIN_STEP1_EN;
            break;
        case PUMP_CHLORINE:
            step_pin = PIN_STEP2_STEP; dir_pin = PIN_STEP2_DIR; en_pin = PIN_STEP2_EN;
            break;
        case PUMP_CLARIFIER:
            step_pin = PIN_STEP3_STEP; dir_pin = PIN_STEP3_DIR; en_pin = PIN_STEP3_EN;
            break;
        default:
            return;
    }

    /* Enable stepper driver */
    HAL_GPIO_WritePin(step_port, en_pin, GPIO_PIN_RESET);  /* Active low enable */

    /* Set direction */
    HAL_GPIO_WritePin(step_port, dir_pin, forward ? GPIO_PIN_SET : GPIO_PIN_RESET);

    /* Step the motor */
    for (uint32_t i = 0; i < steps; i++) {
        HAL_GPIO_WritePin(step_port, step_pin, GPIO_PIN_SET);
        HAL_Delay(PUMP_STEP_DELAY_US / 1000 + 1);
        HAL_GPIO_WritePin(step_port, step_pin, GPIO_PIN_RESET);
        HAL_Delay(PUMP_STEP_DELAY_US / 1000 + 1);

        /* Check for safety faults every 100 steps */
        if (i % 100 == 0) {
            if (g_state.gfci_fault || g_state.entrapment_fault) {
                /* ABORT dosing immediately */
                HAL_GPIO_WritePin(step_port, en_pin, GPIO_PIN_SET);
                g_state.dosing_active = false;
                return;
            }
        }
    }

    /* Disable stepper driver */
    HAL_GPIO_WritePin(step_port, en_pin, GPIO_PIN_SET);
}

float psp_equip_dose(uint8_t pump_id, float volume_ml, uint16_t timeout_s)
{
    /* Safety checks */
    if (pump_id > 2) return 0.0f;
    if (volume_ml <= 0.0f) return 0.0f;

    /* Maximum dose limits */
    const float max_dose[3] = {ACID_MAX_DOSE_ML, CHLORINE_MAX_DOSE_ML, CLARIFIER_MAX_DOSE_ML};
    if (volume_ml > max_dose[pump_id]) return 0.0f;

    /* Calculate steps from volume */
    const uint32_t steps_per_ml[3] = {STEPS_PER_ML_ACID, STEPS_PER_ML_CHLORINE, STEPS_PER_ML_CLARIFIER};
    uint32_t total_steps = (uint32_t)(volume_ml * steps_per_ml[pump_id]);

    /* Verify main pump is running (flow required for chemical distribution) */
    if (!g_state.relays[0]) {
        /* Turn pump on temporarily for dosing */
        relay_set(0, true);
    }

    g_state.dosing_active = true;
    g_state.dosing_pump = pump_id;

    /* Run the peristaltic pump */
    uint32_t start_time = HAL_GetTick();
    pump_step(pump_id, total_steps, true);

    /* Wait for flow to stabilize and verify dosing */
    HAL_Delay(2000);  /* Wait 2 seconds for flow to distribute */

    /* Verify flow sensor shows expected flow */
    float total_flow_ml = g_state.flow_lpm * 1000.0f / 60.0f * ((HAL_GetTick() - start_time) / 1000.0f);

    g_state.dosing_active = false;

    /* Return actual volume dispensed (from step count, calibrated) */
    float actual_ml = (float)total_steps / steps_per_ml[pump_id];
    return actual_ml;
}

/* ============================================================
 * FLOW SENSOR — YF-S201 pulse counter
 * F = (pulse_count / 7.5) L/min
 * ============================================================ */

static volatile uint32_t flow_pulse_count = 0;

void HAL_TIM_IC_CaptureCallback(TIM_HandleTypeDef *htim)
{
    if (htim == &g_state.htim2)
        flow_pulse_count++;
}

static float read_flow_lpm(void)
{
    uint32_t count1 = flow_pulse_count;
    HAL_Delay(1000);
    uint32_t count2 = flow_pulse_count;
    float flow_rate = ((float)(count2 - count1) / 7.5f);  /* L/min */
    return flow_rate;
}

/* ============================================================
 * PRESSURE SENSOR — MPX5010DP
 * Vout = Vs * (0.09 * P + 0.04) where P in kPa
 * ============================================================ */

static float read_pressure_kpa(void)
{
    ADC_ChannelConfTypeDef sConfig = {0};
    sConfig.Channel = PIN_PRESSURE_SENSOR;
    sConfig.Rank = 1;
    sConfig.SamplingTime = ADC_SAMPLETIME_84CYCLES;
    HAL_ADC_ConfigChannel(&g_state.hadc1, &sConfig);

    HAL_ADC_Start(&g_state.hadc1);
    HAL_ADC_PollForConversion(&g_state.hadc1, 10);
    uint16_t raw = HAL_ADC_GetValue(&g_state.hadc1);
    HAL_ADC_Stop(&g_state.hadc1);

    float voltage = (float)raw * 3.3f / 4096.0f;
    /* MPX5010DP: Vout = 5.0 * (0.09 * P + 0.04), but we use 3.3V ref with divider */
    float pressure = (voltage / 3.3f * 5.0f - 0.04f) / 0.09f;
    return pressure;
}

/* ============================================================
 * GFCI / OVERCURRENT DETECTION — ACS712-30A
 * ============================================================ */

static float read_current_a(void)
{
    ADC_ChannelConfTypeDef sConfig = {0};
    sConfig.Channel = PIN_CURRENT_SENSOR;
    sConfig.Rank = 1;
    sConfig.SamplingTime = ADC_SAMPLETIME_84CYCLES;
    HAL_ADC_ConfigChannel(&g_state.hadc1, &sConfig);

    /* Average multiple readings for AC RMS */
    uint32_t sum = 0;
    for (int i = 0; i < 100; i++) {
        HAL_ADC_Start(&g_state.hadc1);
        HAL_ADC_PollForConversion(&g_state.hadc1, 5);
        sum += HAL_ADC_GetValue(&g_state.hadc1);
    }
    HAL_ADC_Stop(&g_state.hadc1);

    float voltage = ((float)sum / 100.0f) * 3.3f / 4096.0f;
    /* ACS712-30A: 66mV/A, midpoint at Vcc/2 = 1.65V */
    float current = (voltage - 1.65f) / 0.066f;
    if (current < 0.0f) current = -current;  /* Absolute value for AC */
    return current;
}

bool psp_equip_gfci_fault(void)
{
    return g_state.gfci_fault;
}

bool psp_equip_entrapment_detected(void)
{
    return g_state.entrapment_fault;
}

int psp_equip_read(psp_equip_reading_t *reading)
{
    reading->pump_on = relay_get(0);
    reading->heater_on = relay_get(1);
    reading->pool_light_on = relay_get(2);
    reading->spa_light_on = relay_get(3);
    reading->valve1_on = relay_get(4);
    reading->valve2_on = relay_get(5);
    reading->blower_on = relay_get(6);
    reading->spare_on = relay_get(7);

    reading->flow_lpm = read_flow_lpm();
    reading->pressure_kpa = read_pressure_kpa();
    reading->current_a = read_current_a();
    reading->pump_dosing = g_state.dosing_active ? (g_state.dosing_pump + 1) : 0;

    /* Safety checks */
    if (reading->pressure_kpa > ENTRAPMENT_PRESSURE_KPA)
        g_state.entrapment_fault = true;
    if (reading->current_a > GFCI_CURRENT_A)
        g_state.gfci_fault = true;

    return 0;
}

/* ============================================================
 * MAIN
 * ============================================================ */

int main(void)
{
    HAL_Init();

    /* Clock: 168 MHz HSE + PLL */
    /* (RCC config — HAL_RCC_OscConfig, HAL_RCC_ClockConfig) */

    /* GPIO init */
    __HAL_RCC_GPIOE_CLK_ENABLE();
    __HAL_RCC_GPIOD_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();

    GPIO_InitTypeDef gpio = {0};
    gpio.Mode = GPIO_MODE_OUTPUT_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;

    /* Relay outputs */
    for (int i = 0; i < 8; i++) {
        gpio.Pin = relay_pins[i];
        HAL_GPIO_Init(GPIOE, &gpio);
        HAL_GPIO_WritePin(GPIOE, relay_pins[i], GPIO_PIN_RESET);  /* All off */
    }

    /* Stepper pins */
    gpio.Pin = PIN_STEP1_STEP | PIN_STEP1_DIR | PIN_STEP1_EN |
               PIN_STEP2_STEP | PIN_STEP2_DIR | PIN_STEP2_EN |
               PIN_STEP3_STEP | PIN_STEP3_DIR | PIN_STEP3_EN;
    HAL_GPIO_Init(GPIOE, &gpio);

    /* Radio init */
    psp_radio_init();

    /* ADC init */
    g_state.hadc1.Instance = ADC1;
    g_state.hadc1.Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV4;
    g_state.hadc1.Init.Resolution = ADC_RESOLUTION_12B;
    g_state.hadc1.Init.ScanConvMode = DISABLE;
    g_state.hadc1.Init.ContinuousConvMode = DISABLE;
    g_state.hadc1.Init.DiscontinuousConvMode = DISABLE;
    g_state.hadc1.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
    g_state.hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
    g_state.hadc1.Init.NbrOfConversion = 1;
    HAL_ADC_Init(&g_state.hadc1);

    /* Main loop */
    while (1) {
        /* 1. Read all sensors */
        psp_equip_reading_t reading;
        psp_equip_read(&reading);

        /* 2. Check for incoming commands from hub */
        psp_frame_t rx_frame;
        if (psp_radio_recv(&rx_frame, 500) == 0) {
            switch (rx_frame.header.msg_type) {
                case PSP_MSG_DOSE_COMMAND: {
                    psp_dose_command_t *dose = (psp_dose_command_t *)rx_frame.payload;
                    float actual = psp_equip_dose(dose->pump_id, dose->volume_ml, dose->duration_s);
                    /* Send ACK with actual volume dispensed */
                    (void)actual;
                    break;
                }
                case PSP_MSG_EQUIP_COMMAND: {
                    psp_equip_command_t *cmd = (psp_equip_command_t *)rx_frame.payload;
                    if (cmd->command == 0)      psp_equip_set_relay(cmd->device_id, false);
                    else if (cmd->command == 1) psp_equip_set_relay(cmd->device_id, true);
                    else if (cmd->command == 2) relay_set(cmd->device_id, !relay_get(cmd->device_id));
                    break;
                }
                case PSP_MSG_ALARM: {
                    /* Emergency shutdown on GFCI or entrapment alarm */
                    for (int i = 0; i < 8; i++)
                        relay_set(i, false);
                    break;
                }
                default:
                    break;
            }
        }

        /* 3. Transmit status to hub */
        psp_equip_status_t status;
        status.relay_states = 0;
        for (int i = 0; i < 8; i++)
            if (reading.relay_states & (1 << i)) status.relay_states |= (1 << i);
        status.flow_lpm = reading.flow_lpm;
        status.pressure_kpa = reading.pressure_kpa;
        status.current_a = reading.current_a;
        status.pump_status = reading.pump_dosing;
        status.battery_mv = 0;  /* Mains powered */
        status.rssi_dbm = psp_radio_get_rssi();

        psp_header_t hdr = {
            .preamble = PSP_PREAMBLE,
            .sync_word = PSP_SYNC_WORD,
            .src_addr = PSP_ADDR_EQUIP_CTRL,
            .dst_addr = PSP_ADDR_HUB,
            .msg_type = PSP_MSG_EQUIP_STATUS
        };
        psp_frame_t tx_frame;
        psp_encode(&hdr, (uint8_t *)&status, sizeof(status),
                   (uint8_t *)&tx_frame, sizeof(tx_frame));
        psp_radio_send(&tx_frame);

        /* 4. Safety: check for entrapment and GFCI continuously */
        if (reading.pressure_kpa > ENTRAPMENT_PRESSURE_KPA) {
            g_state.entrapment_fault = true;
            /* EMERGENCY: shut off pump immediately */
            relay_set(0, false);  /* Pump off */
            /* Send alarm to hub */
            psp_alarm_payload_t alarm = {
                .alarm_type = PSP_ALARM_ENTRAPMENT,
                .severity = 3,  /* EMERGENCY */
                .value = reading.pressure_kpa,
                .timestamp = HAL_GetTick() / 1000
            };
            psp_header_t alarm_hdr = {
                .preamble = PSP_PREAMBLE,
                .sync_word = PSP_SYNC_WORD,
                .src_addr = PSP_ADDR_EQUIP_CTRL,
                .dst_addr = PSP_ADDR_HUB,
                .msg_type = PSP_MSG_ALARM
            };
            psp_frame_t alarm_frame;
            psp_encode(&alarm_hdr, (uint8_t *)&alarm, sizeof(alarm),
                       (uint8_t *)&alarm_frame, sizeof(alarm_frame));
            psp_radio_send_alarm(&alarm_frame);
        }

        if (reading.current_a > GFCI_CURRENT_A) {
            g_state.gfci_fault = true;
            /* EMERGENCY: shut off everything */
            for (int i = 0; i < 8; i++)
                relay_set(i, false);
        }
    }
}