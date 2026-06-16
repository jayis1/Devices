/**
 * FreshKeep — Hub Node Firmware (RP2040 + ESP32-C6)
 * 
 * Main coordinator for the kitchen intelligence mesh network.
 * - RP2040: Mesh TDMA coordinator, local fire-safety rules, TFT display
 * - ESP32-C6: WiFi/BLE bridge to cloud dashboard and mobile app
 * 
 * Inter-MCU communication via UART at 921600 baud
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
#define PIN_RADIO_SPI      21  /* Extra SPI pin */
#define PIN_PIEZO          22
#define PIN_USER_BTN       23
#define PIN_LED_R          24
#define PIN_LED_G          25
#define PIN_LED_B          26
#define PIN_MIC_ADC        27

/* ── Constants ─────────────────────────────────────────────────────── */
#define UART_BAUD          921600
#define NODE_COUNT         4      /* hub + fridge + pantry + stove */
#define FIRE_SAFETY_PERIOD_MS  100  /* Check fire safety every 100ms */
#define MESH_CYCLE_MS      500    /* TDMA frame = 500ms */
#define DISPLAY_UPDATE_MS  1000
#define CLOUD_UPLOAD_MS    5000
#define HEARTBEAT_PERIOD_S 60

/* ── System State ──────────────────────────────────────────────────── */
typedef struct {
    /* Node status */
    uint8_t  node_online[4];        /* Bitmask of which nodes are alive */
    uint32_t node_last_seen[4];    /* Timestamp of last packet from each node */
    
    /* Fridge data */
    fridge_data_t fridge;
    uint8_t  fridge_items[16];      /* Item count per category */
    uint8_t  fridge_expiry_alert;   /* Alert level */
    
    /* Pantry data */
    pantry_data_t pantry;
    uint8_t  pantry_items[16];
    uint8_t  pantry_low_stock;      /* Bitmask of shelves with low stock */
    
    /* Stove guard data */
    stove_data_t stove;
    uint8_t  fire_alert_level;      /* Current fire alert level */
    uint8_t  gas_valve_override;    /* Manual override for gas valve */
    uint32_t unattended_start;     /* When unattended cooking was detected */
    
    /* Fire safety state */
    uint8_t  fire_suppression_active;
    uint8_t  gas_shutoff_active;
    uint32_t fire_alarm_start;
    
    /* Shopping list */
    uint8_t  shopping_list_count;
    uint8_t  shopping_list[32];     /* Category IDs of items to buy */
    
    /* System */
    uint32_t uptime_s;
    uint32_t mesh_frames;
    uint8_t  wifi_connected;
    uint8_t  ble_connected;
} system_state_t;

static system_state_t g_state;

/* ── Radio Handle ──────────────────────────────────────────────────── */
static radio_handle_t g_radio = {
    .config = RADIO_CONFIG_DEFAULT,
    .config.type = RADIO_SX1262,  /* Hub uses SX1262 for longer range */
    .pin_nss = PIN_RADIO_NSS,
    .pin_busy = PIN_RADIO_BUSY,
    .pin_irq = PIN_RADIO_IRQ,
    .pin_reset = PIN_RADIO_RST,
};

/* ── Piezo Alarm ───────────────────────────────────────────────────── */
static void piezo_alarm(uint8_t level) {
    uint slice = pwm_gpio_to_slice_num(PIN_PIEZO);
    uint channel = pwm_gpio_to_channel(PIN_PIEZO);
    
    switch (level) {
        case ALERT_INFO:
            /* No sound */
            pwm_set_chan_level(slice, channel, 0);
            break;
        case ALERT_WARNING:
            /* Single beep every 5 seconds */
            pwm_set_chan_level(slice, channel, 50);
            sleep_ms(100);
            pwm_set_chan_level(slice, channel, 0);
            break;
        case ALERT_URGENT:
            /* Double beep every 3 seconds */
            for (int i = 0; i < 2; i++) {
                pwm_set_chan_level(slice, channel, 100);
                sleep_ms(100);
                pwm_set_chan_level(slice, channel, 0);
                sleep_ms(100);
            }
            break;
        case ALERT_CRITICAL:
            /* Continuous rapid beeping */
            for (int i = 0; i < 10; i++) {
                pwm_set_chan_level(slice, channel, 200);
                sleep_ms(100);
                pwm_set_chan_level(slice, channel, 0);
                sleep_ms(50);
            }
            break;
        case ALERT_EMERGENCY:
            /* Continuous siren */
            for (int i = 0; i < 20; i++) {
                pwm_set_chan_level(slice, channel, 255);
                sleep_ms(50);
                pwm_set_chan_level(slice, channel, 100);
                sleep_ms(50);
            }
            break;
    }
}

