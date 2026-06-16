/**
 * PowerPulse Circuit Monitor — Main Entry Point (STM32G474, FreeRTOS)
 * 
 * Monitors up to 16 circuit breakers using CT sensors and an ADS131E08
 * high-precision ADC. Detects arc faults, overloads, and per-circuit
 * energy consumption. Communicates with hub via Sub-GHz 868 MHz.
 */

#include <stdio.h>
#include <string.h>
#include <math.h>
#include "stm32g4xx_hal.h"
#include "cmsis_os.h"
#include "powerpulse_protocol.h"
#include "cc1101.h"
#include "ads131e08.h"
#include "eeprom.h"

// ─── Constants ──────────────────────────────────────────────────────

#define FW_VERSION_MAJOR    1
#define FW_VERSION_MINOR    0

#define NUM_CIRCUITS        16
#define ADC_SAMPLE_RATE     8000    // 8 kHz per channel
#define ADC_CHANNELS        8       // ADS131E08 has 8 channels
#define ADC_OVERSAMPLE      2       // 2 ADCs multiplexed for 16 circuits
#define VOLTAGE_SCALE      0.123f  // Calibration: ADC counts to volts
#define CT_RATIO           2000.0f  // SCT-013-030: 30A / 15mA = 2000 turns
#define BURDEN_RESISTOR    33.0f    // Burden resistor in ohms
#define ARC_DETECT_WINDOW  256      // Samples per arc detection window
#define ARC_THRESHOLD      3.5f    // Energy ratio threshold for arc detection
#define ARC_MIN_BURSTS     3       // Minimum consecutive bursts for confirmation

#define NODE_ADDRESS       PP_ADDR_CIRCUIT_MONITOR(0)  // Panel 0

// ─── Global State ──────────────────────────────────────────────────

typedef struct {
    // Per-circuit data
    float rms_current[NUM_CIRCUITS];       // RMS current in amps
    float rms_voltage;                      // RMS mains voltage in volts
    float power_factor[NUM_CIRCUITS];      // Per-circuit power factor
    uint16_t real_power[NUM_CIRCUITS];     // Real power in watts
    uint32_t energy_wh[NUM_CIRCUITS];      // Cumulative energy in Wh
    
    // Arc detection state
    uint8_t  arc_fault_circuit;            // Circuit with active arc fault (255 = none)
    uint8_t  arc_confidence;               // Arc detection confidence (0-100%)
    uint16_t arc_duration_ms;              // Duration of arc burst in ms
    uint8_t  arc_burst_count;             // Consecutive burst count
    float    arc_energy_ratio;             // Current energy ratio
    
    // Calibration
    float ct_offset[NUM_CIRCUITS];         // Zero-offset calibration per CT
    float ct_gain[NUM_CIRCUITS];           // Gain calibration per CT
    float voltage_offset;                   // Voltage offset
    float voltage_gain;                     // Voltage gain
    
    // Communication
    uint16_t tx_seq;                       // TX sequence counter
    bool     calibrated;
    uint32_t last_hub_contact_ms;
    
    // Temperature
    float panel_temp_c;                    // Panel temperature
    
    // Statistics
    uint32_t total_samples;
    uint32_t uptime_seconds;
} circuit_monitor_state_t;

static circuit_monitor_state_t g_state = {0};

// ─── ADC Sample Buffers ────────────────────────────────────────────

// Double buffer for ADC samples
#define SAMPLE_BUFFER_SIZE  512  // Per channel

static int16_t adc_buffer_a[NUM_CIRCUITS][SAMPLE_BUFFER_SIZE];
static int16_t adc_buffer_b[NUM_CIRCUITS][SAMPLE_BUFFER_SIZE];
static volatile uint8_t active_buffer = 0;  // 0 = A, 1 = B
static volatile uint16_t sample_index = 0;

// ─── Task Handles ──────────────────────────────────────────────────

