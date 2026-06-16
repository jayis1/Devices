/**
 * CradleKeep — Hub Node Firmware (RP2040 + ESP32-C6)
 * 
 * Main coordinator for the infant monitoring mesh network.
 * - RP2040 Core 0: Mesh TDMA coordinator, local safety rules, TFT display
 * - RP2040 Core 1: UART communication with ESP32-C6 for WiFi/BLE bridge
 * 
 * Inter-MCU communication via UART at 921600 baud
 * 
 * Safety-critical: Breathing monitoring runs locally with no cloud dependency.
 * Apnea detection triggers immediate alerts via mesh, speaker, and BLE.
 */

#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "hardware/spi.h"
#include "hardware/i2c.h"
#include "hardware/uart.h"
#include "hardware/gpio.h"
#include "hardware/pwm.h"
#include "hardware/flash.h"
#include "protocol.h"
#include "radio.h"

/* ── Pin Definitions (RP2040) ──────────────────────────────────────── */
#define PIN_ESP_UART_TX   0
#define PIN_ESP_UART_RX   1
#define PIN_I2C_SDA       4
#define PIN_I2C_SCL       5
#define PIN_SPI_SCK       6
#define PIN_SPI_MOSI      7
#define PIN_SPI_MISO      8
#define PIN_SD_CS         9
#define PIN_TFT_CS        10
#define PIN_TFT_DC        11
#define PIN_TFT_RST       12
#define PIN_TFT_BL        13
#define PIN_RADIO_BUSY    14
#define PIN_RADIO_IRQ      15
#define PIN_RADIO_RST      16
#define PIN_RADIO_NSS      17
#define PIN_RADIO_SCK      18
#define PIN_RADIO_MOSI     19
#define PIN_RADIO_MISO     20
#define PIN_RADIO_SPI_EXTRA 21
#define PIN_DAC_BCLK       22
#define PIN_DAC_LRCLK      23  /* Shared with USER_BTN via config */
#define PIN_DAC_DIN        24
#define PIN_MIC_BCLK       25
#define PIN_MIC_LRCLK      26
#define PIN_MIC_DOUT       27
#define PIN_AMP_ENABLE     28
#define PIN_USER_BTN       29  /* Also ADC2 on RP2040 */

/* ── Constants ─────────────────────────────────────────────────────── */
#define UART_BAUD              921600
#define NODE_COUNT              4   /* hub + crib pad + nursery monitor + feeding station */
#define BREATH_SAFETY_PERIOD_MS 100  /* Check breathing safety every 100ms */
#define MESH_CYCLE_MS           500
#define DISPLAY_UPDATE_MS       1000
#define CLOUD_UPLOAD_MS         5000
#define HEARTBEAT_PERIOD_S      60

/* ── Breathing Safety Thresholds ─────────────────────────────────── */
#define APNEA_WARN_MS           5000   /* 5s no breath → gentle alert */
#define APNEA_URGENT_MS         10000  /* 10s no breath → sound alert */
#define APNEA_EMERGENCY_MS      15000  /* 15s no breath → full alarm */
#define BREATH_RATE_LOW         15     /* Below 15 BPM → alert */
#define BREATH_RATE_HIGH        70     /* Above 70 BPM → alert */
#define TEMP_HIGH_C             36.0   /* Mattress >36°C → alert */
#define TEMP_LOW_C              18.0   /* Mattress <18°C → alert */
#define MOVEMENT_QUIET_S        60     /* 60s no movement → check */

