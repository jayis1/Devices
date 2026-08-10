/*
 * FireSync — Panel Monitor Firmware
 * STM32G431CBU6, Bare-metal (HAL library)
 *
 * Monitors the electrical panel for arc faults and overheating.
 * Samples main breaker current at 8 kHz, runs 2048-point FFT,
 * classifies with ArcDetect CNN. Triggers shunt-trip breaker
 * on confirmed arc fault or thermal overload.
 *
 * Build: STM32CubeIDE / CMake with STM32G4 HAL
 */
#include "stm32g4xx.h"
#include "stm32g4xx_hal.h"
#include <string.h>
#include <math.h>
#include "../common/protocol.h"
#include "../common/subghz_mesh.h"
#include "../common/config.h"

/* === Globals === */
static fs_mesh_ctx_t g_mesh;
static fs_panel_telem_t g_telem;
static volatile uint16_t g_adc_buffer[3 * 8000]; /* 3 channels × 1 second at 8 kHz */
static volatile uint16_t g_adc_idx = 0;
static volatile uint8_t  g_adc_ready = 0;

/* Current samples for FFT (L1) */
static float g_current_samples[2048];
static float g_fft_magnitude[1024];

/* === SX1262 HAL (STM32 SPI) === */
static SPI_HandleTypeDef hspi1;

static int hal_spi_init(void)
{
    /* SPI1: MOSI=PB0, MISO=PB1, SCK=PB2, CS=PB3 (manual) */
    __HAL_RCC_SPI1_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();

    hspi1.Instance = SPI1;
    hspi1.Init.Mode = SPI_MODE_MASTER;
    hspi1.Init.Direction = SPI_DIRECTION_2LINES;
    hspi1.Init.DataSize = SPI_DATASIZE_8BIT;
    hspi1.Init.CLKPolarity = SPI_POLARITY_LOW;
    hspi1.Init.CLKPhase = SPI_PHASE_1EDGE;
    hspi1.Init.NSS = SPI_NSS_SOFT;
    hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_32; /* 170MHz/32 ≈ 5 MHz */
    hspi1.Init.FirstBit = SPI_FIRSTBIT_MSB;
    HAL_SPI_Init(&hspi1);

    /* GPIO for NSS (PB3), RST (PB5), DIO1 (PB4), BUSY (PB6) */
    GPIO_InitTypeDef gpio = {0};
    gpio.Mode = GPIO_MODE_OUTPUT_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_HIGH;

    gpio.Pin = PANEL_PIN_SX_NSS;
    HAL_GPIO_WritePin(GPIOB, gpio.Pin, GPIO_PIN_SET);
    HAL_GPIO_Init(GPIOB, &gpio);

    gpio.Pin = PANEL_PIN_SX_RST;
    HAL_GPIO_WritePin(GPIOB, gpio.Pin, GPIO_PIN_SET);
    HAL_GPIO_Init(GPIOB, &gpio);

    gpio.Mode = GPIO_MODE_INPUT;
    gpio.Pin = PANEL_PIN_SX_DIO1 | PANEL_PIN_SX_BUSY;
    HAL_GPIO_Init(GPIOB, &gpio);

    return 0;
}

static int hal_spi_xfer(const uint8_t *tx, uint8_t *rx, size_t len)
{
    if (tx && rx) {
        HAL_SPI_TransmitReceive(&hspi1, (uint8_t *)tx, rx, len, 100);
    } else if (tx) {
        HAL_SPI_Transmit(&hspi1, (uint8_t *)tx, len, 100);
    } else if (rx) {
        HAL_SPI_Receive(&hspi1, rx, len, 100);
    }
    return 0;
}

