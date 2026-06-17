/**
 * @file main.c
 * @brief SoundNest Smart Masking Speaker — Main entry point.
 *
 * Generates adaptive noise masking, nature soundscapes, and tinnitus
 * masking tones. Receives commands from hub via Sub-GHz mesh.
 * ESP32-S3-MINI-1 with PCM5102A DAC and MAX98306 amplifier.
 */

#include <stdio.h>
#include <string.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/i2s_std.h"
#include "driver/gpio.h"
#include "esp_log.h"

#include "../../../common/protocol/mesh_packet.h"

static const char *TAG = "MASKING_SPK";

/* ── Configuration ──────────────────────────────────────────────────── */

#define SAMPLE_RATE         44100   /* High quality audio output */
#define DMA_BUF_COUNT       8
#define DMA_BUF_SAMPLES     1024
#define I2S_PORT            I2S_NUM_0

/* Noise types */
#define NOISE_WHITE         0
#define NOISE_PINK          1
#define NOISE_BROWN         2

/* ── State ──────────────────────────────────────────────────────────── */

typedef struct {
    /* Masking state */
    masking_mode_t mode;
    uint8_t volume;          /* 0-100% */
    uint8_t stereo_balance;  /* 0-100 (50=center) */
    uint16_t freq_hz[2];    /* Tinnitus center freq range */
    uint8_t bandwidth;      /* Tinnitus bandwidth index */
    uint8_t fade_in_ms;     /* Fade in duration */
    uint8_t fade_out_ms;    /* Fade out duration */
    uint8_t duration_min;   /* Auto-stop after N minutes (0=forever) */
    uint8_t adaptive;       /* 1=adaptive volume */
    bool active;

    /* Audio state */
    float current_volume;    /* Smoothed volume for fade */
    float target_volume;     /* Target volume */

    /* Noise generators */
    uint32_t noise_seed;    /* PRNG seed */
    float pink_key[7];      /* Paul Kellet pink noise Voss-McCartney */
    int pink_idx;

    /* Nature synthesis */
    float rain_phase;
    float stream_phase;
    float forest_phase;
    float ocean_phase;
    uint32_t nature_tick;

    /* Tinnitus */
    float tinnitus_freq;
    float tinnitus_phase;

    /* Reference mic (adaptive feedback) */
    float ambient_spl;
    float mask_target_spl;

    /* Mesh */
    uint16_t node_addr;
    uint16_t hub_addr;
    uint16_t seq_num;
    bool mesh_joined;
} speaker_state_t;

static speaker_state_t g_state;

/* ── I2S Configuration ──────────────────────────────────────────────── */

static i2s_chan_handle_t tx_chan;

static esp_err_t init_i2s(void)
{
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_PORT, I2S_ROLE_MASTER);
    chan_cfg.dma_desc_num = DMA_BUF_COUNT;
    chan_cfg.dma_frame_num = DMA_BUF_SAMPLES;
    i2s_new_channel(&chan_cfg, &tx_chan, NULL);

    i2s_std_config_t std_cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(SAMPLE_RATE),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_32BIT,
                                                         I2S_SLOT_MODE_STEREO),
        .gpio_cfg = {
            .mclk = -1,  /* No MCLK for PCM5102A (uses internal PLL) */
            .bclk = GPIO_NUM_1,
            .ws = GPIO_NUM_2,
            .dout = GPIO_NUM_3,
            .din = GPIO_NUM_8,
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv = false,
            },
        },
    };

    ESP_ERROR_CHECK(i2s_channel_init_std_mode(tx_chan, &std_cfg));
    ESP_ERROR_CHECK(i2s_channel_enable(tx_chan));

    ESP_LOGI(TAG, "I2S initialized: %dHz, 32-bit, stereo", SAMPLE_RATE);
    return ESP_OK;
}

/* ── Noise Generation ─────────────────────────────────────────────────── */

/* Simple xorshift32 PRNG */
static uint32_t xorshift32(uint32_t *state)
{
    uint32_t x = *state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *state = x;
    return x;
}

/* Generate white noise sample: uniform distribution, [-1, 1] */
static float generate_white_noise(uint32_t *seed)
{
    uint32_t r = xorshift32(seed);
    return (float)((int32_t)r) / (float)0x7FFFFFFF;
}