/* ── Fire Safety Engine ────────────────────────────────────────────── */
static void fire_safety_check(void) {
    /* ── Critical fire detection (instant action) ──────────────────── */
    if (g_state.stove.fire_confidence > 220 ||    /* ML model says fire */
        g_state.stove.max_temp_c > 300 ||           /* Thermal: >300°C */
        g_state.stove.flame_detected ||              /* IR flame sensor */
        g_state.stove.lpg_ppm > 1000) {              /* Gas leak >1000 ppm */
        
        if (g_state.fire_alert_level < ALERT_EMERGENCY) {
            g_state.fire_alert_level = ALERT_EMERGENCY;
            g_state.fire_alarm_start = time_us_32() / 1000;
            
            /* EMERGENCY ACTION: Shut off gas valve immediately */
            g_state.gas_shutoff_active = 1;
            
            /* Send gas shutoff command to stove guard */
            packet_t pkt;
            pkt_init(&pkt, ADDR_HUB, ADDR_STOVE_GUARD, PKT_COMMAND);
            command_t cmd = {
                .cmd_id = CMD_GAS_SHUTOFF,
                .param_len = 1,
                .params = {0x01}  /* Close valve */
            };
            pkt_add_payload(&pkt, (uint8_t*)&cmd, sizeof(cmd));
            pkt_finalize(&pkt);
            radio_send(&g_radio, pkt.data, pkt.len);
            
            /* Send FIRE_ALARM broadcast */
            pkt_init(&pkt, ADDR_HUB, ADDR_BROADCAST, PKT_FIRE_ALARM);
            fire_alarm_t alarm = {
                .max_temp_c = g_state.stove.max_temp_c,
                .lpg_ppm = g_state.stove.lpg_ppm,
                .smoke_level = g_state.stove.smoke_level,
                .flame_detected = g_state.stove.flame_detected,
                .fire_confidence = g_state.stove.fire_confidence,
                .source_node = ADDR_STOVE_GUARD,
                .timestamp_ms = (uint16_t)(time_us_32() / 1000),
            };
            pkt_add_payload(&pkt, (uint8_t*)&alarm, sizeof(alarm));
            pkt_finalize(&pkt);
            radio_send(&g_radio, pkt.data, pkt.len);
            
            /* Sound alarm */
            piezo_alarm(ALERT_EMERGENCY);
            
            /* Notify cloud via ESP32-C6 */
            /* (handled in core 1 UART bridge) */
        }
    }
    /* ── Unattended cooking detection ───────────────────────────────── */
    else if (g_state.stove.burner_state > 0 && !g_state.stove.motion_detected) {
        /* Burner on, no person detected */
        if (g_state.unattended_start == 0) {
            g_state.unattended_start = time_us_32() / 1000;
        } else {
            uint32_t elapsed = (time_us_32() / 1000) - g_state.unattended_start;
            if (elapsed > 600000 && g_state.fire_alert_level < ALERT_URGENT) {
                /* 10 minutes unattended → WARNING */
                g_state.fire_alert_level = ALERT_URGENT;
                piezo_alarm(ALERT_URGENT);
            }
            if (elapsed > 1200000 && g_state.fire_alert_level < ALERT_CRITICAL) {
                /* 20 minutes unattended → CRITICAL, auto shutoff */
                g_state.fire_alert_level = ALERT_CRITICAL;
                g_state.gas_shutoff_active = 1;
                
                packet_t pkt;
                pkt_init(&pkt, ADDR_HUB, ADDR_STOVE_GUARD, PKT_COMMAND);
                command_t cmd = {
                    .cmd_id = CMD_GAS_SHUTOFF,
                    .param_len = 1,
                    .params = {0x01}
                };
                pkt_add_payload(&pkt, (uint8_t*)&cmd, sizeof(cmd));
                pkt_finalize(&pkt);
                radio_send(&g_radio, pkt.data, pkt.len);
                
                piezo_alarm(ALERT_CRITICAL);
            }
        }
    } else {
        g_state.unattended_start = 0;
        if (g_state.fire_alert_level > ALERT_WARNING) {
            g_state.fire_alert_level = ALERT_WARNING;
        }
    }
    
    /* ── Gas leak detection ─────────────────────────────────────────── */
    if (g_state.stove.lpg_ppm > 300 && g_state.fire_alert_level < ALERT_CRITICAL) {
        g_state.fire_alert_level = ALERT_CRITICAL;
        g_state.gas_shutoff_active = 1;
        piezo_alarm(ALERT_CRITICAL);
    }
    if (g_state.stove.co_ppm > 100 && g_state.fire_alert_level < ALERT_URGENT) {
        g_state.fire_alert_level = ALERT_URGENT;
        piezo_alarm(ALERT_URGENT);
    }
}