static void hal_cs_low(void)  { HAL_GPIO_WritePin(GPIOB, PANEL_PIN_SX_NSS, GPIO_PIN_RESET); }
static void hal_cs_high(void) { HAL_GPIO_WritePin(GPIOB, PANEL_PIN_SX_NSS, GPIO_PIN_SET); }
static void hal_reset(int a)  { HAL_GPIO_WritePin(GPIOB, PANEL_PIN_SX_RST, a ? GPIO_PIN_RESET : GPIO_PIN_SET); }
static int  hal_dio1_read(void) { return HAL_GPIO_ReadPin(GPIOB, PANEL_PIN_SX_DIO1); }
static int  hal_busy_read(void) { return HAL_GPIO_ReadPin(GPIOB, PANEL_PIN_SX_BUSY); }
static void hal_delay_ms(uint32_t ms) { HAL_Delay(ms); }
static void hal_delay_us(uint32_t us) {
    uint32_t ticks = (SystemCoreClock / 1000000) * us / 5;
    while (ticks--) __NOP();
}
static void hal_on_dio1(void) {}

static fs_radio_hal_t g_radio_hal = {
    .spi_init = hal_spi_init, .spi_xfer = hal_spi_xfer,
    .cs_low = hal_cs_low, .cs_high = hal_cs_high,
    .reset = hal_reset, .dio1_read = hal_dio1_read,
    .busy_read = hal_busy_read, .delay_ms = hal_delay_ms,
    .delay_us = hal_delay_us, .on_dio1 = hal_on_dio1,
};

/* === ADC for CT Clamps (8 kHz sampling) === */
static ADC_HandleTypeDef hadc1;
static TIM_HandleTypeDef htim3;

static void adc_init(void)
{
    /* ADC1 channels: PA0 (IN1, L1), PA1 (IN2, L2), PA2 (IN3, voltage) */
    __HAL_RCC_ADC12_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();

    GPIO_InitTypeDef gpio = {0};
    gpio.Mode = GPIO_MODE_ANALOG;
    gpio.Pull = GPIO_NOPULL;
    gpio.Pin = GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2;
    HAL_GPIO_Init(GPIOA, &gpio);

    hadc1.Instance = ADC1;
    hadc1.Init.ClockPrescaler = ADC_CLOCK_ASYNC_DIV8;
    hadc1.Init.Resolution = ADC_RESOLUTION_12B;
    hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
    hadc1.Init.ScanMode = ADC_SCAN_ENABLE;
    hadc1.Init.EOCSelection = ADC_EOC_SEQ_CONV;
    hadc1.Init.ContinuousConvMode = DISABLE;
    hadc1.Init.ExternalTrigConv = ADC_EXTERNALTRIG_T3_TRGO;
    hadc1.Init.DMAContinuousRequests = ENABLE;
    HAL_ADC_Init(&hadc1);

    /* Configure channels */
    ADC_ChannelConfTypeDef ch = {0};
    ch.Channel = ADC_CHANNEL_1; ch.Rank = 1; ch.SamplingTime = ADC_SAMPLETIME_2CYCLES_5;
    HAL_ADC_ConfigChannel(&hadc1, &ch);
    ch.Channel = ADC_CHANNEL_2; ch.Rank = 2;
    HAL_ADC_ConfigChannel(&hadc1, &ch);
    ch.Channel = ADC_CHANNEL_3; ch.Rank = 3;
    HAL_ADC_ConfigChannel(&hadc1, &ch);

    /* TIM3 for 8 kHz trigger (24 MHz / 3000 = 8 kHz) */
    __HAL_RCC_TIM3_CLK_ENABLE();
    htim3.Instance = TIM3;
    htim3.Init.Prescaler = 0;
    htim3.Init.CounterMode = TIM_COUNTERMODE_UP;
    htim3.Init.Period = 3000 - 1; /* 24 MHz / 3000 = 8 kHz */
    htim3.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    HAL_TIM_Base_Init(&htim3);

    TIM_MasterConfigTypeDef mcfg = {0};
    mcfg.MasterOutputTrigger = TIM_TRGO_UPDATE;
    HAL_TIMEx_MasterConfigSynchronization(&htim3, &mcfg);

    /* DMA for ADC */
    __HAL_RCC_DMA1_CLK_ENABLE();
    DMA_HandleTypeDef hdma_adc;
    hdma_adc.Instance = DMA1_Channel1;
    hdma_adc.Init.Request = DMA_REQUEST_ADC1;
    hdma_adc.Init.Direction = DMA_PERIPH_TO_MEMORY;
    hdma_adc.Init.PeriphInc = DMA_PINC_DISABLE;
    hdma_adc.Init.MemInc = DMA_MINC_ENABLE;
    hdma_adc.Init.PeriphDataAlignment = DMA_PDATAALIGN_HALFWORD;
    hdma_adc.Init.MemDataAlignment = DMA_MDATAALIGN_HALFWORD;
    hdma_adc.Init.Mode = DMA_CIRCULAR;
    hdma_adc.Init.Priority = DMA_PRIORITY_HIGH;
    HAL_DMA_Init(&hdma_adc);
    __HAL_LINKDMA(&hadc1, DMA_Handle, hdma_adc);

    HAL_NVIC_SetPriority(DMA1_Channel1_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ(DMA1_Channel1_IRQn);
}

static void adc_start(void)
{
    HAL_ADC_Start_DMA(&hadc1, (uint32_t *)g_adc_buffer, 3 * 8000);
    HAL_TIM_Base_Start(&htim3);
}

/* DMA complete callback */
void HAL_ADC_ConvHalfCpltCallback(ADC_HandleTypeDef *hadc)
{
    /* First half complete — process if needed */
}

void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc)
{
    g_adc_ready = 1;
}

