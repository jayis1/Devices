/**
 * BreathHome - HVAC Controller Node Firmware
 * ESP32-S3 + CC2652R7 (Zigbee) + SX1261 (Sub-GHz)
 * 
 * Controls ventilation, air purifiers, smart vents, and exhaust fans
 * based on air quality data from room sensors. Bridges Sub-GHz mesh
 * to Zigbee smart home devices.
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/drivers/uart.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

/* ========== CONSTANTS ========== */
#define MESH_FRAME_MS             900
#define MESH_SLOT_MS              50
#define HVAC_SLOT                  9

#define RELAY_FAN_OVERRIDE         0
#define RELAY_BATH_EXHAUST         1
#define RELAY_RANGE_HOOD           2
#define RELAY_WHOLE_HOUSE_FAN      3

#define MAX_VENT_ZONES             8
#define MAX_ZIGBEE_DEVICES        32

#define FILTER_REPLACE_THRESHOLD   20.0f   /* percent remaining */
#define FILTER_CRITICAL_THRESHOLD  10.0f   /* percent remaining */

/* ========== DATA STRUCTURES ========== */

typedef struct __attribute__((packed)) {
    uint8_t  src_id;
    uint8_t  dst_id;
    uint8_t  msg_type;
    uint16_t seq_num;
    uint8_t  payload[48];
    uint16_t crc16;
} mesh_packet_t;

typedef struct {
    uint8_t  vent_positions[MAX_VENT_ZONES];  /* 0-100% per zone */
    uint8_t  purifier_speed;                    /* 0=off, 1-4 */
    float    filter_health_pct;
    float    duct_pressure_pa;
    float    supply_air_temp_c;
    float    blower_current_ma;
    uint8_t  relay_states;  /* bitmask */
    uint32_t last_update_ms;
} hvac_state_t;

typedef struct {
    uint8_t  device_type;  /* 0=vent, 1=purifier, 2=thermostat, 3=switch */
    uint16_t zigbee_short_addr;
    uint8_t  ieee_addr[8];
    uint8_t  room_id;
    uint8_t  vent_position;  /* 0-100% (vents only) */
    uint8_t  purifier_speed;  /* 0-4 (purifiers only) */
    uint32_t last_seen_ms;
    uint8_t  active;
} zigbee_device_t;

typedef struct {
    float pressure_history[168];  /* 168 hours = 7 days of hourly readings */
    float current_history[168];
    float temperature_history[168];
    int   history_idx;
    float filter_rul_pct;
    uint32_t filter_install_date;
} filter_health_t;

/* ========== GLOBALS ========== */
static hvac_state_t hvac;
static zigbee_device_t zigbee_devices[MAX_ZIGBEE_DEVICES];
static uint8_t zigbee_num_devices = 0;
static filter_health_t filter;
static uint8_t node_id = 1;  /* HVAC controller is always node 1 */
static uint16_t mesh_seq = 0;

/* Room air quality (received from hub) */
typedef struct {
    uint16_t aqi_score;
    uint8_t  aqi_category;
    float    pm2_5;
    float    co2;
    float    voc_index;
    float    humidity;
} room_aqi_t;

static room_aqi_t room_aqi[8];  /* AQI per room zone */

/* ========== GPIO DEFINITIONS ========== */

/* Relays */
#define RELAY_FAN_PIN        18
#define RELAY_BATH_PIN       19
#define RELAY_RANGE_PIN      20
#define RELAY_WHOLE_HOUSE_PIN 21

/* 433MHz transmitter */
#define RF_433_TX_PIN        22

/* Sensors */
#define CURRENT_SENSE_ADC_CH  1  /* ADC1_CH1, GPIO15 */
#define ONEWIRE_PIN           14  /* DS18B20 */
#define I2C_BME688_ADDR       0x76
#define I2C_BMP390_ADDR       0x77

/* ========== SENSOR READINGS ========== */

static const struct device *i2c_dev;
static const struct device *adc_dev;