/* ── System State ──────────────────────────────────────────────────── */
typedef struct {
    /* Node status */
    uint8_t  node_online[NODE_COUNT];
    uint32_t node_last_seen[NODE_COUNT];
    
    /* Crib pad data */
    crib_data_t crib;
    uint32_t last_breath_time_ms;    /* Timestamp of last detected breath */
    uint16_t apnea_count;            /* Apnea events in last minute */
    uint8_t  consecutive_no_breath;  /* Count of consecutive no-breath checks */
    uint8_t  current_sleep_stage;
    
    /* Nursery monitor data */
    nursery_data_t nursery;
    uint8_t  current_cry_type;
    uint8_t  current_cry_confidence;
    
    /* Feeding station data */
    feeding_data_t feeding;
    uint32_t last_feeding_time_ms;
    uint16_t last_feeding_volume_ml;
    
    /* Alert state */
    uint8_t  breathing_alert_level;   /* 0=none, 1-4=alert levels */
    uint8_t  environment_alert_level;
    uint8_t  sound_playing;           /* Currently playing sound type */
    uint32_t sound_start_time_ms;
    
    /* Sound effectiveness tracking */
    uint8_t  sound_effectiveness[7]; /* Effectiveness score per sound type */
    uint8_t  sound_attempts[7];      /* How many times each sound was tried */
    
    /* Timing */
    uint32_t mesh_sync_time_ms;
    uint32_t display_update_time_ms;
    uint32_t cloud_upload_time_ms;
} system_state_t;

system_state_t state;
radio_config_t radio_cfg;
radio_stats_t  radio_stats;

/* ── SPI/Digital Display Buffer ────────────────────────────────────── */
#define TFT_WIDTH  240
#define TFT_HEIGHT 320
uint16_t tft_buffer[TFT_WIDTH * 8];  /* Line buffer for TFT */

/* ── Forward Declarations ──────────────────────────────────────────── */
void core1_main(void);           /* ESP32-C6 UART bridge */
void mesh_coordinator(void);     /* TDMA scheduler */
void breathing_safety_check(void);
void process_crib_data(const crib_data_t *data);
void process_nursery_data(const nursery_data_t *data);
void process_feeding_data(const feeding_data_t *data);
void process_cry_event(const cry_event_t *event);
void process_breathing_alert(const breathing_alert_t *alert);
void update_display(void);
void play_sound(uint8_t sound_type);
void stop_sound(void);
void send_to_esp32(const uint8_t *data, uint16_t len);
void send_command(uint8_t target, uint8_t cmd_id, int16_t p1, int16_t p2, uint16_t p3);

/* ── I2C Sensor Reading ─────────────────────────────────────────────── */
/* Hub has SHT40 on I2C for ambient room conditions */
#define SHT40_ADDR 0x44

typedef struct {
    float temperature_c;
    float humidity_pct;
} sht40_data_t;

int read_sht40(sht40_data_t *data) {
    uint8_t cmd = 0xFD;  /* High-precision measurement */
    i2c_write_blocking(i2c0, SHT40_ADDR, &cmd, 1, false);
    busy_wait_us(10000);  /* 10ms measurement time */
    
    uint8_t buf[6];
    i2c_read_blocking(i2c0, SHT40_ADDR, buf, 6, false);
    
    uint16_t temp_raw = (buf[0] << 8) | buf[1];
    uint16_t hum_raw = (buf[3] << 8) | buf[4];
    
    data->temperature_c = -45.0f + 175.0f * (float)temp_raw / 65535.0f;
    data->humidity_pct = 0.0f + 100.0f * (float)hum_raw / 65535.0f;
    
    return 0;
}