/* ── Spoilage Alert Engine ─────────────────────────────────────────── */
static void spoilage_check(void) {
    /* Check fridge spoilage score from gas sensors */
    if (g_state.fridge.spoilage_score > 80) {
        g_state.fridge_expiry_alert = ALERT_CRITICAL;
        piezo_alarm(ALERT_URGENT);
    } else if (g_state.fridge.spoilage_score > 50) {
        g_state.fridge_expiry_alert = ALERT_WARNING;
    } else {
        g_state.fridge_expiry_alert = ALERT_INFO;
    }
    
    /* Check pantry low stock */
    for (int i = 0; i < 6; i++) {
        if (g_state.pantry.weight_mg[i] < 100000 && g_state.pantry.weight_mg[i] > 0) {
            /* Shelf has < 100g remaining */
            g_state.pantry_low_stock |= (1 << i);
        }
    }
}

/* ── Shopping List Generator ────────────────────────────────────────── */
static void generate_shopping_list(void) {
    g_state.shopping_list_count = 0;
    
    /* Add items based on low stock */
    if (g_state.pantry_low_stock & 0x01) g_state.shopping_list[g_state.shopping_list_count++] = 0;  /* Category 0 */
    if (g_state.pantry_low_stock & 0x02) g_state.shopping_list[g_state.shopping_list_count++] = 1;
    if (g_state.pantry_low_stock & 0x04) g_state.shopping_list[g_state.shopping_list_count++] = 2;
    if (g_state.pantry_low_stock & 0x08) g_state.shopping_list[g_state.shopping_list_count++] = 3;
    if (g_state.pantry_low_stock & 0x10) g_state.shopping_list[g_state.shopping_list_count++] = 4;
    if (g_state.pantry_low_stock & 0x20) g_state.shopping_list[g_state.shopping_list_count++] = 5;
    
    /* Add items based on expiry */
    if (g_state.fridge.spoilage_score > 50) {
        /* Some fridge items are expiring — suggest using them */
        g_state.shopping_list[g_state.shopping_list_count++] = 10; /* "Use soon" flag */
    }
}

