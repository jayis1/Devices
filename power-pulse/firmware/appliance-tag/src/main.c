/**
 * PowerPulse Appliance Tag — Main (nRF52840, nRF Connect SDK / Zephyr)
 * 
 * BLE mesh plug-level energy monitor and relay controller.
 * Measures per-appliance power using BL0937, provides on/off control
 * via solid-state relay, and broadcasts data to hub over BLE mesh.
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/mesh.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/random/random.h>

#include "powerpulse_protocol.h"

LOG_MODULE_REGISTER(appliance_tag, LOG_LEVEL_INF);

// ─── Device Tree ────────────────────────────────────────────────────

#define BL0937_CF_PIN    DT_PROP(DT_NODELABEL(bl0937), cf_gpios)
#define BL0937_CF1_PIN   DT_PROP(DT_NODELABEL(bl0937), cf1_gpios)
#define BL0937_SEL_PIN   DT_PROP(DT_NODELABEL(bl0937), sel_gpios)
#define RELAY_PIN         DT_PROP(DT_NODELABEL(relay), gpios)
#define BUTTON_PIN        DT_PROP(DT_NODELABEL(button0), gpios)
#define WS2812_PIN        DT_PROP(DT_NODELABEL(led0), gpios)
#define OLED_SDA_PIN      DT_PROP(DT_NODELABEL(oled), sda_gpios)
#define OLED_SCL_PIN      DT_PROP(DT_NODELABEL(oled), scl_gpios)

// ─── Constants ──────────────────────────────────────────────────────

#define FW_VERSION_MAJOR  1
#define FW_VERSION_MINOR  0

#define BLE_MESH_CID      0x05F0  // Custom company ID
#define PP_MODEL_ID_SRV   0x1001  // PowerPulse Appliance Server
#define PP_MODEL_ID_CLI   0x1002  // PowerPulse Appliance Client

// BL0937 calibration constants
#define BL0937_VOLTAGE_CONST   0.22f    // Voltage coefficient (calibrated per unit)
#define BL0937_CURRENT_CONST   0.0013f  // Current coefficient (calibrated per unit)
#define BL0937_POWER_CONST     0.82f    // Power coefficient (calibrated per unit)

#define TAG_ID  0x01  // Default tag ID, configured during provisioning

// ─── Global State ──────────────────────────────────────────────────

typedef struct {
    // BL0937 readings
    float voltage;          // V
    float current;          // A
    float power;             // W
    float power_factor;     // PF (-1 to 1)
    float energy_wh;        // Cumulative Wh
    
    // BL0937 pulse counting
    uint32_t cf_pulses;     // Power pulses from BL0937
    uint32_t cf1_pulses;    // Current/voltage pulses
    int64_t  last_cf_time;  // Timestamp of last CF pulse
    int64_t  last_cf1_time; // Timestamp of last CF1 pulse
    bool     cf1_is_current; // CF1 mode: true=current, false=voltage
    
    // Relay state
    bool relay_on;
    
    // Calibration
    float voltage_coeff;
    float current_coeff;
    float power_coeff;
    
    // BLE mesh
    bool provisioned;
    uint16_t mesh_addr;
    
    // Statistics
    uint32_t uptime_seconds;
    uint8_t  temperature_c;  // Internal MCU temperature
} tag_state_t;

static tag_state_t g_state = {
    .voltage_coeff = BL0937_VOLTAGE_CONST,
    .current_coeff = BL0937_CURRENT_CONST,
    .power_coeff = BL0937_POWER_CONST,
};

// ─── K Threads ──────────────────────────────────────────────────────

static struct k_thread energy_thread;
static struct k_thread ble_thread;
static struct k_thread display_thread;
static struct k_thread relay_thread;

K_THREAD_STACK_DEFINE(energy_stack, 2048);
K_THREAD_STACK_DEFINE(ble_stack, 4096);
K_THREAD_STACK_DEFINE(display_stack, 1024);
K_THREAD_STACK_DEFINE(relay_stack, 512);

// ─── Semaphores & Queues ───────────────────────────────────────────

static struct k_sem energy_sem;
static struct k_mutex state_mutex;

// ─── BL0937 Pulse Counter ISR ──────────────────────────────────────

static struct gpio_callback cf_cb;
static struct gpio_callback cf1_cb;

static void cf_pulse_handler(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
    int64_t now = k_uptime_get();
    g_state.cf_pulses++;
    
    // Calculate instantaneous power from pulse interval
    if (g_state.last_cf_time > 0) {
        float interval_ms = (float)(now - g_state.last_cf_time);
        if (interval_ms > 0) {
            // Power = power_coeff / pulse_interval
            // BL0937 CF output frequency is proportional to active power
            g_state.power = g_state.power_coeff * 1000000.0f / interval_ms;
        }
    }
    g_state.last_cf_time = now;
    
    k_sem_give(&energy_sem);
}

static void cf1_pulse_handler(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
    g_state.cf1_pulses++;
    g_state.last_cf1_time = k_uptime_get();
}

// ─── Energy Calculation Thread ──────────────────────────────────────

static void energy_task(void *p1, void *p2, void *p3)
{
    LOG_INF("Energy task started");
    
    uint32_t last_energy_update = 0;
    uint32_t pulse_window_start = 0;
    uint32_t last_report_time = 0;
    
    while (1) {
        // Wait for energy semaphore or timeout (1 second)
        k_sem_take(&energy_sem, K_SECONDS(1));
        
        k_mutex_lock(&state_mutex, K_FOREVER);
        
        // Calculate voltage and current from CF1 pulses
        // In SEL=HIGH mode, CF1 outputs current signal
        // In SEL=LOW mode, CF1 outputs voltage signal
        
        // Toggle CF1 mode every 5 seconds to alternate between V and I measurement
        uint32_t now = k_uptime_get() / 1000;
        bool sel_state = (now % 10) < 5;  // Alternate every 5 seconds
        gpio_pin_set(gpio_port, BL0937_SEL_PIN, sel_state ? 1 : 0);
        
        if (sel_state) {
            // CF1 is measuring current
            g_state.cf1_is_current = true;
            if (g_state.cf1_pulses > 0 && g_state.last_cf1_time > 0) {
                // Current = current_coeff * pulse_rate
                g_state.current = g_state.current_coeff * g_state.cf1_pulses;
            }
        } else {
            // CF1 is measuring voltage
            g_state.cf1_is_current = false;
            if (g_state.cf1_pulses > 0 && g_state.last_cf1_time > 0) {
                g_state.voltage = g_state.voltage_coeff * g_state.cf1_pulses;
            }
        }
        
        // Calculate power factor
        if (g_state.voltage > 0 && g_state.current > 0 && g_state.power > 0) {
            float apparent = g_state.voltage * g_state.current;
            g_state.power_factor = (apparent > 0.1f) ? g_state.power / apparent : 0.0f;
            if (g_state.power_factor > 1.0f) g_state.power_factor = 1.0f;
        }
        
        // Accumulate energy (Wh)
        g_state.energy_wh += g_state.power / 3600.0f;  // 1W for 1 second = 1/3600 Wh
        
        k_mutex_unlock(&state_mutex);
        
        // Publish BLE mesh data every 5 seconds
        if (now - last_report_time >= 5) {
            last_report_time = now;
            // Publish to BLE mesh (handled in ble_thread)
        }
        
        k_sleep(K_MSEC(100));
    }
}

// ─── BLE Mesh Thread ────────────────────────────────────────────────

static void ble_mesh_task(void *p1, void *p2, void *p3)
{
    LOG_INF("BLE Mesh task started");
    
    // Initialize BLE
    int err = bt_enable(NULL);
    if (err) {
        LOG_ERR("Bluetooth init failed (err %d)", err);
        return;
    }
    
    LOG_INF("Bluetooth initialized");
    
    // Initialize BLE mesh
    err = bt_mesh_init(NULL, NULL);
    if (err) {
        LOG_ERR("Mesh init failed (err %d)", err);
        return;
    }
    
    // In production: bt_mesh_prov_enable(BT_MESH_PROV_ADV | BT_MESH_PROV_GATT)
    // Wait for provisioning from hub via the PowerPulse mobile app
    
    while (1) {
        if (g_state.provisioned) {
            // Build appliance data payload
            k_mutex_lock(&state_mutex, K_FOREVER);
            
            pp_appliance_data_payload_t data = {
                .tag_id = TAG_ID,
                .voltage_mv = (uint16_t)(g_state.voltage * 1000),
                .current_ma = (uint16_t)(g_state.current * 1000),
                .power_w = (uint16_t)(g_state.power),
                .power_factor = (int16_t)(g_state.power_factor * 10000),
                .energy_wh = (uint32_t)(g_state.energy_wh),
                .relay_state = g_state.relay_on ? 1 : 0,
                .temperature_c = (uint8_t)(g_state.temperature_c),
            };
            
            k_mutex_unlock(&state_mutex);
            
            // Publish via BLE mesh vendor model
            // bt_mesh_model_publish(&pp_model_srv, &data, sizeof(data));
            LOG_INF("Published: V=%.1f I=%.2f P=%.0f PF=%.2f E=%.0fWh Relay=%d",
                    g_state.voltage, g_state.current, g_state.power,
                    g_state.power_factor, g_state.energy_wh, g_state.relay_on);
        }
        
        k_sleep(K_SECONDS(5));
    }
}

// ─── Display Thread ─────────────────────────────────────────────────

static void display_task(void *p1, void *p2, void *p3)
{
    LOG_INF("Display task started");
    
    // Initialize SSD1306 OLED
    // ssd1306_init(&oled_dev);
    
    while (1) {
        k_mutex_lock(&state_mutex, K_FOREVER);
        
        float power = g_state.power;
        bool relay = g_state.relay_on;
        
        k_mutex_unlock(&state_mutex);
        
        // Update OLED display
        // ssd1306_clear();
        // ssd1306_draw_text(0, 0, "PowerPulse Tag", FONT_SMALL);
        
        char buf[32];
        snprintf(buf, sizeof(buf), "%.0f W", power);
        // ssd1306_draw_text(0, 16, buf, FONT_LARGE);
        
        snprintf(buf, sizeof(buf), "%s", relay ? "ON" : "OFF");
        // ssd1306_draw_text(0, 40, buf, FONT_MEDIUM);
        
        // ssd1306_update();
        
        k_sleep(K_MSEC(500));
    }
}

// ─── Relay Control Thread ───────────────────────────────────────────

static void relay_task(void *p1, void *p2, void *p3)
{
    LOG_INF("Relay task started");
    
    while (1) {
        // Check for relay commands from BLE mesh
        // In production: receive commands from mesh model callbacks
        
        // For now: relay state is controlled directly
        gpio_pin_set(gpio_port, RELAY_PIN, g_state.relay_on ? 1 : 0);
        
        k_sleep(K_MSEC(100));
    }
}

// ─── Button Handler ─────────────────────────────────────────────────

static struct gpio_callback button_cb;

static void button_handler(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
    static int64_t last_press = 0;
    int64_t now = k_uptime_get();
    
    // Debounce: ignore presses within 200ms
    if (now - last_press < 200) return;
    last_press = now;
    
    // Toggle relay
    k_mutex_lock(&state_mutex, K_FOREVER);
    g_state.relay_on = !g_state.relay_on;
    k_mutex_unlock(&state_mutex);
    
    LOG_INF("Button pressed: relay %s", g_state.relay_on ? "ON" : "OFF");
    
    // Long press (>3 seconds) = enter BLE pairing mode
    // This would be handled with a timer in production
}

// ─── Main ──────────────────────────────────────────────────────────

int main(void)
{
    LOG_INF("=== PowerPulse Appliance Tag v%d.%d ===", FW_VERSION_MAJOR, FW_VERSION_MINOR);
    
    // Initialize mutex and semaphore
    k_mutex_init(&state_mutex);
    k_sem_init(&energy_sem, 0, 1);
    
    // Initialize GPIO
    // Configure BL0937 input pins
    gpio_pin_configure(gpio_port, BL0937_CF_PIN, GPIO_INPUT);
    gpio_pin_configure(gpio_port, BL0937_CF1_PIN, GPIO_INPUT);
    gpio_pin_configure(gpio_port, BL0937_SEL_PIN, GPIO_OUTPUT_INACTIVE);
    
    // Configure relay output
    gpio_pin_configure(gpio_port, RELAY_PIN, GPIO_OUTPUT_INACTIVE);
    
    // Configure button
    gpio_pin_configure(gpio_port, BUTTON_PIN, GPIO_INPUT | GPIO_PULL_UP);
    
    // Configure LED
    gpio_pin_configure(gpio_port, WS2812_PIN, GPIO_OUTPUT_INACTIVE);
    
    // Setup interrupts for BL0937 CF and CF1 pins
    gpio_pin_interrupt_configure(gpio_port, BL0937_CF_PIN, GPIO_INT_EDGE_FALLING);
    gpio_pin_interrupt_configure(gpio_port, BL0937_CF1_PIN, GPIO_INT_EDGE_FALLING);
    gpio_init_callback(&cf_cb, cf_pulse_handler, BIT(BL0937_CF_PIN));
    gpio_init_callback(&cf1_cb, cf1_pulse_handler, BIT(BL0937_CF1_PIN));
    gpio_add_callback(gpio_port, &cf_cb);
    gpio_add_callback(gpio_port, &cf1_cb);
    
    // Setup button interrupt
    gpio_pin_interrupt_configure(gpio_port, BUTTON_PIN, GPIO_INT_EDGE_FALLING);
    gpio_init_callback(&button_cb, button_handler, BIT(BUTTON_PIN));
    gpio_add_callback(gpio_port, &button_cb);
    
    // Initialize I2C for OLED
    // i2c_init(&i2c_dev, I2C_SPEED_FAST);
    
    // Start threads
    k_thread_create(&energy_thread, energy_stack, K_THREAD_STACK_SIZEOF(energy_stack),
                    energy_task, NULL, NULL, NULL, 5, 0, K_NO_WAIT);
    
    k_thread_create(&ble_thread, ble_stack, K_THREAD_STACK_SIZEOF(ble_stack),
                    ble_mesh_task, NULL, NULL, NULL, 3, 0, K_NO_WAIT);
    
    k_thread_create(&display_thread, display_stack, K_THREAD_STACK_SIZEOF(display_stack),
                    display_task, NULL, NULL, NULL, 2, 0, K_NO_WAIT);
    
    k_thread_create(&relay_thread, relay_stack, K_THREAD_STACK_SIZEOF(relay_stack),
                    relay_task, NULL, NULL, NULL, 4, 0, K_NO_WAIT);
    
    // Main loop: monitor internal temperature
    while (1) {
        // Read internal temperature (nRF52840 has built-in temp sensor)
        // g_state.temperature_c = read_internal_temp();
        
        g_state.uptime_seconds++;
        k_sleep(K_SECONDS(1));
    }
    
    return 0;
}