/* Generate pink noise using Voss-McCartney algorithm */
static float generate_pink_noise(float key[7], int *idx)
{
    key[*idx] = generate_white_noise(&g_state.noise_seed);
    *idx = (*idx + 1) & 6;  /* Modulo 7 */

    float sum = 0.0f;
    for (int i = 0; i < 7; i++) {
        sum += key[i];
    }
    return sum / 7.0f;
}

/* Generate brown noise: integrated white noise */
static float generate_brown_noise(uint32_t *seed, float *last)
{
    float white = generate_white_noise(seed);
    *last = *last + (0.02f * white);
    /* Clamp to prevent overflow */
    if (*last > 1.0f) *last = 1.0f;
    if (*last < -1.0f) *last = -1.0f;
    return *last;
}

/* ── Nature Sound Synthesis ───────────────────────────────────────────── */

/* Simple rain: filtered noise with random amplitude modulation */
static float generate_rain(uint32_t *seed, float *phase)
{
    float base = generate_white_noise(seed) * 0.3f;
    /* Amplitude modulation for rain pattern */
    *phase += 0.0001f;
    float am = 0.5f + 0.5f * sinf(*phase * 3.7f);  /* Slow variation */
    return base * am;
}

/* Simple stream: filtered noise with gentle modulation */
static float generate_stream(uint32_t *seed, float *phase)
{
    float base = generate_white_noise(seed) * 0.2f;
    /* Low-pass filter effect (simple) */
    static float last = 0;
    last = last * 0.95f + base * 0.05f;
    *phase += 0.00005f;
    float modulation = 0.7f + 0.3f * sinf(*phase * 2.1f);
    return (base * 0.3f + last * 0.7f) * modulation;
}

/* Simple forest: sparse chirps + rustling */
static float generate_forest(uint32_t *seed, float *phase)
{
    float rustle = generate_white_noise(seed) * 0.05f;
    *phase += 0.00001f;
    /* Occasional chirp */
    if (xorshift32(seed) % 5000 == 0) {
        float chirp = sinf(*phase * 200.0f) * 0.3f;
        return chirp + rustle;
    }
    return rustle;
}

/* Simple ocean: low-frequency modulation of noise */
static float generate_ocean(uint32_t *seed, float *phase, uint32_t *tick)
{
    float base = generate_white_noise(seed) * 0.15f;
    *phase += 0.00002f;
    /* Wave pattern: ~6 second period */
    float wave = powf(sinf(*phase * 0.5f), 2.0f);
    /* Low-pass filter */
    static float last = 0;
    last = last * 0.97f + base * 0.03f;
    return (base * 0.2f + last * 0.8f) * wave;
}

/* ── Tinnitus Masking ──────────────────────────────────────────────────── */

static float generate_tinnitus_mask(float freq, float *phase, float bandwidth)
{
    /* Generate narrowband noise centered at tinnitus frequency
     * Slightly below the tinnitus frequency (1 octave below) for
     * effective masking per clinical guidelines */
    float mask_freq = freq * 0.5f;  /* 1 octave below */
    float phase_inc = 2.0f * M_PI * mask_freq / SAMPLE_RATE;

    /* Sine wave at mask frequency */
    float sine = sinf(*phase);

    /* Add bandwidth noise */
    float noise = generate_white_noise(&g_state.noise_seed) * bandwidth;

    *phase += phase_inc;
    if (*phase > 2.0f * M_PI * 100.0f) *phase -= 2.0f * M_PI * 100.0f;

    return (sine + noise) * 0.5f;
}

/* ── Privacy Masking ──────────────────────────────────────────────────── */

/* Speech-shaped noise: emphasizes speech frequency bands (250-4000Hz)
 * to effectively mask conversations from eavesdropping */
static float generate_privacy_mask(uint32_t *seed, float *phase)
{
    float noise = generate_white_noise(seed);

    /* Simple speech-shaping: emphasize 250-4000Hz range */
    /* In production, use proper bandpass IIR filter */
    static float filter_state = 0;
    filter_state = filter_state * 0.7f + noise * 0.3f;
    float shaped = noise * 0.4f + filter_state * 0.6f;

    *phase += 0.0001f;
    return shaped * 0.6f;
}

/* ── Audio Output Task ────────────────────────────────────────────────── */