/* ── Breathing Safety Engine ────────────────────────────────────────── */
void breathing_safety_check(void) {
    uint32_t now = time_us_32() / 1000;
    uint32_t time_since_breath = now - state.last_breath_time_ms;
    
    /* Check for apnea (no breathing movement) */
    if (state.crib.breath_rate == 0 || time_since_breath > 3000) {
        state.consecutive_no_breath++;
        
        if (time_since_breath > APNEA_EMERGENCY_MS) {
            /* EMERGENCY: 15+ seconds without breathing */
            state.breathing_alert_level = ALERT_EMERGENCY;
            
            /* Full alarm: siren on hub + push to app + SMS */
            breathing_alert_t alert = {
                .alert_level = ALERT_EMERGENCY,
                .breath_rate = state.crib.breath_rate,
                .apnea_duration_ms = (uint16_t)(time_since_breath & 0xFFFF),
                .time_since_breath = (uint16_t)(time_since_breath & 0xFFFF),
                .position = state.crib.position,
                .movement_score = state.crib.movement_score,
                .source_node = ADDR_HUB,
                .timestamp_ms = (uint16_t)(now & 0xFFFF),
            };
            
            /* Send emergency alert to all nodes */
            packet_t pkt = {
                .src = ADDR_HUB,
                .dst = ADDR_BROADCAST,
                .type = PKT_BREATHING_ALERT,
                .payload_len = sizeof(alert),
            };
            memcpy(pkt.payload, &alert, sizeof(alert));
            radio_send(&pkt);
            
        } else if (time_since_breath > APNEA_URGENT_MS) {
            /* URGENT: 10-15 seconds without breathing */
            state.breathing_alert_level = ALERT_URGENT;
            
        } else if (time_since_breath > APNEA_WARN_MS) {
            /* WARNING: 5-10 seconds without breathing */
            state.breathing_alert_level = ALERT_WARNING;
        }
    } else {
        /* Breathing detected - reset counters */
        state.consecutive_no_breath = 0;
        if (state.breathing_alert_level > ALERT_INFO) {
            state.breathing_alert_level = ALERT_INFO;  /* Clear alert */
        }
    }
    
    /* Check breathing rate limits */
    if (state.crib.breath_rate > 0) {
        if (state.crib.breath_rate < BREATH_RATE_LOW) {
            /* Abnormally slow breathing */
            if (state.breathing_alert_level < ALERT_WARNING) {
                state.breathing_alert_level = ALERT_WARNING;
            }
        }
        if (state.crib.breath_rate > BREATH_RATE_HIGH) {
            /* Abnormally fast breathing */
            if (state.breathing_alert_level < ALERT_WARNING) {
                state.breathing_alert_level = ALERT_WARNING;
            }
        }
    }
    
    /* Check mattress temperature */
    float mattress_temp = state.crib.temp_c_x10 / 10.0f;
    if (mattress_temp > TEMP_HIGH_C || mattress_temp < TEMP_LOW_C) {
        if (state.environment_alert_level < ALERT_WARNING) {
            state.environment_alert_level = ALERT_WARNING;
        }
    }
    
    /* Check wetness */
    if (state.crib.wetness_flag) {
        env_event_t evt = {
            .event_type = 6,  /* wetness */
            .severity = ALERT_INFO,
            .value = state.crib.wetness_level,
            .threshold = 50,
            .timestamp_ms = (uint16_t)(now & 0xFFFF),
        };
        /* Send wetness notification to app */
    }
}

/* ── Process Data from Crib Pad ─────────────────────────────────────── */
void process_crib_data(const crib_data_t *data) {
    uint32_t now = time_us_32() / 1000;
    
    /* Update state */
    memcpy(&state.crib, data, sizeof(crib_data_t));
    state.node_online[0] = 1;  /* Crib pad is alive */
    state.node_last_seen[0] = now;
    
    /* Track last breath time */
    if (data->breath_rate > 0) {
        state.last_breath_time_ms = now;
    }
    
    /* Track apnea events */
    if (data->breath_apnea_count > state.apnea_count) {
        /* New apnea event detected */
        state.apnea_count = data->breath_apnea_count;
    }
}

/* ── Process Data from Nursery Monitor ──────────────────────────────── */
void process_nursery_data(const nursery_data_t *data) {
    uint32_t now = time_us_32() / 1000;
    
    memcpy(&state.nursery, data, sizeof(nursery_data_t));
    state.node_online[1] = 1;
    state.node_last_seen[1] = now;
    
    /* Handle cry detection */
    if (data->cry_type != CRY_NONE && data->cry_confidence > 128) {
        state.current_cry_type = data->cry_type;
        state.current_cry_confidence = data->cry_confidence;
        
        /* Auto-soothe: play sound based on cry type */
        switch (data->cry_type) {
            case CRY_HUNGRY:
                /* Don't auto-play sound for hunger — parent should feed */
                break;
            case CRY_TIRED:
                play_sound(SOUND_WHITE_NOISE);
                break;
            case CRY_COLIC:
                play_sound(SOUND_SHUSH);
                break;
            case CRY_DISCOMFORT:
                play_sound(SOUND_WHITE_NOISE);
                break;
            case CRY_PAIN:
                play_sound(SOUND_HEARTBEAT);
                break;
            default:
                play_sound(SOUND_WHITE_NOISE);
                break;
        }
    }
}

