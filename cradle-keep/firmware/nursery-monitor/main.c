/**
 * CradleKeep — Nursery Monitor Node Firmware (ESP32-S3)
 * 
 * Wall-mounted unit with camera, dual MEMS microphones, and
 * environmental sensors. Runs TinyML cry classification on-device.
 * 
 * Key features:
 * - Real-time cry detection and classification (5 categories)
 * - Dual-mic beamforming for cry direction
 * - Night vision with IR LEDs (invisible 940nm)
 * - Environmental monitoring (temp, humidity, CO2, VOC, light, noise)
 * - Video stream on-demand via WiFi (RTSP)
 * - Sub-GHz mesh backup for safety data
 */

#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "esp_camera.h"
#include "driver/i2s.h"
#include "driver/i2c.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

/* ── TFLite Micro Includes ───────────────────────────────────────────── */
#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/micro/micro_mutable_op_resolver.h"
#include "tensorflow/lite/schema/schema_generated.h"

/* ── Pin Definitions (ESP32-S3) ──────────────────────────────────────── */
#define PIN_CAM_SCL       1
#define PIN_CAM_SDA       2
#define PIN_CAM_D0        11
#define PIN_CAM_D1        12
#define PIN_CAM_D2        13
#define PIN_CAM_D3        14
#define PIN_CAM_D4        15
#define PIN_CAM_D5        16
#define PIN_CAM_D6        17
#define PIN_CAM_D7        18
#define PIN_CAM_PCLK      12
#define PIN_CAM_VSYNC     13
#define PIN_CAM_HREF      14
#define PIN_CAM_XCLK      15
#define PIN_IR_LED_PWM     4
#define PIN_IR_CUT_EN      5
#define PIN_MIC1_BCLK      6
#define PIN_MIC1_LRCLK     7
#define PIN_MIC1_DOUT      8
#define PIN_MIC2_DOUT      9
#define PIN_ENV_SDA       10
#define PIN_ENV_SCL       11
#define PIN_RADIO_SCK     37
#define PIN_RADIO_MOSI    38
#define PIN_RADIO_MISO    39
#define PIN_RADIO_NSS     40
#define PIN_RADIO_IRQ     41
#define PIN_RADIO_BUSY    42
#define PIN_RADIO_RST     45

/* ── Constants ─────────────────────────────────────────────────────── */
#define SAMPLE_RATE          16000
#define FFT_SIZE             512
#define MEL_BANDS            64
#define AUDIO_WINDOW_MS      1000   /* 1-second windows for cry detection */
#define CRY_CONFIDENCE_THRESH 128   /* 0-255, above this = confident classification */
#define ENV_READ_INTERVAL_MS 10000  /* Read env sensors every 10 seconds */
#define CAM_RESOLUTION       FRAMESIZE_QVGA  /* 320x240 for on-device */
#define I2S_NUM              I2S_NUM_0
#define I2C_NUM              I2C_NUM_0

/* ── Sensor I2C Addresses ───────────────────────────────────────────── */
#define SHT40_ADDR           0x44
#define SCD30_ADDR           0x61
#define SGP40_ADDR           0x59
#define VEML7700_ADDR        0x10
#define OV5640_ADDR          0x3C

/* ── Cry Classification Model ────────────────────────────────────────── */
/* MobileNetV1 0.25 quantized model for 5-class cry classification */
/* Input: 1-channel mel-spectrogram 64x64 */
/* Output: 6 classes (none + 5 cry types) */
extern const unsigned char g_cry_model_data[];
extern const unsigned int g_cry_model_len;

/* ── Audio Processing ─────────────────────────────────────────────────── */
typedef struct {
    float mel_spectrogram[MEL_BANDS * 64];  /* 64 time frames × 64 mel bands */
    uint8_t cry_type;
    uint8_t cry_confidence;
    uint8_t cry_intensity;
} audio_result_t;

typedef struct {
    float temperature_c;
    float humidity_pct;
    uint16_t co2_ppm;
    uint16_t voc_index;
    uint16_t light_lux;
    uint8_t noise_level_db;
} env_data_t;

typedef struct {
    audio_result_t audio;
    env_data_t env;
    uint8_t ir_active;
    uint8_t camera_ready;
    uint8_t baby_present;
    uint8_t alert_level;
    uint8_t battery_pct;
    uint8_t sound_playing;
} nursery_state_t;