static osThreadId_t adc_task_handle;
static osThreadId_t arc_detect_task_handle;
static osThreadId_t power_calc_task_handle;
static osThreadId_t subghz_task_handle;
static osThreadId_t heartbeat_task_handle;
static osThreadId_t temp_task_handle;

// ─── Message Queue ─────────────────────────────────────────────────

#define CIRCUIT_DATA_QUEUE_LEN  4

typedef struct {
    pp_circuit_reading_t readings[NUM_CIRCUITS];
    uint16_t voltage_mv;
    uint16_t frequency_cph;
    uint8_t  num_active;
    uint8_t  circuit_mask;
} circuit_data_msg_t;

static osMessageQueueId_t circuit_data_queue;

// ─── SPI Handle ────────────────────────────────────────────────────

static SPI_HandleTypeDef hspi1;  // CC1101
static SPI_HandleTypeDef hspi2;  // ADS131E08 (via isolators)
static I2C_HandleTypeDef hi2c1; // EEPROM + TMP117

// ─── Function Prototypes ───────────────────────────────────────────

void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_SPI1_Init(void);
static void MX_SPI2_Init(void);
static void MX_I2C1_Init(void);

static void adc_task(void *argument);
static void arc_detect_task(void *argument);
static void power_calc_task(void *argument);
static void subghz_task(void *argument);
static void heartbeat_task(void *argument);
static void temp_task(void *argument);

static void calculate_rms(uint8_t circuit, int16_t *samples, uint16_t num_samples);
static void calculate_power_factor(uint8_t circuit, int16_t *voltage_samples, int16_t *current_samples, uint16_t num_samples);
static void send_circuit_data(void);
static void send_arc_fault_alert(uint8_t circuit, uint8_t confidence, uint8_t arc_type, uint16_t duration_ms);

// ─── Main ──────────────────────────────────────────────────────────

int main(void)
{
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_SPI1_Init();
    MX_SPI2_Init();
    MX_I2C1_Init();
    
    // Load calibration from EEPROM
    eeprom_init(&hi2c1, AT24C256_ADDR);
    if (eeprom_load_calibration(g_state.ct_offset, g_state.ct_gain,
                                 &g_state.voltage_offset, &g_state.voltage_gain) == 0) {
        g_state.calibrated = true;
    } else {
        // Default calibration values
        for (int i = 0; i < NUM_CIRCUITS; i++) {
            g_state.ct_offset[i] = 0.0f;
            g_state.ct_gain[i] = 1.0f;
        }
        g_state.voltage_offset = 0.0f;
        g_state.voltage_gain = 1.0f;
    }
    
    // Initialize ADS131E08 ADC
    ads131e08_init(&hspi2, ADS131E08_DR_8000SPS);
    ads131e08_set_channel_enable(0xFF);  // Enable all 8 channels
    
    // Initialize CC1101 Sub-GHz radio
    cc1101_init(&hspi1, CC1101_CS_PIN, CC1101_GDO0_PIN, CC1101_GDO2_PIN);
    cc1101_set_frequency(868000000);
    cc1101_set_data_rate(10);  // 10 kbps
    cc1101_set_tx_power(0x0C);  // +10 dBm
    
    // Create message queue
    circuit_data_queue = osMessageQueueNew(CIRCUIT_DATA_QUEUE_LEN, sizeof(circuit_data_msg_t), NULL);
    
    // Initialize state
    g_state.tx_seq = 0;
    g_state.arc_fault_circuit = 255;
    
    // Create FreeRTOS tasks
    const osThreadAttr_t adc_task_attr = {
        .name = "adc_task",
        .stack_size = 2048,
        .priority = osPriorityAboveNormal,
    };
    const osThreadAttr_t arc_attr = {
        .name = "arc_detect",
        .stack_size = 4096,
        .priority = osPriorityHigh,
    };
    const osThreadAttr_t power_attr = {
        .name = "power_calc",
        .stack_size = 2048,
        .priority = osPriorityAboveNormal,
    };
    const osThreadAttr_t subghz_attr = {
        .name = "subghz_task",
        .stack_size = 2048,
        .priority = osPriorityNormal,
    };
    const osThreadAttr_t hb_attr = {
        .name = "heartbeat",
        .stack_size = 1024,
        .priority = osPriorityLow,
    };
    const osThreadAttr_t temp_attr = {
        .name = "temp_task",
        .stack_size = 1024,
        .priority = osPriorityLow,
    };
    
    adc_task_handle = osThreadNew(adc_task, NULL, &adc_task_attr);
    arc_detect_task_handle = osThreadNew(arc_detect_task, NULL, &arc_attr);
    power_calc_task_handle = osThreadNew(power_calc_task, NULL, &power_attr);
    subghz_task_handle = osThreadNew(subghz_task, NULL, &subghz_attr);
    heartbeat_task_handle = osThreadNew(heartbeat_task, NULL, &hb_attr);
    temp_task_handle = osThreadNew(temp_task, NULL, &temp_attr);
    
    // Start scheduler
    osKernelStart();
    
    while (1) {
        // Should never reach here
    }
}