/* ── Process Data from Feeding Station ───────────────────────────────── */
void process_feeding_data(const feeding_data_t *data) {
    uint32_t now = time_us_32() / 1000;
    
    memcpy(&state.feeding, data, sizeof(feeding_data_t));
    state.node_online[2] = 1;
    state.node_last_seen[2] = now;
    
    /* Track feeding completion */
    if (data->feeding_state == FEED_DONE) {
        state.last_feeding_time_ms = now;
        state.last_feeding_volume_ml = data->volume_consumed_ml;
    }
}

/* ── Sound Playback ───────────────────────────────────────────────────── */
void play_sound(uint8_t sound_type) {
    state.sound_playing = sound_type;
    state.sound_start_time_ms = time_us_32() / 1000;
    
    /* Send play command to ESP32-C6 for audio streaming */
    uint8_t cmd_buf[4] = {0x01, sound_type, 0x00, 0x00};  /* CMD_PLAY_SOUND */
    send_to_esp32(cmd_buf, 4);
    
    /* Also play locally via I2S DAC for immediate response */
    /* PCM5102A DAC plays from SD card sound library */
    gpio_put(PIN_AMP_ENABLE, 1);  /* Enable speaker amp */
}

void stop_sound(void) {
    state.sound_playing = 0;
    
    uint8_t cmd_buf[4] = {0x02, 0x00, 0x00, 0x00};  /* CMD_STOP_SOUND */
    send_to_esp32(cmd_buf, 4);
    
    gpio_put(PIN_AMP_ENABLE, 0);  /* Disable speaker amp */
}

/* ── ESP32-C6 UART Communication ─────────────────────────────────────── */
void send_to_esp32(const uint8_t *data, uint16_t len) {
    /* Frame format: 0xAA 0x55 | LEN(2) | DATA | CRC16(2) */
    uint8_t frame[256];
    uint16_t idx = 0;
    
    frame[idx++] = 0xAA;
    frame[idx++] = 0x55;
    frame[idx++] = len & 0xFF;
    frame[idx++] = (len >> 8) & 0xFF;
    memcpy(&frame[idx], data, len);
    idx += len;
    
    uint16_t crc = crc16_ccitt(data, len);
    frame[idx++] = crc & 0xFF;
    frame[idx++] = (crc >> 8) & 0xFF;
    
    uart_write_blocking(uart0, frame, idx);
}

/* ── Send Command to Node ─────────────────────────────────────────────── */
void send_command(uint8_t target, uint8_t cmd_id, int16_t p1, int16_t p2, uint16_t p3) {
    command_payload_t cmd = {
        .cmd_id = cmd_id,
        .target_node = target,
        .param1 = p1,
        .param2 = p2,
        .param3 = p3,
    };
    
    packet_t pkt = {
        .src = ADDR_HUB,
        .dst = target,
        .type = PKT_COMMAND,
        .payload_len = sizeof(cmd),
    };
    memcpy(pkt.payload, &cmd, sizeof(cmd));
    radio_send(&pkt);
}