nursery_state_t state;

/* ── Audio Buffer ──────────────────────────────────────────────────────── */
static int16_t audio_buffer[SAMPLE_RATE];  /* 1 second at 16kHz */
static int16_t mic1_buffer[SAMPLE_RATE];
static int16_t mic2_buffer[SAMPLE_RATE];
static uint16_t audio_buffer_idx = 0;

/* ── TFLite Micro Interpreter ──────────────────────────────────────────── */
static const tflite::Model *model = nullptr;
static tflite::MicroInterpreter *interpreter = nullptr;
static TfLiteTensor *input_tensor = nullptr;
static TfLiteTensor *output_tensor = nullptr;

/* ── Mel-Spectrogram Computation ───────────────────────────────────────── */
/**
 * Compute mel-spectrogram from audio buffer.
 * Simplified version: Hann window + FFT + mel filterbank + log
 */
void compute_mel_spectrogram(const int16_t *audio, int audio_len, 
                              float *mel_output, int mel_bands, int time_frames) {
    /* For each time frame:
     * 1. Apply Hann window
     * 2. Compute FFT magnitude
     * 3. Apply mel filterbank
     * 4. Take log
     */
    int hop_size = (audio_len - FFT_SIZE) / time_frames;
    if (hop_size < 1) hop_size = 1;
    
    /* Mel filterbank center frequencies (64 bands, 0-8000 Hz) */
    float mel_low = 0.0f;
    float mel_high = 2595.0f * log10f(1.0f + 8000.0f / 700.0f);
    
    float mel_centers[MEL_BANDS];
    for (int i = 0; i < MEL_BANDS; i++) {
        float mel = mel_low + (mel_high - mel_low) * (i + 1) / (MEL_BANDS + 1);
        mel_centers[i] = 700.0f * (powf(10, mel / 2595.0f) - 1.0f);
    }
    
    for (int frame = 0; frame < time_frames; frame++) {
        int offset = frame * hop_size;
        
        /* Apply Hann window and compute FFT */
        float windowed[FFT_SIZE];
        float real[FFT_SIZE];
        float imag[FFT_SIZE];
        
        for (int i = 0; i < FFT_SIZE; i++) {
            float hann = 0.5f * (1.0f - cosf(2.0f * 3.14159f * i / (FFT_SIZE - 1)));
            int idx = offset + i;
            if (idx < audio_len) {
                windowed[i] = (float)audio[idx] * hann / 32768.0f;
            } else {
                windowed[i] = 0.0f;
            }
            real[i] = windowed[i];
            imag[i] = 0.0f;
        }
        
        /* Simplified FFT (in practice, use ESP-DSP library) */
        /* Compute magnitude spectrum */
        float magnitude[FFT_SIZE / 2];
        for (int k = 0; k < FFT_SIZE / 2; k++) {
            /* Placeholder: in production, use Cooley-Tukey FFT */
            magnitude[k] = windowed[k] > 0 ? windowed[k] : -windowed[k];
        }
        
        /* Apply mel filterbank */
        for (int mel = 0; mel < mel_bands; mel++) {
            float mel_energy = 0.0f;
            for (int k = 0; k < FFT_SIZE / 2; k++) {
                float freq = (float)k * SAMPLE_RATE / FFT_SIZE;
                /* Triangular filter centered on mel_centers[mel] */
                float center = mel_centers[mel];
                float half_width = (mel_centers[1] - mel_centers[0]);
                if (freq >= center - half_width && freq <= center + half_width) {
                    float weight = 1.0f - fabsf(freq - center) / half_width;
                    mel_energy += magnitude[k] * weight;
                }
            }
            /* Log mel energy */
            mel_output[frame * mel_bands + mel] = logf(mel_energy + 1e-6f);
        }
    }
}

/* ── Cry Classification ────────────────────────────────────────────────── */
/**
 * Run cry classification model on latest audio window.
 * Returns cry type (CRY_*) and confidence (0-255).
 */