/* ── TDMA Mesh Coordinator ─────────────────────────────────────────── */
static void mesh_coordinator_cycle(void) {
    uint32_t frame_start = time_us_32() / 1000;
    
    /* ── Slot 0: Listen for STOVE GUARD data (priority) ─────────── */
    uint32_t slot0_start = time_us_32() / 1000;
    radio_recv(&g_radio, NULL, 0, SLOT_DURATION_MS);
    /* Process stove guard data if received */
    
    /* ── Slot 1: Listen for FRIDGE data ─────────────────────────── */
    radio_recv(&g_radio, NULL, 0, SLOT_DURATION_MS);
    
    /* ── Slot 2: Listen for PANTRY data ─────────────────────────── */
    radio_recv(&g_radio, NULL, 0, SLOT_DURATION_MS);
    
    /* ── Slot 3: Hub broadcasts commands ────────────────────────── */
    packet_t hub_pkt;
    pkt_init(&hub_pkt, ADDR_HUB, ADDR_BROADCAST, PKT_HEARTBEAT);
    heartbeat_t hb = {
        .node_id = ADDR_HUB,
        .battery_pct = 100, /* Hub always on mains */
        .uptime_min = (uint16_t)(g_state.uptime_s / 60),
    };
    pkt_add_payload(&hub_pkt, (uint8_t*)&hb, sizeof(hb));
    pkt_finalize(&hub_pkt);
    radio_send(&g_radio, hub_pkt.data, hub_pkt.len);
    
    /* ── Slot 4: ACK/retransmit ──────────────────────────────────── */
    radio_recv(&g_radio, NULL, 0, SLOT_DURATION_MS);
    
    g_state.mesh_frames++;
}

/* ── UART Bridge to ESP32-C6 ───────────────────────────────────────── */
static void send_to_esp(const char *msg) {
    uart_puts(uart0, msg);
    uart_putc(uart0, '\n');
}

static void format_mqtt_message(char *buf, size_t len, const char *topic, const uint8_t *data, uint8_t data_len) {
    /* Format: MQTT:<topic>:<hex_data>\n */
    int pos = snprintf(buf, len, "MQTT:%s:", topic);
    for (uint8_t i = 0; i < data_len && pos < (int)len - 2; i++) {
        pos += snprintf(buf + pos, len - pos, "%02X", data[i]);
    }
    buf[pos] = '\n';
    buf[pos + 1] = '\0';
}