/* === DS18B20 1-Wire (bitbang) === */
static void ds18b20_init_pin(uint8_t pin)
{
    GPIO_InitTypeDef gpio = {0};
    gpio.Pin = (1 << pin);
    gpio.Mode = GPIO_MODE_OUTPUT_OD;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOA, &gpio);
}

static int ds18b20_read_temp(uint8_t pin, int8_t *temp_c)
{
    /* Production: full 1-Wire protocol (reset → ROM skip → convert → read) */
    /* Placeholder: return ambient */
    *temp_c = 35; /* °C */
    return 0;
}

/* === FFT (Radix-2, 2048-point) === */
/* Production: use CMSIS-DSP arm_rfft_fast_f32 */
static void fft_2048(const float *input, float *magnitude)
{
    /* Production: arm_rfft_fast_f32(&fft_inst, input, fft_out, 0); */
    /* Placeholder: simple magnitude approximation */
    for (int i = 0; i < 1024; i++) {
        float re = 0, im = 0;
        for (int j = 0; j < 2048; j++) {
            float angle = -2.0f * M_PI * i * j / 2048.0f;
            re += input[j] * cosf(angle);
            im += input[j] * sinf(angle);
        }
        magnitude[i] = sqrtf(re * re + im * im);
    }
}

/* === ArcDetect CNN (TFLite-Micro) === */
static int arcdetect_infer(const float *fft_mag, uint8_t *class_out,
                            uint8_t *confidence_out)
{
    /* Production: TFLite-Micro int8 model
     * Input: 1024 FFT frequency bins (0-4 kHz)
     * Conv1D(32,k=7)+ReLU+MaxPool → Conv1D(16,k=5)+ReLU+MaxPool
     * → Dense(32)+ReLU → Dense(4)+Softmax
     */

    /* Heuristic placeholder: check for arc signatures */
    /* Series arcs: broadband high-freq noise (2-4 kHz) */
    /* Parallel arcs: sharp peaks at 60 Hz harmonics */
    float hf_energy = 0;
    for (int i = 512; i < 1024; i++) hf_energy += fft_mag[i];

    float lf_energy = 0;
    for (int i = 0; i < 100; i++) lf_energy += fft_mag[i];

    float total = hf_energy + lf_energy;
    if (total < 0.001f) total = 0.001f;
    float hf_ratio = hf_energy / total;

    if (hf_ratio > 0.6f) {
        *class_out = FS_ARC_SERIES;
        *confidence_out = 85;
    } else if (hf_ratio > 0.3f && lf_energy > 1000) {
        *class_out = FS_ARC_PARALLEL;
        *confidence_out = 78;
    } else {
        *class_out = FS_ARC_NORMAL;
        *confidence_out = 92;
    }
    return 0;
}