// ─── ADC Sampling Task ──────────────────────────────────────────────

static void adc_task(void *argument)
{
    (void)argument;
    
    int16_t raw_samples[8];  // 8 channels from ADS131E08
    uint32_t sample_count = 0;
    
    while (1) {
        // Read all 8 channels from ADS131E08
        if (ads131e08_read_all_channels(&hspi2, raw_samples) == HAL_OK) {
            // Store samples in active buffer
            uint8_t buf = active_buffer;
            uint16_t idx = sample_index;
            
            // Distribute samples to per-circuit buffers
            // ADS131E08 channels 0-7 map to circuits 0-7 (or 8-15 for second ADC)
            for (int ch = 0; ch < 8; ch++) {
                if (idx < SAMPLE_BUFFER_SIZE) {
                    if (buf == 0) {
                        adc_buffer_a[ch][idx] = raw_samples[ch];
                    } else {
                        adc_buffer_b[ch][idx] = raw_samples[ch];
                    }
                }
            }
            
            sample_index++;
            
            // When buffer is full, signal processing task and switch buffers
            if (sample_index >= SAMPLE_BUFFER_SIZE) {
                sample_index = 0;
                active_buffer = 1 - active_buffer;  // Toggle buffer
                
                // Signal power calculation task
                // (In production, use task notification or semaphore)
            }
        }
        
        // Sample at 8 kHz = 125 µs per sample
        // HAL timer will handle precise timing
        osDelay(1);  // Approximate; production uses timer-driven DMA
    }
}

// ─── Arc Fault Detection Task ───────────────────────────────────────