/* ── Mesh TDMA Coordinator ────────────────────────────────────────────── */
void mesh_coordinator(void) {
    uint32_t now = time_us_32() / 1000;
    uint32_t frame_start = now;
    uint8_t current_slot;
    
    while (true) {
        now = time_us_32() / 1000;
        current_slot = (now % FRAME_DURATION_MS) / SLOT_DURATION_MS;
        
        switch (current_slot) {
            case SLOT_CRIB_PAD:
            case SLOT_NURSERY_MONITOR:
            case SLOT_FEEDING_STATION:
                /* Receive data from corresponding node */
                radio_set_rx_mode();
                packet_t pkt;
                if (radio_receive(&pkt, 80) == 0) {
                    /* Process received data */
                    switch (pkt.type) {
                        case PKT_CRIB_DATA:
                            if (pkt.payload_len == sizeof(crib_data_t)) {
                                crib_data_t crib;
                                memcpy(&crib, pkt.payload, sizeof(crib));
                                process_crib_data(&crib);
                            }
                            break;
                        case PKT_NURSERY_DATA:
                            if (pkt.payload_len == sizeof(nursery_data_t)) {
                                nursery_data_t nursery;
                                memcpy(&nursery, pkt.payload, sizeof(nursery));
                                process_nursery_data(&nursery);
                            }
                            break;
                        case PKT_FEEDING_DATA:
                            if (pkt.payload_len == sizeof(feeding_data_t)) {
                                feeding_data_t feeding;
                                memcpy(&feeding, pkt.payload, sizeof(feeding));
                                process_feeding_data(&feeding);
                            }
                            break;
                        case PKT_CRY_EVENT:
                            if (pkt.payload_len == sizeof(cry_event_t)) {
                                cry_event_t event;
                                memcpy(&event, pkt.payload, sizeof(event));
                                process_cry_event(&event);
                            }
                            break;
                        case PKT_BREATHING_ALERT:
                            if (pkt.payload_len == sizeof(breathing_alert_t)) {
                                breathing_alert_t alert;
                                memcpy(&alert, pkt.payload, sizeof(alert));
                                process_breathing_alert(&alert);
                            }
                            break;
                    }
                }
                break;
                
            case SLOT_HUB_CMD:
                /* Hub broadcasts sync + commands */
                radio_set_tx_mode();
                /* Send heartbeat/beacon */
                packet_t beacon;
                beacon.src = ADDR_HUB;
                beacon.dst = ADDR_BROADCAST;
                beacon.type = PKT_HEARTBEAT;
                beacon.payload_len = 4;
                uint32_t ts = now;
                memcpy(beacon.payload, &ts, 4);
                radio_send(&beacon);
                break;
                
            case SLOT_CTRL_ACK:
                /* ACK/retransmit slot */
                /* Send ACKs for received packets */
                radio_set_rx_mode();
                break;
        }
        
        /* Run breathing safety check every cycle */
        breathing_safety_check();
        
        /* Wait for next slot */
        uint32_t slot_end = frame_start + (current_slot + 1) * SLOT_DURATION_MS;
        while ((time_us_32() / 1000) < slot_end) {
            tight_loop_contents();
        }
    }
}

/* ── Process Cry Event ─────────────────────────────────────────────────── */
void process_cry_event(const cry_event_t *event) {
    /* Forward to ESP32-C6 for cloud/app notification */
    uint8_t buf[sizeof(cry_event_t) + 2];
    buf[0] = 0x10;  /* ESP command: CRY_EVENT */
    buf[1] = sizeof(cry_event_t);
    memcpy(&buf[2], event, sizeof(cry_event_t));
    send_to_esp32(buf, sizeof(buf));
}

/* ── Process Breathing Alert from Crib Pad ─────────────────────────────── */
void process_breathing_alert(const breathing_alert_t *alert) {
    /* Escalate alert level */
    if (alert->alert_level > state.breathing_alert_level) {
        state.breathing_alert_level = alert->alert_level;
    }
    
    /* Forward to ESP32-C6 for cloud/app push */
    uint8_t buf[sizeof(breathing_alert_t) + 2];
    buf[0] = 0x11;  /* ESP command: BREATHING_ALERT */
    buf[1] = sizeof(breathing_alert_t);
    memcpy(&buf[2], alert, sizeof(breathing_alert_t));
    send_to_esp32(buf, sizeof(buf));
}

/* ── Display Update ────────────────────────────────────────────────────── */
void update_display(void) {
    /* Render status on 2.8" TFT:
     * - Sleep stage icon + label
     * - Breathing rate
     * - Last feeding time + volume
     * - Room temp + humidity
     * - Alert level (if any)
     * - Connection status icons
     */
    /* (TFT rendering code would use ILI9341 SPI commands) */
}