static void audio_output_task(void *pvParameters)
{
    ESP_LOGI(TAG, "Audio output task started (%dHz)", SAMPLE_RATE);

    int32_t *dma_buf = malloc(DMA_BUF_SAMPLES * 2 * sizeof(int32_t));  /* Stereo */
    if (!dma_buf) {
        ESP_LOGE(TAG, "Failed to allocate DMA buffer");
        vTaskDelete(NULL);
    }

    float brown_last = 0.0f;

    while (1) {
        if (!g_state.active || g_state.mode == MASKING_OFF) {
            /* Output silence */
            memset(dma_buf, 0, DMA_BUF_SAMPLES * 2 * sizeof(int32_t));
            size_t bytes_written;
            i2s_channel_write(tx_chan, dma_buf, DMA_BUF_SAMPLES * 2 * sizeof(int32_t),
                              &bytes_written, pdMS_TO_TICKS(100));
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }

        /* Generate audio samples */
        for (int i = 0; i < DMA_BUF_SAMPLES; i++) {
            float sample_l = 0.0f;
            float sample_r = 0.0f;

            switch (g_state.mode) {
            case MASKING_WHITE_NOISE:
                sample_l = sample_r = generate_white_noise(&g_state.noise_seed);
                break;

            case MASKING_PINK_NOISE:
                sample_l = sample_r = generate_pink_noise(g_state.pink_key, &g_state.pink_idx);
                break;

            case MASKING_BROWN_NOISE:
                sample_l = sample_r = generate_brown_noise(&g_state.noise_seed, &brown_last);
                break;

            case MASKING_NATURE_RAIN:
                sample_l = sample_r = generate_rain(&g_state.noise_seed, &g_state.rain_phase);
                break;

            case MASKING_NATURE_STREAM:
                sample_l = sample_r = generate_stream(&g_state.noise_seed, &g_state.stream_phase);
                break;

            case MASKING_NATURE_FOREST:
                sample_l = sample_r = generate_forest(&g_state.noise_seed, &g_state.forest_phase);
                break;

            case MASKING_NATURE_OCEAN:
                sample_l = sample_r = generate_ocean(&g_state.noise_seed, &g_state.ocean_phase, &g_state.nature_tick);
                break;

            case MASKING_TINNITUS:
                sample_l = sample_r = generate_tinnitus_mask(
                    g_state.tinnitus_freq, &g_state.tinnitus_phase,
                    (float)g_state.bandwidth / 10.0f);
                break;

            case MASKING_PRIVACY:
                sample_l = generate_privacy_mask(&g_state.noise_seed, &g_state.rain_phase);
                sample_r = generate_privacy_mask(&g_state.noise_seed, &g_state.stream_phase);
                break;

            default:
                sample_l = sample_r = 0.0f;
                break;
            }

            /* Apply volume with smooth fade */
            g_state.current_volume += (g_state.target_volume - g_state.current_volume) * 0.001f;
            sample_l *= g_state.current_volume;
            sample_r *= g_state.current_volume;

            /* Apply stereo balance */
            float balance = (float)g_state.stereo_balance / 100.0f;
            float left_gain = cosf((1.0f - balance) * M_PI * 0.5f);
            float right_gain = sinf((1.0f - balance) * M_PI * 0.5f);
            sample_l *= left_gain;
            sample_r *= right_gain;

            /* Clamp to [-1, 1] */
            if (sample_l > 1.0f) sample_l = 1.0f;
            if (sample_l < -1.0f) sample_l = -1.0f;
            if (sample_r > 1.0f) sample_r = 1.0f;
            if (sample_r < -1.0f) sample_r = -1.0f;

            /* Convert to 32-bit signed integer for I2S */
            dma_buf[i * 2] = (int32_t)(sample_l * 2147483647.0f);
            dma_buf[i * 2 + 1] = (int32_t)(sample_r * 2147483647.0f);
        }

        /* Write to I2S */
        size_t bytes_written;
        esp_err_t ret = i2s_channel_write(tx_chan, dma_buf,
                                            DMA_BUF_SAMPLES * 2 * sizeof(int32_t),
                                            &bytes_written, pdMS_TO_TICKS(100));
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "I2S write error: %s", esp_err_to_name(ret));
        }

        /* Adaptive volume adjustment */
        if (g_state.adaptive && g_state.ambient_spl > 0) {
            /* Target: ambient + 10 dB */
            g_state.mask_target_spl = g_state.ambient_spl + 10.0f;
            /* Map target SPL to volume (simplified) */
            g_state.target_volume = fminf(fmaxf(g_state.mask_target_spl / 100.0f, 0.1f), 1.0f);
        } else {
            g_state.target_volume = (float)g_state.volume / 100.0f;
        }
    }
}