static void arc_detect_task(void *argument)
{
    (void)argument;
    
    float baseline_energy[NUM_CIRCUITS] = {0};
    float window_energy[NUM_CIRCUITS] = {0};
    uint16_t burst_count[NUM_CIRCUITS] = {0};
    
    // High-frequency content buffer (above 10 kHz in frequency domain)
    float hf_energy[NUM_CIRCUITS] = {0};
    
    while (1) {
        // Process each circuit for arc fault signatures
        for (int ckt = 0; ckt < NUM_CIRCUITS; ckt++) {
            int16_t *buf = (active_buffer == 0) ? adc_buffer_b[ckt] : adc_buffer_a[ckt];
            
            // ── Method: Spectral Energy Ratio Test ──
            // 1. Compute energy in low-frequency band (50-500 Hz)
            // 2. Compute energy in high-frequency band (5-25 kHz)  
            // 3. If HF/LF ratio exceeds threshold → possible arc
            
            // Simplified time-domain approach:
            // Count zero crossings and sudden step changes
            
            float sum_sq = 0;
            float max_val = 0;
            int step_changes = 0;
            float prev = 0;
            
            for (int i = 0; i < ARC_DETECT_WINDOW && i < SAMPLE_BUFFER_SIZE; i++) {
                float sample = (float)buf[i] * g_state.ct_gain[ckt] - g_state.ct_offset[ckt];
                sum_sq += sample * sample;
                
                float abs_val = fabsf(sample);
                if (abs_val > max_val) max_val = abs_val;
                
                // Count step changes (sudden transitions characteristic of arcs)
                if (i > 0) {
                    float diff = fabsf(sample - prev);
                    float rms = sqrtf(sum_sq / (i + 1));
                    if (rms > 0.01f && diff > rms * 4.0f) {
                        step_changes++;
                    }
                }
                prev = sample;
            }
            
            float rms = sqrtf(sum_sq / ARC_DETECT_WINDOW);
            
            // Compute energy ratio: step change density vs expected for normal load
            float step_ratio = (float)step_changes / ARC_DETECT_WINDOW;
            
            // High step change ratio indicates arc fault
            // Normal loads: <0.01 step ratio
            // Arc faults: >0.05 step ratio
            
            if (rms > 0.1f && step_ratio > 0.03f) {
                // Possible arc detected
                burst_count[ckt]++;
                
                float energy_ratio = step_ratio / 0.01f;  // Normalized to expected
                
                if (energy_ratio > g_state.arc_energy_ratio) {
                    g_state.arc_energy_ratio = energy_ratio;
                }
                
                if (burst_count[ckt] >= ARC_MIN_BURSTS) {
                    // Arc fault confirmed!
                    g_state.arc_fault_circuit = ckt;
                    g_state.arc_confidence = (uint8_t)(energy_ratio / ARC_THRESHOLD * 100.0f);
                    if (g_state.arc_confidence > 100) g_state.arc_confidence = 100;
                    g_state.arc_duration_ms = burst_count[ckt] * (ARC_DETECT_WINDOW * 1000 / ADC_SAMPLE_RATE);
                    
                    // Determine arc type
                    uint8_t arc_type = 0;  // 0=series (default)
                    if (rms > 10.0f) {
                        arc_type = 1;  // Parallel (high current)
                    }
                    
                    // Send arc fault alert
                    send_arc_fault_alert(ckt, g_state.arc_confidence, arc_type, g_state.arc_duration_ms);
                    
                    ESP_LOGE("ARC", "ARC FAULT on circuit %d! confidence=%d%% type=%d duration=%dms",
                             ckt, g_state.arc_confidence, arc_type, g_state.arc_duration_ms);
                }
            } else {
                // No arc, reset burst counter
                if (burst_count[ckt] > 0) {
                    burst_count[ckt]--;
                }
                g_state.arc_energy_ratio = 0;
            }
        }
        
        // Check every 64 ms (ARC_DETECT_WINDOW / ADC_SAMPLE_RATE * 1000)
        osDelay(64);
    }
}

// ─── Power Calculation Task ─────────────────────────────────────────

static void power_calc_task(void *argument)
{
    (void)argument;
    
    uint32_t last_energy_update = 0;
    
    while (1) {
        // Calculate RMS values for each circuit
        for (int ckt = 0; ckt < NUM_CIRCUITS; ckt++) {
            int16_t *buf = (active_buffer == 0) ? adc_buffer_a[ckt] : adc_buffer_b[ckt];
            calculate_rms(ckt, buf, SAMPLE_BUFFER_SIZE);
        }
        
        // Calculate power factor for each circuit
        // (using voltage channel as reference)
        for (int ckt = 0; ckt < NUM_CIRCUITS; ckt++) {
            int16_t *current_buf = (active_buffer == 0) ? adc_buffer_a[ckt] : adc_buffer_b[ckt];
            int16_t *voltage_buf = (active_buffer == 0) ? adc_buffer_a[0] : adc_buffer_b[0];
            // Channel 0 is used as voltage reference
            calculate_power_factor(ckt, voltage_buf, current_buf, SAMPLE_BUFFER_SIZE);
        }
        
        // Calculate real power: P = V_rms * I_rms * PF
        for (int ckt = 0; ckt < NUM_CIRCUITS; ckt++) {
            g_state.real_power[ckt] = (uint16_t)(g_state.rms_voltage *
                                                  g_state.rms_current[ckt] *
                                                  fabsf(g_state.power_factor[ckt]));
        }
        
        // Update energy accumulation (Wh)
        uint32_t now = HAL_GetTick();
        if (now - last_energy_update >= 3600000) {  // Every hour
            for (int ckt = 0; ckt < NUM_CIRCUITS; ckt++) {
                g_state.energy_wh[ckt] += g_state.real_power[ckt];  // Approximate
            }
            last_energy_update = now;
        }
        
        // Prepare and queue circuit data message
        send_circuit_data();
        
        // Run every 500 ms
        osDelay(500);
    }
}