void classify_cry(const int16_t *audio, int audio_len, uint8_t *cry_type, uint8_t *confidence) {
    /* Compute mel-spectrogram */
    float mel_spec[MEL_BANDS * 64];
    compute_mel_spectrogram(audio, audio_len, mel_spec, MEL_BANDS, 64);
    
    /* Quantize to INT8 for TFLite model input */
    int8_t input_quantized[MEL_BANDS * 64];
    for (int i = 0; i < MEL_BANDS * 64; i++) {
        /* Scale to INT8 range */
        float normalized = (mel_spec[i] + 6.0f) / 12.0f;  /* Assume range [-6, 6] */
        if (normalized < 0.0f) normalized = 0.0f;
        if (normalized > 1.0f) normalized = 1.0f;
        input_quantized[i] = (int8_t)(normalized * 254.0f - 127.0f);
    }
    
    /* Copy to TFLite input tensor */
    if (input_tensor) {
        int8_t *input_data = input_tensor->data.int8;
        memcpy(input_data, input_quantized, sizeof(input_quantized));
        
        /* Run inference */
        TfLiteStatus status = interpreter->Invoke();
        if (status == kTfLiteOk) {
            /* Get output */
            int8_t *output_data = output_tensor->data.int8;
            
            /* Find class with maximum score */
            int max_idx = 0;
            int8_t max_val = output_data[0];
            for (int i = 1; i < 6; i++) {
                if (output_data[i] > max_val) {
                    max_val = output_data[i];
                    max_idx = i;
                }
            }
            
            /* Convert to confidence (0-255) */
            float scale = output_tensor->params.scale;
            int zero_point = output_tensor->params.zero_point;
            float conf = (max_val - zero_point) * scale;
            *confidence = (uint8_t)(conf * 255.0f);
            if (*confidence > 255) *confidence = 255;
            
            /* Map class index to cry type */
            /* Model output: [none, hungry, tired, pain, colic, discomfort] */
            switch (max_idx) {
                case 0: *cry_type = CRY_NONE; break;
                case 1: *cry_type = CRY_HUNGRY; break;
                case 2: *cry_type = CRY_TIRED; break;
                case 3: *cry_type = CRY_PAIN; break;
                case 4: *cry_type = CRY_COLIC; break;
                case 5: *cry_type = CRY_DISCOMFORT; break;
                default: *cry_type = CRY_NONE; break;
            }
        }
    }
    
    /* Fallback: simple energy-based detection if ML model not loaded */
    if (!input_tensor) {
        float energy = 0.0f;
        for (int i = 0; i < audio_len; i++) {
            float sample = (float)audio[i] / 32768.0f;
            energy += sample * sample;
        }
        energy /= audio_len;
        
        if (energy > 0.01f) {  /* Sound detected */
            *cry_type = CRY_DISCOMFORT;  /* Unknown cry type */
            *confidence = (uint8_t)(energy * 255.0f * 100.0f);
            if (*confidence > 200) *confidence = 200;  /* Cap at ~78% */
        } else {
            *cry_type = CRY_NONE;
            *confidence = 0;
        }
    }
}

/* ── I2S Audio Capture ─────────────────────────────────────────────────── */
void i2s_audio_init(void) {
    i2s_config_t i2s_config = {
        .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
        .sample_rate = SAMPLE_RATE,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
        .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count = 8,
        .dma_buf_len = 1024,
        .use_apll = false,
        .tx_desc_auto_clear = false,
        .fixed_mclk = 0,
    };
    
    i2s_pin_config_t pin_config = {
        .mck_io_num = I2S_PIN_NO_CHANGE,
        .bck_io_num = PIN_MIC1_BCLK,
        .ws_io_num = PIN_MIC1_LRCLK,
        .data_out_num = I2S_PIN_NO_CHANGE,
        .data_in_num = PIN_MIC1_DOUT,
    };
    
    i2s_driver_install(I2S_NUM, &i2s_config, 0, NULL);
    i2s_set_pin(I2S_NUM, &pin_config);
}