/**
 * read_duct_pressure - Read duct static pressure from BMP390
 */
static float read_duct_pressure(void)
{
    /* BMP390 I2C read */
    uint8_t reg = 0x04;  /* Press MSB */
    uint8_t data[3];
    
    int ret = i2c_write(i2c_dev, &reg, 1, I2C_BMP390_ADDR);
    if (ret != 0) return 101325.0f;  /* Default: atmospheric pressure */
    
    k_msleep(10);
    ret = i2c_read(i2c_dev, data, 3, I2C_BMP390_ADDR);
    if (ret != 0) return 101325.0f;
    
    /* Convert to Pascals */
    int32_t raw = (data[0] << 16) | (data[1] << 8) | data[2];
    raw >>= 4;  /* 20-bit value */
    
    /* Simplified conversion - in production use BMP390 compensation */
    float pressure_pa = (float)raw * 0.016f;  /* Simplified scaling */
    
    return pressure_pa;
}

/**
 * read_blower_current - Read HVAC blower current from SCT013-030
 */
static float read_blower_current(void)
{
    /* ADC read from SCT013-030 current transformer */
    /* SCT013-030: 30A/1V, burden resistor on PCB */
    int16_t adc_val;
    
    /* Read ADC channel */
    struct adc_sequence seq = {
        .channels = BIT(0),
        .buffer = &adc_val,
        .buffer_size = sizeof(adc_val),
    };
    adc_read(adc_dev, &seq);
    
    /* Convert ADC value to current */
    /* ADC: 12-bit, 3.3V reference */
    /* SCT013-030: 30A = 1V output, through 10:1 voltage divider */
    float voltage = (float)adc_val * 3.3f / 4096.0f;
    float current = voltage * 30.0f * 10.0f;  /* Scale by turns ratio */
    
    return current;
}

/**
 * read_supply_air_temp - Read supply air temperature from DS18B20
 */
static float read_supply_air_temp(void)
{
    /* DS18B20 1-Wire read */
    /* Simplified - in production use 1-Wire driver */
    
    /* 1-Wire reset */
    /* 1-Wire write: 0xCC (Skip ROM), 0x44 (Convert T) */
    /* Wait 750ms for conversion */
    /* 1-Wire write: 0xCC, 0xBE (Read Scratchpad) */
    /* Read 9 bytes: temp LSB, temp MSB, ... */
    
    /* Placeholder */
    return 22.0f;
}

/**
 * read_bme688 - Read ambient environment near air handler
 */
static void read_bme688(float *temp, float *humidity, float *pressure)
{
    uint8_t reg = 0x1D;
    uint8_t data[8];
    
    int ret = i2c_write(i2c_dev, &reg, 1, I2C_BME688_ADDR);
    if (ret != 0) {
        *temp = 22.0f;
        *humidity = 45.0f;
        *pressure = 101325.0f;
        return;
    }
    
    k_msleep(50);
    ret = i2c_read(i2c_dev, data, 8, I2C_BME688_ADDR);
    if (ret != 0) {
        *temp = 22.0f;
        *humidity = 45.0f;
        *pressure = 101325.0f;
        return;
    }
    
    *temp = 22.5f;
    *humidity = 45.0f;
    *pressure = 101325.0f;
}

/* ========== FILTER HEALTH PREDICTION ========== */

/**
 * update_filter_health - Estimate remaining filter life
 * 
 * Uses duct static pressure delta (current vs. clean) and
 * blower current to estimate filter clogging.
 * 
 * A clean filter has low pressure drop and moderate current.
 * A clogged filter has high pressure drop and high current
 * (or very low current if the blower is stalled).
 */