// ─── Calculate RMS ──────────────────────────────────────────────────

static void calculate_rms(uint8_t circuit, int16_t *samples, uint16_t num_samples)
{
    float sum_sq = 0;
    float offset = g_state.ct_offset[circuit];
    float gain = g_state.ct_gain[circuit];
    
    for (uint16_t i = 0; i < num_samples; i++) {
        float sample = ((float)samples[i] - offset) * gain;
        sum_sq += sample * sample;
    }
    
    float adc_rms = sqrtf(sum_sq / num_samples);
    
    // Convert ADC counts to current:
    // ADC = 24-bit, Vref = 2.5V, full scale = ±2.5V
    // Burden resistor voltage = I_primary / CT_RATIO * R_burden
    // Current = V_burden / R_burden * CT_RATIO
    // Simplified: I_rms = adc_rms * V_per_LSB / R_burden * CT_RATIO
    
    float v_per_lsb = 2.5f / 8388608.0f;  // 2.5V / 2^23
    g_state.rms_current[circuit] = adc_rms * v_per_lsb / BURDEN_RESISTOR * CT_RATIO;
    
    // Voltage channel (circuit 0 is repurposed for voltage sense)
    if (circuit == 0) {
        g_state.rms_voltage = adc_rms * VOLTAGE_SCALE * g_state.voltage_gain;
    }
}

// ─── Calculate Power Factor ─────────────────────────────────────────

static void calculate_power_factor(uint8_t circuit, int16_t *voltage_samples, int16_t *current_samples, uint16_t num_samples)
{
    // Power factor = P / (V_rms * I_rms)
    // P = (1/N) * Σ(v[i] * i[i])
    // This is the dot product method
    
    float offset_v = g_state.voltage_offset;
    float offset_i = g_state.ct_offset[circuit];
    float gain_v = g_state.voltage_gain;
    float gain_i = g_state.ct_gain[circuit];
    
    float sum_vi = 0;
    
    for (uint16_t i = 0; i < num_samples; i++) {
        float v = ((float)voltage_samples[i] - offset_v) * gain_v;
        float ci = ((float)current_samples[i] - offset_i) * gain_i;
        sum_vi += v * ci;
    }
    
    float real_power = sum_vi / num_samples;
    float apparent_power = g_state.rms_voltage * g_state.rms_current[circuit];
    
    if (apparent_power > 0.01f) {
        g_state.power_factor[circuit] = real_power / apparent_power;
        // Clamp to [-1, 1]
        if (g_state.power_factor[circuit] > 1.0f) g_state.power_factor[circuit] = 1.0f;
        if (g_state.power_factor[circuit] < -1.0f) g_state.power_factor[circuit] = -1.0f;
    } else {
        g_state.power_factor[circuit] = 0.0f;
    }
}

// ─── Send Circuit Data ─────────────────────────────────────────────