/* ── Environmental Sensors ──────────────────────────────────────────────── */
void read_env_sensors(env_data_t *env) {
    uint8_t buf[6];
    
    /* SHT40: Temperature + Humidity */
    uint8_t cmd_sht40 = 0xFD;
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, SHT40_ADDR << 1 | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, cmd_sht40, true);
    i2c_master_stop(cmd);
    i2c_master_cmd_begin(I2C_NUM, cmd, 100 / portTICK_PERIOD_MS);
    i2c_cmd_link_delete(cmd);
    
    vTaskDelay(10 / portTICK_PERIOD_MS);
    
    cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, SHT40_ADDR << 1 | I2C_MASTER_READ, true);
    i2c_master_read(cmd, buf, 6, I2C_MASTER_LAST_NACK);
    i2c_master_stop(cmd);
    i2c_master_cmd_begin(I2C_NUM, cmd, 100 / portTICK_PERIOD_MS);
    i2c_cmd_link_delete(cmd);
    
    uint16_t temp_raw = (buf[0] << 8) | buf[1];
    uint16_t hum_raw = (buf[3] << 8) | buf[4];
    env->temperature_c = -45.0f + 175.0f * (float)temp_raw / 65535.0f;
    env->humidity_pct = 0.0f + 100.0f * (float)hum_raw / 65535.0f;
    
    /* SCD30: CO2 (reads are slow, use cached value) */
    /* In practice: periodic reading every 30 seconds */
    env->co2_ppm = 420;  /* Placeholder - would be updated from sensor */
    
    /* SGP40: VOC Index */
    uint8_t sgp40_cmd[] = {0x26, 0x0F};  /* Measure VOC */
    cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, SGP40_ADDR << 1 | I2C_MASTER_WRITE, true);
    i2c_master_write(cmd, sgp40_cmd, 2, true);
    i2c_master_stop(cmd);
    i2c_master_cmd_begin(I2C_NUM, cmd, 100 / portTICK_PERIOD_MS);
    i2c_cmd_link_delete(cmd);
    
    vTaskDelay(50 / portTICK_PERIOD_MS);
    
    /* VEML7700: Ambient Light */
    /* Simplified - would read register 0x04 for lux value */
    env->light_lux = 50;  /* Placeholder */
    
    /* Noise level from audio energy */
    /* Computed during audio processing */
}

/* ── IR LED Control ──────────────────────────────────────────────────────── */
void ir_leds_set_brightness(uint8_t duty_pct) {
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, duty_pct * 255 / 100);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
}

void ir_cut_filter_enable(bool enable) {
    gpio_set_level(PIN_IR_CUT_EN, enable ? 1 : 0);
}

/* ── Camera ──────────────────────────────────────────────────────────────── */
void camera_init(void) {
    camera_config_t config = {
        .pin_pwdn = -1,
        .pin_reset = -1,
        .pin_xclk = PIN_CAM_XCLK,
        .pin_sccb_sda = PIN_CAM_SDA,
        .pin_sccb_scl = PIN_CAM_SCL,
        .pin_d7 = PIN_CAM_D7,
        .pin_d6 = PIN_CAM_D6,
        .pin_d5 = PIN_CAM_D5,
        .pin_d4 = PIN_CAM_D4,
        .pin_d3 = PIN_CAM_D3,
        .pin_d2 = PIN_CAM_D2,
        .pin_d1 = PIN_CAM_D1,
        .pin_d0 = PIN_CAM_D0,
        .pin_vsync = PIN_CAM_VSYNC,
        .pin_href = PIN_CAM_HREF,
        .pin_pclk = PIN_CAM_PCLK,
        .xclk_freq_hz = 20000000,
        .ledc_timer = LEDC_TIMER_0,
        .ledc_channel = LEDC_CHANNEL_0,
        .pixel_format = PIXFORMAT_JPEG,
        .frame_size = FRAMESIZE_QVGA,
        .jpeg_quality = 12,
        .fb_count = 2,
        .grab_mode = CAMERA_GRAB_LATEST,
    };
    
    esp_camera_init(&config);
}