static void update_filter_health(void)
{
    float pressure_pa = read_duct_pressure();
    float current_ma = read_blower_current();
    float supply_temp = read_supply_air_temp();
    
    /* Store readings */
    int idx = filter.history_idx % 168;
    filter.pressure_history[idx] = pressure_pa;
    filter.current_history[idx] = current_ma;
    filter.temperature_history[idx] = supply_temp;
    filter.history_idx++;
    
    /* Filter health estimation */
    /* Baseline: clean filter at ~0.5" WC = 125 Pa pressure drop */
    /* Clogged filter: >1.0" WC = 250 Pa */
    /* Blower current increases as filter clogs (more effort) */
    
    float clean_pressure = 125.0f;  /* Pa, baseline */
    float clogged_pressure = 250.0f;  /* Pa, needs replacement */
    
    float pressure_ratio = (pressure_pa - clean_pressure) / (clogged_pressure - clean_pressure);
    if (pressure_ratio < 0) pressure_ratio = 0;
    if (pressure_ratio > 1) pressure_ratio = 1;
    
    /* Current-based verification: current should increase with pressure */
    float current_ratio = 0;
    if (current_ma > 0) {
        float baseline_current = 2.5f;  /* Amps, clean filter */
        float max_current = 4.5f;  /* Amps, clogged */
        current_ratio = (current_ma - baseline_current) / (max_current - baseline_current);
        if (current_ratio < 0) current_ratio = 0;
        if (current_ratio > 1) current_ratio = 1;
    }
    
    /* Weighted average: pressure is more reliable */
    float filter_degradation = 0.65f * pressure_ratio + 0.35f * current_ratio;
    
    /* Time-based degradation (minimum 1% per day even without pressure data) */
    uint32_t days_since_install = (k_uptime_get_32() - filter.filter_install_date) / 86400000;
    float time_degradation = (float)days_since_install / 90.0f;  /* 90-day filter life baseline */
    if (time_degradation > 1) time_degradation = 1;
    
    /* Combine: use max of sensor-based and time-based */
    float total_degradation = filter_degradation > time_degradation ? 
                               filter_degradation : time_degradation;
    
    filter.filter_rul_pct = 100.0f * (1.0f - total_degradation);
    if (filter.filter_rul_pct < 0) filter.filter_rul_pct = 0;
    if (filter.filter_rul_pct > 100) filter.filter_rul_pct = 100;
    
    hvac.filter_health_pct = filter.filter_rul_pct;
    hvac.duct_pressure_pa = pressure_pa;
    hvac.supply_air_temp_c = supply_temp;
    hvac.blower_current_ma = current_ma;
}

/* ========== ZIGBEE CONTROL ========== */

/**
 * zigbee_send_vent_command - Set smart vent position for a room
 */
static int zigbee_send_vent_command(uint8_t device_idx, uint8_t position)
{
    if (device_idx >= MAX_ZIGBEE_DEVICES) return -1;
    if (!zigbee_devices[device_idx].active) return -1;
    if (position > 100) position = 100;
    
    /* Build Zigbee ZCL command for vent control */
    /* Cluster 0xBREA (BreathHome custom), Attribute 0x0001 (Vent Position) */
    /* Send via UART to CC2652R7 Zigbee coordinator */
    
    uint8_t cmd[16];
    cmd[0] = 0x01;  /* UART command: Zigbee transmit */
    cmd[1] = 0x00;  /* Frame type: data */
    cmd[2] = zigbee_devices[device_idx].zigbee_short_addr & 0xFF;
    cmd[3] = (zigbee_devices[device_idx].zigbee_short_addr >> 8) & 0xFF;
    cmd[4] = 0x01;  /* Endpoint */
    cmd[5] = 0xBH;  /* BreathHome cluster MSB (placeholder) */
    cmd[6] = 0xEA;  /* BreathHome cluster LSB */
    cmd[7] = 0x01;  /* Command: write attribute */
    cmd[8] = 0x00;  /* Attribute ID: 0x0001 (Vent Position) */
    cmd[9] = 0x01;
    cmd[10] = 0x20;  /* Data type: uint8 */
    cmd[11] = position;  /* Value */
    /* CRC */
    uint16_t crc = 0;
    for (int i = 2; i < 12; i++) {
        crc ^= (uint16_t)cmd[i] << 8;
        for (int j = 0; j < 8; j++) {
            if (crc & 0x8000) crc = (crc << 1) ^ 0x1021;
            else crc <<= 1;
        }
    }
    cmd[12] = (crc >> 8) & 0xFF;
    cmd[13] = crc & 0xFF;
    
    /* Send via UART to CC2652R7 */
    /* uart_send(uart_dev, cmd, 14); */
    
    zigbee_devices[device_idx].vent_position = position;
    return 0;
}