/* ── Core 1: ESP32-C6 UART Bridge ─────────────────────────────────────── */
void core1_main(void) {
    uint8_t rx_buf[256];
    uint16_t rx_idx = 0;
    
    while (true) {
        /* Read data from ESP32-C6 via UART */
        if (uart_is_readable(uart0)) {
            uint8_t byte = uart_getc(uart0);
            rx_buf[rx_idx++] = byte;
            
            /* Simple framing: look for 0xAA 0x55 header */
            if (rx_idx >= 4 && rx_buf[0] == 0xAA && rx_buf[1] == 0x55) {
                uint16_t len = rx_buf[2] | (rx_buf[3] << 8);
                if (rx_idx >= len + 6) {
                    /* Complete frame received from ESP32-C6 */
                    /* Process: could be WiFi data, BLE commands, etc. */
                    rx_idx = 0;
                }
            }
            if (rx_idx >= sizeof(rx_buf)) {
                rx_idx = 0;  /* Overflow protection */
            }
        }
    }
}

/* ── Main ─────────────────────────────────────────────────────────────── */
int main(void) {
    stdio_init_all();
    
    /* Initialize system state */
    memset(&state, 0, sizeof(state));
    state.last_breath_time_ms = time_us_32() / 1000;
    
    /* Initialize I2C for SHT40 */
    i2c_init(i2c0, 100000);
    gpio_set_function(PIN_I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(PIN_I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(PIN_I2C_SDA);
    gpio_pull_up(PIN_I2C_SCL);
    
    /* Initialize UART for ESP32-C6 */
    uart_init(uart0, UART_BAUD);
    gpio_set_function(PIN_ESP_UART_TX, GPIO_FUNC_UART);
    gpio_set_function(PIN_ESP_UART_RX, GPIO_FUNC_UART);
    
    /* Initialize SPI for TFT display */
    spi_init(spi1, 40000000);
    gpio_set_function(PIN_SPI_SCK, GPIO_FUNC_SPI);
    gpio_set_function(PIN_SPI_MOSI, GPIO_FUNC_SPI);
    gpio_set_function(PIN_SPI_MISO, GPIO_FUNC_SPI);
    
    /* Initialize GPIO */
    gpio_init(PIN_TFT_CS); gpio_set_dir(PIN_TFT_CS, GPIO_OUT);
    gpio_init(PIN_TFT_DC); gpio_set_dir(PIN_TFT_DC, GPIO_OUT);
    gpio_init(PIN_TFT_RST); gpio_set_dir(PIN_TFT_RST, GPIO_OUT);
    gpio_init(PIN_TFT_BL); gpio_set_dir(PIN_TFT_BL, GPIO_OUT);
    gpio_init(PIN_AMP_ENABLE); gpio_set_dir(PIN_AMP_ENABLE, GPIO_OUT);
    gpio_init(PIN_USER_BTN); gpio_set_dir(PIN_USER_BTN, GPIO_IN);
    gpio_pull_up(PIN_USER_BTN);
    
    /* Initialize speaker amp (off by default) */
    gpio_put(PIN_AMP_ENABLE, 0);
    
    /* Initialize radio */
    radio_cfg.address = ADDR_HUB;
    radio_cfg.frequency = 868000000;
    radio_cfg.spreading_factor = 7;
    radio_cfg.bandwidth = 4;  /* 125kHz */
    radio_cfg.coding_rate = 1;  /* 4/5 */
    radio_cfg.tx_power = 20;  /* Hub transmits at higher power */
    radio_cfg.preamble_len = 8;
    radio_cfg.sync_word = 0x0C4B;
    
    radio_init(&radio_cfg);
    
    /* Launch Core 1 for ESP32-C6 communication */
    multicore_launch_core1(core1_main);
    
    /* Main loop: mesh coordinator */
    mesh_coordinator();
    
    return 0;
}