/* ── Transmit to Hub via Sub-GHz ────────────────────────────────────────── */
void transmit_to_hub(void) {
    nursery_data_t data = {
        .cry_type = state.audio.cry_type,
        .cry_confidence = state.audio.cry_confidence,
        .cry_intensity = state.audio.cry_intensity,
        .room_temp_c_x10 = (int16_t)(state.env.temperature_c * 10),
        .room_humidity_x10 = (uint16_t)(state.env.humidity_pct * 10),
        .co2_ppm = state.env.co2_ppm,
        .voc_index = state.env.voc_index,
        .light_lux = state.env.light_lux,
        .noise_level_db = state.env.noise_level_db,
        .ir_active = state.ir_active,
        .camera_ready = state.camera_ready,
        .baby_present = state.baby_present,
        .alert_level = state.alert_level,
        .battery_pct = state.battery_pct,
        .signal_strength = 0,
        .sound_type_playing = state.sound_playing,
        .sound_duration_s = 0,
    };
    
    packet_t pkt = {
        .src = ADDR_NURSERY_MONITOR,
        .dst = ADDR_HUB,
        .type = PKT_NURSERY_DATA,
        .payload_len = sizeof(data),
    };
    memcpy(pkt.payload, &data, sizeof(data));
    
    radio_send(&pkt);
}

/* ── Audio Task (runs on Core 1) ──────────────────────────────────────── */
void audio_task(void *pvParameters) {
    int16_t rx_buffer[256];
    size_t bytes_read;
    
    while (1) {
        /* Read audio from I2S */
        i2s_read(I2S_NUM, rx_buffer, sizeof(rx_buffer), &bytes_read, portMAX_DELAY);
        
        /* Copy to audio buffer */
        int samples_read = bytes_read / 2;
        for (int i = 0; i < samples_read && audio_buffer_idx < SAMPLE_RATE; i++) {
            /* Use mic 1 (left channel) */
            audio_buffer[audio_buffer_idx++] = rx_buffer[i * 2];
        }
        
        /* When we have 1 second of audio, classify */
        if (audio_buffer_idx >= SAMPLE_RATE) {
            uint8_t cry_type, confidence;
            classify_cry(audio_buffer, SAMPLE_RATE, &cry_type, &confidence);
            
            /* Update state */
            state.audio.cry_type = cry_type;
            state.audio.cry_confidence = confidence;
            
            /* Calculate intensity (RMS) */
            float rms = 0.0f;
            for (int i = 0; i < SAMPLE_RATE; i++) {
                float sample = (float)audio_buffer[i] / 32768.0f;
                rms += sample * sample;
            }
            rms = sqrtf(rms / SAMPLE_RATE);
            state.audio.cry_intensity = (uint8_t)(rms * 255.0f * 10.0f);
            if (state.audio.cry_intensity > 255) state.audio.cry_intensity = 255;
            
            /* Send cry event if detected */
            if (cry_type != CRY_NONE && confidence > CRY_CONFIDENCE_THRESH) {
                cry_event_t event = {
                    .cry_type = cry_type,
                    .cry_confidence = confidence,
                    .cry_intensity = state.audio.cry_intensity,
                    .duration_s = 1,  /* 1-second window */
                    .preceding_sleep = 0,  /* Filled by hub */
                    .time_since_feed_m = 0,  /* Filled by hub */
                    .time_since_sleep_m = 0,  /* Filled by hub */
                    .timestamp_ms = (uint32_t)(esp_timer_get_time() / 1000),
                };
                
                packet_t pkt = {
                    .src = ADDR_NURSERY_MONITOR,
                    .dst = ADDR_HUB,
                    .type = PKT_CRY_EVENT,
                    .payload_len = sizeof(event),
                };
                memcpy(pkt.payload, &event, sizeof(event));
                radio_send(&pkt);
            }
            
            /* Reset buffer */
            audio_buffer_idx = 0;
        }
        
        /* Update noise level */
        float noise_rms = 0.0f;
        int count = samples_read > 256 ? 256 : samples_read;
        for (int i = 0; i < count; i++) {
            float s = (float)rx_buffer[i * 2] / 32768.0f;
            noise_rms += s * s;
        }
        if (count > 0) {
            noise_rms = sqrtf(noise_rms / count);
            /* Convert RMS to approximate dB SPL */
            state.env.noise_level_db = (uint8_t)(20.0f * log10f(noise_rms + 1e-10f) + 94.0f);
        }
        
        /* Auto IR LEDs based on light level */
        if (state.env.light_lux < 5) {
            ir_leds_set_brightness(80);  /* 80% brightness */
            ir_cut_filter_enable(true);
            state.ir_active = 1;
        } else if (state.env.light_lux < 30) {
            ir_leds_set_brightness(30);  /* 30% brightness */
            ir_cut_filter_enable(true);
            state.ir_active = 1;
        } else {
            ir_leds_set_brightness(0);
            ir_cut_filter_enable(false);
            state.ir_active = 0;
        }
    }
}