/* ── Core 0: Main Loop (Mesh + Safety + Display) ────────────────────── */
int main(void) {
    stdio_init_all();
    
    /* Initialize system state */
    memset(&g_state, 0, sizeof(g_state));
    
    /* Initialize GPIOs */
    gpio_init(PIN_USER_BTN);
    gpio_set_dir(PIN_USER_BTN, GPIO_IN);
    gpio_pull_up(PIN_USER_BTN);
    
    gpio_init(PIN_LED_R);
    gpio_init(PIN_LED_G);
    gpio_init(PIN_LED_B);
    gpio_set_dir(PIN_LED_R, GPIO_OUT);
    gpio_set_dir(PIN_LED_G, GPIO_OUT);
    gpio_set_dir(PIN_LED_B, GPIO_OUT);
    
    /* Initialize UART to ESP32-C6 */
    uart_init(uart0, UART_BAUD);
    gpio_set_function(PIN_ESP_UART_TX, GPIO_FUNC_UART);
    gpio_set_function(PIN_ESP_UART_RX, GPIO_FUNC_UART);
    
    /* Initialize I2C for radio */
    i2c_init(i2c0, 400000);
    gpio_set_function(PIN_I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(PIN_I2C_SCL, GPIO_FUNC_I2C);
    
    /* Initialize SPI for display + SD */
    spi_init(spi0, 32000000);
    gpio_set_function(PIN_SPI_SCK, GPIO_FUNC_SPI);
    gpio_set_function(PIN_SPI_MOSI, GPIO_FUNC_SPI);
    gpio_set_function(PIN_SPI_MISO, GPIO_FUNC_SPI);
    
    /* Initialize piezo PWM */
    gpio_set_function(PIN_PIEZO, GPIO_FUNC_PWM);
    pwm_set_wrap(pwm_gpio_to_slice_num(PIN_PIEZO), 65535);
    pwm_set_enabled(pwm_gpio_to_slice_num(PIN_PIEZO), true);
    
    /* Initialize radio */
    radio_init(&g_radio, &RADIO_CONFIG_DEFAULT);
    
    /* Initialize display */
    /* (TFT init via ILI9341 driver — omitted for brevity) */
    
    /* Status LED: green = running */
    gpio_put(PIN_LED_G, 1);
    
    /* ── Main Loop ────────────────────────────────────────────────── */
    uint32_t last_safety_check = 0;
    uint32_t last_spoilage_check = 0;
    uint32_t last_cloud_upload = 0;
    uint32_t last_display_update = 0;
    
    while (1) {
        uint32_t now = time_us_32() / 1000;
        
        /* TDMA mesh cycle (every 500ms) */
        mesh_coordinator_cycle();
        
        /* Fire safety check (every 100ms) */
        if (now - last_safety_check > FIRE_SAFETY_PERIOD_MS) {
            fire_safety_check();
            last_safety_check = now;
        }
        
        /* Spoilage check (every 30 seconds) */
        if (now - last_spoilage_check > 30000) {
            spoilage_check();
            last_spoilage_check = now;
        }
        
        /* Generate shopping list (every 5 minutes) */
        if (now - last_spoilage_check > 300000) {
            generate_shopping_list();
        }
        
        /* Cloud upload (every 5 seconds) */
        if (now - last_cloud_upload > CLOUD_UPLOAD_MS) {
            /* Send fridge data to ESP32-C6 for WiFi upload */
            char buf[128];
            format_mqtt_message(buf, sizeof(buf), "freshkeep/fridge/data",
                              (uint8_t*)&g_state.fridge, sizeof(g_state.fridge));
            send_to_esp(buf);
            
            format_mqtt_message(buf, sizeof(buf), "freshkeep/pantry/data",
                              (uint8_t*)&g_state.pantry, sizeof(g_state.pantry));
            send_to_esp(buf);
            
            format_mqtt_message(buf, sizeof(buf), "freshkeep/stove/data",
                              (uint8_t*)&g_state.stove, sizeof(g_state.stove));
            send_to_esp(buf);
            
            /* Fire alert */
            if (g_state.fire_alert_level >= ALERT_CRITICAL) {
                format_mqtt_message(buf, sizeof(buf), "freshkeep/fire/alarm",
                                  (uint8_t*)&g_state.fire_alert_level, 1);
                send_to_esp(buf);
            }
            
            last_cloud_upload = now;
        }
        
        /* Display update (every 1 second) */
        if (now - last_display_update > DISPLAY_UPDATE_MS) {
            /* Update TFT display with current status */
            /* (TFT rendering omitted for brevity — would show:
             *   - Fridge temp + spoilage score
             *   - Pantry items count + low stock alerts
             *   - Stove guard status
             *   - Shopping list items
             *   - Fire alert level (if any)
             *   - Battery levels
             * )
             */
            last_display_update = now;
        }
        
        /* Update uptime */
        g_state.uptime_s = now / 1000;
        
        /* Check node timeouts (mark offline if not seen for 10 seconds) */
        for (int i = 1; i < NODE_COUNT; i++) {
            if (g_state.node_online[i] && 
                (now - g_state.node_last_seen[i]) > 10000) {
                g_state.node_online[i] = 0;
            }
        }
        
        /* User button: silence alarm / toggle display */
        if (!gpio_get(PIN_USER_BTN)) {
            /* Silence alarm for 5 minutes */
            g_state.fire_alert_level = ALERT_INFO;
            sleep_ms(200); /* Debounce */
        }
        
        /* Status LED */
        if (g_state.fire_alert_level >= ALERT_EMERGENCY) {
            gpio_put(PIN_LED_R, 1);
            gpio_put(PIN_LED_G, 0);
            gpio_put(PIN_LED_B, 0);
        } else if (g_state.fire_alert_level >= ALERT_WARNING) {
            gpio_put(PIN_LED_R, 1);
            gpio_put(PIN_LED_G, 1);
            gpio_put(PIN_LED_B, 0);
        } else {
            gpio_put(PIN_LED_R, 0);
            gpio_put(PIN_LED_G, 1);
            gpio_put(PIN_LED_B, 0);
        }
    }
    
    return 0;
}