/**
 * zigbee_send_purifier_command - Set air purifier speed
 */
static int zigbee_send_purifier_command(uint8_t device_idx, uint8_t speed)
{
    if (device_idx >= MAX_ZIGBEE_DEVICES) return -1;
    if (!zigbee_devices[device_idx].active) return -1;
    if (speed > 4) speed = 4;
    
    /* Build Zigbee ZCL command for purifier speed */
    /* Cluster 0xBREA, Attribute 0x0002 (Purifier Speed) */
    
    uint8_t cmd[14];
    cmd[0] = 0x01;  /* UART command */
    cmd[1] = 0x00;
    cmd[2] = zigbee_devices[device_idx].zigbee_short_addr & 0xFF;
    cmd[3] = (zigbee_devices[device_idx].zigbee_short_addr >> 8) & 0xFF;
    cmd[4] = 0x01;  /* Endpoint */
    cmd[5] = 0xBH;
    cmd[6] = 0xEA;
    cmd[7] = 0x01;
    cmd[8] = 0x02;  /* Attribute 0x0002 */
    cmd[9] = 0x00;
    cmd[10] = 0x30;  /* Data type: uint8 */
    cmd[11] = speed;
    /* CRC */
    uint16_t crc = 0;
    for (int i = 2; i < 12; i++) {
        crc ^= (uint16_t)cmd[i] << 8;
        for (int j = 0; j < 8; j++) {
            if (crc & 0x8000) crc = (crc << 1) ^ 0x1021;
            else crc <<= 1;
        }
    }
    cmd[12] = (crc >> 8) & 0xFF;
    cmd[13] = crc & 0xFF;
    
    /* uart_send(uart_dev, cmd, 14); */
    
    zigbee_devices[device_idx].purifier_speed = speed;
    return 0;
}

/* ========== 433MHZ RF CONTROL ========== */

/**
 * rf433_send_command - Send RF command to dumb appliances (range hood, exhaust fan)
 * Uses FS1000A 433MHz transmitter with simple OOK encoding
 */
static void rf433_send_command(uint32_t code, uint8_t repeats)
{
    /* Simple OOK protocol: preamble + code + checksum */
    for (uint8_t r = 0; r < repeats; r++) {
        /* Preamble: 12ms high, 12ms low */
        gpio_pin_set_raw(gpio_port, RF_433_TX_PIN, 1);
        k_msleep(12);
        gpio_pin_set_raw(gpio_port, RF_433_TX_PIN, 0);
        k_msleep(12);
        
        /* Data: 32 bits, Manchester encoded */
        for (int i = 31; i >= 0; i--) {
            if (code & (1UL << i)) {
                /* Bit 1: 1.2ms high, 0.6ms low */
                gpio_pin_set_raw(gpio_port, RF_433_TX_PIN, 1);
                k_usleep(1200);
                gpio_pin_set_raw(gpio_port, RF_433_TX_PIN, 0);
                k_usleep(600);
            } else {
                /* Bit 0: 0.6ms high, 1.2ms low */
                gpio_pin_set_raw(gpio_port, RF_433_TX_PIN, 1);
                k_usleep(600);
                gpio_pin_set_raw(gpio_port, RF_433_TX_PIN, 0);
                k_usleep(1200);
            }
        }
        
        /* Inter-message gap */
        k_msleep(10);
    }
}

/* ========== RELAY CONTROL ========== */

/**
 * set_relay - Set relay state for HVAC control
 */