static void send_circuit_data(void)
{
    // Build the circuit data message
    uint8_t payload[PP_MAX_PAYLOAD];
    uint16_t pos = 0;
    
    // Header
    pp_circuit_data_header_t header = {
        .voltage_mv = (uint16_t)(g_state.rms_voltage * 1000),
        .frequency_cph = 5000,  // 50.00 Hz (assume, would measure in production)
        .num_active = 0,
        .circuit_mask = 0,
    };
    
    memcpy(&payload[pos], &header, sizeof(header));
    pos += sizeof(header);
    
    // Add readings for circuits with non-zero current
    for (int i = 0; i < NUM_CIRCUITS && pos + sizeof(pp_circuit_reading_t) <= PP_MAX_PAYLOAD; i++) {
        if (g_state.rms_current[i] > 0.01f) {  // > 10mA
            pp_circuit_reading_t reading = {
                .circuit_id = i,
                .current_ma = (uint16_t)(g_state.rms_current[i] * 1000),
                .power_w = g_state.real_power[i],
                .power_factor = (int16_t)(g_state.power_factor[i] * 10000),
                .energy_wh = g_state.energy_wh[i],
            };
            memcpy(&payload[pos], &reading, sizeof(reading));
            pos += sizeof(reading);
            header.num_active++;
            header.circuit_mask |= (1 << i);
        }
    }
    
    // Update header with correct count
    memcpy(&payload[0], &header, sizeof(header));
    
    // Build and send frame
    uint8_t frame_buf[256];
    uint16_t frame_len = pp_frame_build(
        NODE_ADDRESS, PP_ADDR_HUB, PP_MSG_CIRCUIT_DATA,
        g_state.tx_seq++, payload, pos,
        frame_buf, sizeof(frame_buf)
    );
    
    if (frame_len > 0) {
        cc1101_send_packet(frame_buf, frame_len);
        cc1101_set_rx_mode();
    }
}

// ─── Send Arc Fault Alert ──────────────────────────────────────────

static void send_arc_fault_alert(uint8_t circuit, uint8_t confidence, uint8_t arc_type, uint16_t duration_ms)
{
    pp_arc_fault_payload_t arc = {
        .circuit_id = circuit,
        .confidence_pct = confidence,
        .arc_type = arc_type,
        .timestamp_unix = g_state.uptime_seconds,  // Simplified timestamp
        .duration_ms = duration_ms,
        .severity = (confidence > 80) ? 4 : (confidence > 50) ? 3 : 2,
    };
    
    uint8_t frame_buf[64];
    uint16_t frame_len = pp_frame_build(
        NODE_ADDRESS, PP_ADDR_HUB, PP_MSG_ARC_FAULT_ALERT,
        g_state.tx_seq++, (const uint8_t *)&arc, sizeof(arc),
        frame_buf, sizeof(frame_buf)
    );
    
    if (frame_len > 0) {
        // Send 3 times for reliability (arc fault is critical)
        for (int i = 0; i < 3; i++) {
            cc1101_send_packet(frame_buf, frame_len);
            HAL_Delay(50);  // Brief delay between retransmissions
        }
        cc1101_set_rx_mode();
    }
}

// ─── Sub-GHz Communication Task ─────────────────────────────────────

static void subghz_task(void *argument)
{
    (void)argument;
    
    uint8_t rx_buf[256];
    
    cc1101_set_rx_mode();
    
    while (1) {
        // Check for incoming messages
        int16_t rx_len = cc1101_receive_packet(rx_buf, sizeof(rx_buf), 100);
        
        if (rx_len > 0) {
            pp_frame_header_t header;
            const uint8_t *payload;
            uint16_t payload_len;
            
            if (pp_frame_parse(rx_buf, rx_len, &header, &payload, &payload_len) == 0) {
                // Handle commands from hub
                if (header.type == PP_MSG_CALIBRATION) {
                    const pp_calibration_payload_t *cal = (const pp_calibration_payload_t *)payload;
                    // Apply calibration
                    if (cal->cal_type == 0) {  // CT zero offset
                        g_state.ct_offset[cal->param_id] = cal->value / 1000.0f;
                    } else if (cal->cal_type == 1) {  // CT gain
                        g_state.ct_gain[cal->param_id] = cal->value / 10000.0f;
                    }
                    eeprom_save_calibration(g_state.ct_offset, g_state.ct_gain,
                                            &g_state.voltage_offset, &g_state.voltage_gain);
                }
                
                // Send ACK
                uint8_t ack_buf[32];
                pp_ack_payload_t ack = { .acked_seq = header.seq, .status = 0 };
                uint16_t ack_len = pp_frame_build(
                    NODE_ADDRESS, header.src, PP_MSG_ACK,
                    g_state.tx_seq++, (const uint8_t *)&ack, sizeof(ack),
                    ack_buf, sizeof(ack_buf)
                );
                if (ack_len > 0) {
                    cc1101_send_packet(ack_buf, ack_len);
                    cc1101_set_rx_mode();
                }
                
                g_state.last_hub_contact_ms = HAL_GetTick();
            }
        }
        
        osDelay(10);
    }
}