/* === Shunt-Trip Relay === */
static void shunt_trip(void)
{
    /* Energize shunt-trip relay (PB7) for 500 ms */
    HAL_GPIO_WritePin(GPIOB, 1 << PANEL_PIN_SHUNT_TRIP, GPIO_PIN_SET);
    HAL_Delay(500);
    HAL_GPIO_WritePin(GPIOB, 1 << PANEL_PIN_SHUNT_TRIP, GPIO_PIN_RESET);
    g_telem.shunt_tripped = 1;
}

/* === Fire Alert === */
static void send_fire_alert(uint8_t fire_class, uint8_t confidence)
{
    fs_message_t msg;
    fs_build_fire_alert(&msg, g_mesh.node_id, g_mesh.msg_counter++,
                       fire_class, confidence, 0xFE, /* Room 0xFE = panel */
                       0, 0, 0, 0, 0);
    fs_mesh_send_emergency(&g_mesh, &g_radio_hal, &msg);
}

/* === Panel Monitor Task === */
static void panel_task(void)
{
    memset(&g_telem, 0, sizeof(g_telem));
    g_telem.subtype = FS_TELEM_PANEL;
    g_telem.battery_v = 420;

    while (1) {
        if (g_adc_ready) {
            g_adc_ready = 0;

            /* Extract L1 current samples (every 3rd sample starting at 0) */
            for (int i = 0; i < 2048; i++) {
                uint16_t raw = g_adc_buffer[i * 3];
                /* Convert to current: (raw - 2048) × scale */
                /* SCT-013-100: 100A → 50mA → 100Ω burden → 5V → 12-bit ADC */
                g_current_samples[i] = ((float)raw - 2048.0f) / 2048.0f * 100.0f;
            }

            /* Compute RMS current */
            float sum_sq = 0;
            for (int i = 0; i < 2048; i++)
                sum_sq += g_current_samples[i] * g_current_samples[i];
            float rms = sqrtf(sum_sq / 2048.0f);
            g_telem.main_current_x100 = (uint16_t)(rms * 100);

            /* Compute voltage (channel 3) */
            float v_sum = 0;
            for (int i = 0; i < 2048; i++) {
                v_sum += (float)g_adc_buffer[i * 3 + 2] / 2048.0f * 240.0f;
            }
            g_telem.voltage_x10 = (uint16_t)(v_sum / 2048.0f * 10);

            /* Power */
            g_telem.power_x10 = (uint16_t)(rms * (g_telem.voltage_x10 / 10.0f) * 10);

            /* FFT */
            fft_2048(g_current_samples, g_fft_magnitude);

            /* ArcDetect CNN */
            uint8_t arc_class, arc_conf;
            arcdetect_infer(g_fft_magnitude, &arc_class, &arc_conf);
            g_telem.arc_fault_class = arc_class;
            g_telem.arc_confidence = arc_conf;

            /* Action on arc fault */
            if ((arc_class == FS_ARC_SERIES || arc_class == FS_ARC_PARALLEL) &&
                arc_conf >= FS_ARC_CONFIDENCE_PCT) {
                /* Shunt trip! */
                shunt_trip();
                send_fire_alert(FS_CLASS_FLAMING_FIRE, arc_conf);
            }

            /* Read thermal sensors */
            int8_t bus_temp, neutral_temp, brk1_temp, brk2_temp;
            ds18b20_read_temp(PANEL_PIN_TEMP_BUS, &bus_temp);
            ds18b20_read_temp(PANEL_PIN_TEMP_NEUT, &neutral_temp);
            ds18b20_read_temp(PANEL_PIN_TEMP_BRK1, &brk1_temp);
            ds18b20_read_temp(PANEL_PIN_TEMP_BRK2, &brk2_temp);

            g_telem.bus_bar_temp_c = bus_temp;
            g_telem.breaker_temp_c =
                (brk1_temp > brk2_temp) ? brk1_temp : brk2_temp;

            /* Thermal overload protection */
            if (bus_temp > FS_BUS_BAR_TRIP_C ||
                brk1_temp > FS_BREAKER_TRIP_C ||
                brk2_temp > FS_BREAKER_TRIP_C) {
                shunt_trip();
                send_fire_alert(FS_CLASS_SMOLDERING, 80);
            } else if (bus_temp > FS_BUS_BAR_WARN_C) {
                /* Warning only — no trip yet */
            }
        }

        HAL_Delay(100); /* Check every 100 ms */
    }
}