static void set_relay(uint8_t relay, bool on)
{
    uint8_t pin;
    switch (relay) {
        case RELAY_FAN_OVERRIDE:    pin = RELAY_FAN_PIN; break;
        case RELAY_BATH_EXHAUST:    pin = RELAY_BATH_PIN; break;
        case RELAY_RANGE_HOOD:      pin = RELAY_RANGE_PIN; break;
        case RELAY_WHOLE_HOUSE_FAN: pin = RELAY_WHOLE_HOUSE_PIN; break;
        default: return;
    }
    
    gpio_pin_set_raw(gpio_port, pin, on ? 1 : 0);
    
    if (on) {
        hvac.relay_states |= (1 << relay);
    } else {
        hvac.relay_states &= ~(1 << relay);
    }
}

/* ========== VENTILATION LOGIC ========== */

/**
 * ventilation_control_loop - Main logic for controlling ventilation
 * 
 * Reads room AQI data and decides:
 * 1. Which rooms need more ventilation (open vents)
 * 2. Which rooms have good air (close vents to redirect flow)
 * 3. Whether to run the air purifier and at what speed
 * 4. Whether to activate exhaust fans (bathroom, kitchen, whole-house)
 * 5. Whether to override the furnace fan
 * 
 * Safety: Never close all vents (furnace damage risk)
 */
static void ventilation_control_loop(void)
{
    uint8_t open_vents = 0;
    uint8_t worst_room = 0;
    uint16_t worst_aqi = 0;
    
    /* Find worst AQI room */
    for (int i = 0; i < MAX_VENT_ZONES; i++) {
        if (room_aqi[i].aqi_score > worst_aqi) {
            worst_aqi = room_aqi[i].aqi_score;
            worst_room = i;
        }
    }
    
    /* Adjust vent positions based on AQI */
    for (int i = 0; i < MAX_VENT_ZONES; i++) {
        if (!room_aqi[i].aqi_score) continue;  /* No data for this room */
        
        uint8_t target_position;
        uint16_t aqi = room_aqi[i].aqi_score;
        
        if (aqi <= 50) {
            /* Good air: reduce ventilation to this room, redirect to worse rooms */
            target_position = 30;  /* Minimum 30% - never close fully (safety) */
        } else if (aqi <= 100) {
            /* Moderate: normal ventilation */
            target_position = 60;
        } else if (aqi <= 150) {
            /* Unhealthy for sensitive: increase ventilation */
            target_position = 80;
        } else {
            /* Unhealthy+: maximum ventilation */
            target_position = 100;
        }
        
        /* Find matching Zigbee vent device */
        for (int d = 0; d < MAX_ZIGBEE_DEVICES; d++) {
            if (zigbee_devices[d].active && 
                zigbee_devices[d].room_id == i &&
                zigbee_devices[d].device_type == 0) {  /* Vent */
                zigbee_send_vent_command(d, target_position);
            }
        }
        
        hvac.vent_positions[i] = target_position;
        if (target_position > 30) open_vents++;
    }
    
    /* Safety: ensure at least one vent is open */
    if (open_vents == 0) {
        hvac.vent_positions[0] = 50;  /* Open living room vent */
    }
    
    /* Air purifier control */
    if (worst_aqi > 150) {
        hvac.purifier_speed = 4;  /* High */
    } else if (worst_aqi > 100) {
        hvac.purifier_speed = 3;  /* Medium-high */
    } else if (worst_aqi > 50) {
        hvac.purifier_speed = 2;  /* Medium */
    } else {
        hvac.purifier_speed = 0;  /* Off */
    }
    
    /* Send purifier speed to all purifier devices */
    for (int d = 0; d < MAX_ZIGBEE_DEVICES; d++) {
        if (zigbee_devices[d].active && zigbee_devices[d].device_type == 1) {
            zigbee_send_purifier_command(d, hvac.purifier_speed);
        }
    }
    
    /* Relay control for exhaust fans */
    
    /* Bathroom exhaust: run if any bathroom has high humidity or mold risk */
    bool bathroom_exhaust_needed = false;
    for (int i = 0; i < MAX_VENT_ZONES; i++) {
        if (room_aqi[i].humidity > 70.0f || 
            (room_aqi[i].voc_index > 200 && room_aqi[i].humidity > 50)) {
            bathroom_exhaust_needed = true;
        }
    }
    set_relay(RELAY_BATH_EXHAUST, bathroom_exhaust_needed);
    
    /* Range hood: run if kitchen has high VOC/NOx (cooking fumes) */
    bool range_hood_needed = false;
    for (int i = 0; i < MAX_VENT_ZONES; i++) {
        if (room_aqi[i].voc_index > 300 || room_aqi[i].co2 > 1200) {
            /* Assume zone 2 is kitchen */
            if (i == 2) range_hood_needed = true;
        }
    }
    set_relay(RELAY_RANGE_HOOD, range_hood_needed);
    
    /* Also send 433MHz command for dumb range hoods */
    if (range_hood_needed) {
        rf433_send_command(0x55AA55AA, 3);  /* Range hood ON code */
    }
    
    /* Furnace fan override: run if any room needs extra ventilation */
    bool fan_override = false;
    for (int i = 0; i < MAX_VENT_ZONES; i++) {
        if (room_aqi[i].aqi_score > 150) {
            fan_override = true;
            break;
        }
    }
    set_relay(RELAY_FAN_OVERRIDE, fan_override);
    
    /* Whole-house fan: run if outdoor air is better than indoor */
    /* (This requires outdoor AQI data from cloud - simplified here) */
    bool whole_house_fan = false;
    for (int i = 0; i < MAX_VENT_ZONES; i++) {
        if (room_aqi[i].aqi_score > 200) {
            whole_house_fan = true;
            break;
        }
    }
    set_relay(RELAY_WHOLE_HOUSE_FAN, whole_house_fan);
}