// ─── Heartbeat Task ─────────────────────────────────────────────────

static void heartbeat_task(void *argument)
{
    (void)argument;
    
    while (1) {
        pp_heartbeat_payload_t hb = {
            .node_type = PP_NODE_CIRCUIT_MONITOR,
            .battery_pct = 255,  // Mains powered
            .uptime_min = (uint16_t)(g_state.uptime_seconds / 60),
            .num_circuits = NUM_CIRCUITS,
            .firmware_ver = (FW_VERSION_MAJOR << 4) | FW_VERSION_MINOR,
            .signal_rssi = (int8_t)cc1101_get_rssi(),
            .flags = g_state.calibrated ? 0x02 : 0x00,  // bit1=calibrated
        };
        
        uint8_t tx_buf[64];
        uint16_t tx_len = pp_frame_build(
            NODE_ADDRESS, PP_ADDR_HUB, PP_MSG_HEARTBEAT,
            g_state.tx_seq++, (const uint8_t *)&hb, sizeof(hb),
            tx_buf, sizeof(tx_buf)
        );
        
        if (tx_len > 0) {
            cc1101_send_packet(tx_buf, tx_len);
            cc1101_set_rx_mode();
        }
        
        g_state.uptime_seconds += 30;
        osDelay(30000);  // Every 30 seconds
    }
}

// ─── Temperature Monitoring Task ────────────────────────────────────

static void temp_task(void *argument)
{
    (void)argument;
    
    while (1) {
        // Read panel temperature from TMP117
        float temp = tmp117_read_temperature(&hi2c1);
        g_state.panel_temp_c = temp;
        
        // Check for over-temperature
        if (temp > 70.0f) {
            ESP_LOGW("TEMP", "Panel temperature high: %.1f°C", temp);
        }
        if (temp > 85.0f) {
            ESP_LOGE("TEMP", "Panel temperature CRITICAL: %.1f°C", temp);
        }
        
        osDelay(5000);  // Read every 5 seconds
    }
}

// ─── STM32 HAL Callbacks ───────────────────────────────────────────

void HAL_SPI_RxCpltCallback(SPI_HandleTypeDef *hspi)
{
    // ADS131E08 DMA complete callback
    // Signal ADC task that new data is available
}

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    // CC1101 GDO0 interrupt — packet received or TX complete
    if (GPIO_Pin == CC1101_GDO0_PIN) {
        // Signal Sub-GHz task
    }
}

// ─── System Clock Configuration (170 MHz) ───────────────────────────

void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
    
    HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1);
    
    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    RCC_OscInitStruct.HSEState = RCC_HSE_ON;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
    RCC_OscInitStruct.PLL.PLLM = RCC_PLLM_DIV6;    // 8 MHz / 6 = 1.33 MHz
    RCC_OscInitStruct.PLL.PLLN = 128;                // 1.33 * 128 = 170.67 MHz
    RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
    RCC_OscInitStruct.PLL.PLLQ = RCC_PLLQ_DIV4;
    RCC_OscInitStruct.PLL.PLLR = RCC_PLLR_DIV2;
    HAL_RCC_OscConfig(&RCC_OscInitStruct);
    
    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK |
                                   RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;
    HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_4);
}