/* ── Reference Mic Task ──────────────────────────────────────────────── */

static void ref_mic_task(void *pvParameters)
{
    ESP_LOGI(TAG, "Reference mic task started");

    int16_t *mic_buf = malloc(1024 * sizeof(int16_t));
    if (!mic_buf) {
        ESP_LOGE(TAG, "Failed to allocate mic buffer");
        vTaskDelete(NULL);
    }

    while (1) {
        /* Read from reference microphone (SPH0645) */
        /* In production, use I2S to read from reference mic */
        /* For now, estimate ambient SPL from volume setting */

        /* Simple RMS-based SPL estimation */
        float rms = 0.01f;  /* Placeholder */
        float spl = 20.0f * log10f(rms / 0.00002f);  /* dB SPL */

        g_state.ambient_spl = spl;

        vTaskDelay(pdMS_TO_TICKS(1000));  /* Update every 1 second */
    }
}

/* ── Mesh Command Handler ─────────────────────────────────────────────── */

static void mesh_handler_task(void *pvParameters)
{
    ESP_LOGI(TAG, "Mesh handler task started");

    while (1) {
        /* In production, receive mesh packets via SX1262 */
        /* Process masking commands from hub */
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

/* ── Main Entry Point ─────────────────────────────────────────────────── */

void app_main(void)
{
    ESP_LOGI(TAG, "╔══════════════════════════════════════╗");
    ESP_LOGI(TAG, "║   SoundNest Masking Speaker v1.0    ║");
    ESP_LOGI(TAG, "║   Adaptive Noise Masking              ║");
    ESP_LOGI(TAG, "╚══════════════════════════════════════╝");

    /* Initialize state */
    memset(&g_state, 0, sizeof(g_state));
    g_state.mode = MASKING_OFF;
    g_state.volume = 50;
    g_state.stereo_balance = 50;
    g_state.noise_seed = 0xDEADBEEF;
    g_state.tinnitus_freq = 6000.0f;  /* Default: 6kHz */
    g_state.current_volume = 0.0f;
    g_state.target_volume = 0.0f;
    g_state.hub_addr = 0x0001;

    /* Initialize GPIO */
    gpio_set_direction(GPIO_NUM_9, GPIO_MODE_OUTPUT);   /* AMP_ENABLE */
    gpio_set_direction(GPIO_NUM_17, GPIO_MODE_OUTPUT);   /* LED_DATA */
    gpio_set_direction(GPIO_NUM_18, GPIO_MODE_OUTPUT);   /* IR_LED1 */
    gpio_set_direction(GPIO_NUM_19, GPIO_MODE_OUTPUT);   /* IR_LED2 */
    gpio_set_direction(GPIO_NUM_21, GPIO_MODE_INPUT);    /* BUTTON */

    /* Initialize I2S */
    init_i2s();

    /* Enable amplifier */
    gpio_set_level(GPIO_NUM_9, 1);

    /* Create tasks */
    xTaskCreate(audio_output_task, "audio_out", 8192, NULL, 5, NULL);
    xTaskCreate(ref_mic_task, "ref_mic", 4096, NULL, 3, NULL);
    xTaskCreate(mesh_handler_task, "mesh", 4096, NULL, 4, NULL);

    ESP_LOGI(TAG, "All tasks started. Masking speaker is running.");

    /* Main loop: monitor button and status */
    bool last_button = false;
    while (1) {
        /* Mode button: cycle through masking modes */
        bool button = gpio_get_level(GPIO_NUM_21) == 0;
        if (button && !last_button) {
            /* Cycle through modes */
            masking_mode_t modes[] = {
                MASKING_OFF, MASKING_WHITE_NOISE, MASKING_PINK_NOISE,
                MASKING_BROWN_NOISE, MASKING_NATURE_RAIN,
                MASKING_NATURE_STREAM, MASKING_NATURE_FOREST,
            };
            int num_modes = sizeof(modes) / sizeof(modes[0]);
            int current_idx = 0;

            for (int i = 0; i < num_modes; i++) {
                if (g_state.mode == modes[i]) {
                    current_idx = i;
                    break;
                }
            }
            g_state.mode = modes[(current_idx + 1) % num_modes];
            g_state.active = (g_state.mode != MASKING_OFF);

            ESP_LOGI(TAG, "Mode changed to: %s",
                     mesh_masking_mode_name(g_state.mode));
        }
        last_button = button;

        vTaskDelay(pdMS_TO_TICKS(50));
    }
}