/* ========== MESH COMMUNICATION ========== */

/**
 * mesh_send_hvac_status - Send HVAC status to hub
 */
static int mesh_send_hvac_status(void)
{
    mesh_packet_t pkt;
    pkt.src_id = node_id;
    pkt.dst_id = 0;  /* Hub */
    pkt.msg_type = 0x06;  /* HVAC_STATUS */
    pkt.seq_num = mesh_seq++;
    
    /* Pack HVAC state */
    uint8_t *p = pkt.payload;
    for (int i = 0; i < MAX_VENT_ZONES; i++) {
        *p++ = hvac.vent_positions[i];
    }
    *p++ = hvac.purifier_speed;
    memcpy(p, &hvac.filter_health_pct, 4); p += 4;
    memcpy(p, &hvac.duct_pressure_pa, 4); p += 4;
    memcpy(p, &hvac.supply_air_temp_c, 4); p += 4;
    memcpy(p, &hvac.blower_current_ma, 4); p += 4;
    *p++ = hvac.relay_states;
    
    /* sx1261_transmit_packet(&pkt); */
    return 0;
}

/**
 * mesh_process_command - Process command from hub
 */
static void mesh_process_command(const mesh_packet_t *cmd)
{
    if (cmd->msg_type != 0x04) return;  /* HVAC_COMMAND */
    
    uint8_t command = cmd->payload[0];
    uint8_t room_id = cmd->payload[1];
    uint8_t value = cmd->payload[2];
    
    switch (command) {
        case 0:  /* Set vent position */
            if (room_id < MAX_VENT_ZONES) {
                hvac.vent_positions[room_id] = value;
                for (int d = 0; d < MAX_ZIGBEE_DEVICES; d++) {
                    if (zigbee_devices[d].active && 
                        zigbee_devices[d].room_id == room_id &&
                        zigbee_devices[d].device_type == 0) {
                        zigbee_send_vent_command(d, value);
                    }
                }
            }
            break;
        case 1:  /* Set purifier speed */
            hvac.purifier_speed = value;
            for (int d = 0; d < MAX_ZIGBEE_DEVICES; d++) {
                if (zigbee_devices[d].active && zigbee_devices[d].device_type == 1) {
                    zigbee_send_purifier_command(d, value);
                }
            }
            break;
        case 2:  /* Fan on */
            set_relay(RELAY_FAN_OVERRIDE, true);
            break;
        case 3:  /* Fan off */
            set_relay(RELAY_FAN_OVERRIDE, false);
            break;
        case 4:  /* Range hood on */
            set_relay(RELAY_RANGE_HOOD, true);
            rf433_send_command(0x55AA55AA, 3);
            break;
        case 5:  /* Range hood off */
            set_relay(RELAY_RANGE_HOOD, false);
            rf433_send_command(0xAA55AA55, 3);
            break;
        case 6:  /* Bathroom exhaust on */
            set_relay(RELAY_BATH_EXHAUST, true);
            break;
        case 7:  /* Bathroom exhaust off */
            set_relay(RELAY_BATH_EXHAUST, false);
            break;
    }
}