/* ── Environment Task ────────────────────────────────────────────────────── */
void env_task(void *pvParameters) {
    while (1) {
        read_env_sensors(&state.env);
        vTaskDelay(pdMS_TO_TICKS(ENV_READ_INTERVAL_MS));
    }
}

/* ── Transmit Task ──────────────────────────────────────────────────────── */
void transmit_task(void *pvParameters) {
    while (1) {
        transmit_to_hub();
        vTaskDelay(pdMS_TO_TICKS(2000));  /* Transmit every 2 seconds */
    }
}

/* ── Main ─────────────────────────────────────────────────────────────────── */
void app_main(void) {
    ESP_LOGI("NurseryMonitor", "Starting CradleKeep Nursery Monitor");
    
    /* Initialize state */
    memset(&state, 0, sizeof(state));
    state.camera_ready = 1;
    
    /* Initialize I2C for environmental sensors */
    i2c_config_t i2c_config = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = PIN_ENV_SDA,
        .scl_io_num = PIN_ENV_SCL,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = 100000,
    };
    i2c_param_config(I2C_NUM, &i2c_config);
    i2c_driver_install(I2C_NUM, I2C_MODE_MASTER, 0, 0, 0);
    
    /* Initialize IR LED PWM */
    ledc_timer_config_t timer_conf = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = LEDC_TIMER_8_BIT,
        .timer_num = LEDC_TIMER_1,
        .freq_hz = 25000,
    };
    ledc_timer_config(&timer_conf);
    
    ledc_channel_config_t ledc_conf = {
        .gpio_num = PIN_IR_LED_PWM,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = LEDC_CHANNEL_1,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = LEDC_TIMER_1,
        .duty = 0,
        .hpoint = 0,
    };
    ledc_channel_config(&ledc_conf);
    
    /* Initialize IR cut filter GPIO */
    gpio_set_direction(PIN_IR_CUT_EN, GPIO_MODE_OUTPUT);
    ir_cut_filter_enable(false);
    
    /* Initialize camera */
    camera_init();
    
    /* Initialize I2S for microphones */
    i2s_audio_init();
    
    /* Initialize Sub-GHz radio */
    radio_config_t radio_cfg = {
        .address = ADDR_NURSERY_MONITOR,
        .frequency = 868000000,
        .spreading_factor = 7,
        .bandwidth = 4,
        .coding_rate = 1,
        .tx_power = 14,
        .preamble_len = 8,
        .sync_word = 0x0C4B,
    };
    radio_init(&radio_cfg);
    
    /* Initialize TFLite Micro cry classification model */
    model = tflite::GetModel(g_cry_model_data);
    static tflite::MicroMutableOpResolver<10> resolver;
    resolver.AddConv2D();
    resolver.AddDepthwiseConv2D();
    resolver.AddAveragePool2D();
    resolver.AddReshape();
    resolver.AddSoftmax();
    resolver.AddFullyConnected();
    resolver.AddQuantize();
    resolver.AddDequantize();
    
    static uint8_t tensor_arena[50 * 1024];  /* 50KB arena */
    static tflite::MicroInterpreter static_interpreter(
        model, resolver, tensor_arena, sizeof(tensor_arena));
    interpreter = &static_interpreter;
    
    interpreter->AllocateTensors();
    input_tensor = interpreter->input(0);
    output_tensor = interpreter->output(0);
    
    ESP_LOGI("NurseryMonitor", "Model input: %dx%dx%d", 
             input_tensor->dims->data[1],
             input_tensor->dims->data[2],
             input_tensor->dims->data[3]);
    ESP_LOGI("NurseryMonitor", "Model output: %d classes", 
             output_tensor->dims->data[1]);
    
    /* Launch tasks */
    xTaskCreatePinnedToCore(audio_task, "audio", 8192, NULL, 5, NULL, 1);
    xTaskCreatePinnedToCore(env_task, "env", 4096, NULL, 3, NULL, 0);
    xTaskCreatePinnedToCore(transmit_task, "tx", 4096, NULL, 2, NULL, 0);
}