/* === Telemetry Task === */
static void telemetry_task(void)
{
    while (1) {
        HAL_Delay(PANEL_TELEM_INTERVAL_MS);

        g_telem.rssi = g_mesh.last_rssi;

        fs_message_t msg;
        fs_build_panel_telem(&msg, g_mesh.node_id, g_mesh.msg_counter++, &g_telem);
        fs_mesh_send(&g_mesh, &g_radio_hal, &msg, 0);
    }
}

/* === Radio RX Task === */
static void radio_rx_task(void)
{
    uint8_t rx_buf[FS_MAX_MSG];
    fs_message_t msg;

    while (1) {
        int rx_len = fs_sx1262_rx(&g_radio_hal, rx_buf, sizeof(rx_buf),
                                   4000, &g_mesh.last_rssi);

        if (rx_len > 0 && fs_decode(&msg, rx_buf, rx_len) == 0) {
            switch (msg.header.type) {
            case FS_MSG_PANEL_SHUTOFF:
                shunt_trip();
                break;

            case FS_MSG_COMMAND:
                if (msg.payload[0] == FS_CMD_TRIP_BREAKER) {
                    shunt_trip();
                } else if (msg.payload[0] == FS_CMD_RESET_BREAKER) {
                    g_telem.shunt_tripped = 0;
                    /* Production: reset breaker (manual reset required) */
                }
                break;
            }
        }
    }
}

/* === Main === */
int main(void)
{
    /* HAL init */
    HAL_Init();
    SystemClock_Config();

    /* GPIO for shunt trip (PB7), LED (PB8) */
    __HAL_RCC_GPIOB_CLK_ENABLE();
    GPIO_InitTypeDef gpio = {0};
    gpio.Mode = GPIO_MODE_OUTPUT_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    gpio.Pin = (1 << PANEL_PIN_SHUNT_TRIP) | (1 << PANEL_PIN_LED);
    HAL_GPIO_Init(GPIOB, &gpio);

    /* ADC + DMA for CT clamps */
    adc_init();
    adc_start();

    /* DS18B20 1-Wire pins (PA4-PA7) */
    __HAL_RCC_GPIOA_CLK_ENABLE();
    ds18b20_init_pin(PANEL_PIN_TEMP_BUS);
    ds18b20_init_pin(PANEL_PIN_TEMP_NEUT);
    ds18b20_init_pin(PANEL_PIN_TEMP_BRK1);
    ds18b20_init_pin(PANEL_PIN_TEMP_BRK2);

    /* Radio */
    fs_radio_config_t radio_cfg = {
        .freq_hz = FS_SUBGHZ_FREQ_HZ,
        .spreading_factor = FS_SUBGHZ_SF,
        .bandwidth_hz = FS_SUBGHZ_BW_HZ,
        .tx_power_dbm = FS_SUBGHZ_TX_POWER_DBM,
        .sync_word = FS_SYNC_WORD,
        .preamble_len = FS_SUBGHZ_PREAMBLE,
    };

    hal_spi_init();
    fs_sx1262_init(&g_radio_hal, &radio_cfg);

    /* Mesh */
    uint8_t aes_key[16] = {0};
    fs_mesh_init(&g_mesh, 0x11, FS_NODE_PANEL, aes_key);
    g_mesh.battery_v = 420;

    if (fs_mesh_join(&g_mesh, &g_radio_hal) == 0) {
        /* Joined */
    }

    /* Simple cooperative scheduler (no RTOS on bare-metal) */
    while (1) {
        panel_task();
        telemetry_task();
        radio_rx_task();
    }
}