/* ========== MAIN THREADS ========== */

static void sensor_thread(void *p1, void *p2, void *p3)
{
    /* Initialize sensors */
    float temp, hum, press;
    
    while (1) {
        /* Read sensors */
        read_bme688(&temp, &hum, &press);
        update_filter_health();
        
        k_msleep(30000);  /* Every 30 seconds */
    }
}

static void ventilation_thread(void *p1, void *p2, void *p3)
{
    /* Wait for initial data */
    k_msleep(5000);
    
    while (1) {
        ventilation_control_loop();
        mesh_send_hvac_status();
        
        k_msleep(30000);  /* Every 30 seconds */
    }
}

static void mesh_thread(void *p1, void *p2, void *p3)
{
    /* Initialize SX1261 */
    /* sx1261_init(); */
    
    while (1) {
        /* Listen for mesh commands from hub */
        /* Process received packets in mesh_process_command() */
        
        /* Send status in assigned slot */
        int64_t frame_start = k_uptime_get() % MESH_FRAME_MS;
        int64_t slot_time = HVAC_SLOT * MESH_SLOT_MS;
        int64_t wait_time = slot_time - frame_start;
        
        if (wait_time > 0) {
            k_msleep(wait_time);
        }
        
        mesh_send_hvac_status();
        
        /* Listen for rest of frame */
        k_msleep(MESH_FRAME_MS - MESH_SLOT_MS);
    }
}

K_THREAD_DEFINE(sensor_tid, 1024, sensor_thread, NULL, NULL, NULL, 3, 0, 0);
K_THREAD_DEFINE(vent_tid, 2048, ventilation_thread, NULL, NULL, NULL, 4, 0, 0);
K_THREAD_DEFINE(mesh_tid, 1024, mesh_thread, NULL, NULL, NULL, 5, 0, 0);

int main(void)
{
    /* Initialize GPIOs */
    /* Relays */
    gpio_pin_configure(gpio_port, RELAY_FAN_PIN, GPIO_OUTPUT_LOW);
    gpio_pin_configure(gpio_port, RELAY_BATH_PIN, GPIO_OUTPUT_LOW);
    gpio_pin_configure(gpio_port, RELAY_RANGE_PIN, GPIO_OUTPUT_LOW);
    gpio_pin_configure(gpio_port, RELAY_WHOLE_HOUSE_PIN, GPIO_OUTPUT_LOW);
    gpio_pin_configure(gpio_port, RF_433_TX_PIN, GPIO_OUTPUT_LOW);
    
    /* Initialize I2C */
    i2c_dev = DEVICE_DT_GET(DT_NODELABEL(i2c0));
    
    /* Initialize ADC for current sensor */
    adc_dev = DEVICE_DT_GET(DT_NODELABEL(adc1));
    
    /* Initialize filter health tracking */
    filter.filter_rul_pct = 100.0f;
    filter.filter_install_date = k_uptime_get_32();
    filter.history_idx = 0;
    
    printk("BreathHome HVAC Controller started\n");
    printk("Waiting for hub pairing and Zigbee devices...\n");
    
    return 0;
}