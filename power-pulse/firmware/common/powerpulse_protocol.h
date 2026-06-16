/**
 * PowerPulse Protocol — Shared wireless frame definitions
 * 
 * Common header used by all nodes (Hub, Circuit Monitor, Appliance Tag, Solar).
 * Defines frame format, message types, and node address scheme.
 */

#ifndef POWERPULSE_PROTOCOL_H
#define POWERPULSE_PROTOCOL_H

#include <stdint.h>
#include <stdbool.h>

// ─── Frame Constants ───────────────────────────────────────────────

#define PP_SOF              0xAA
#define PP_MAX_PAYLOAD      200
#define PP_HEADER_SIZE      10    // SOF(1) + LEN(1) + SRC(2) + DST(2) + TYPE(1) + SEQ(2) + CRC(2)... wait
                                  // Actually: SOF(1) + LEN(1) + SRC(2) + DST(2) + TYPE(1) + SEQ(2) = 9 bytes header
                                  // CRC is at end of payload, so header = 9, total = 9 + payload_len + 2
#define PP_FRAME_OVERHEAD   11    // header(9) + CRC(2)
#define PP_MAX_FRAME        (PP_HEADER_SIZE + PP_MAX_PAYLOAD + 2)  // 211 bytes

// ─── Node Addresses ───────────────────────────────────────────────

#define PP_ADDR_BROADCAST   0xFFFF
#define PP_ADDR_HUB         0x0001

// Circuit monitor: 0x0100 + panel_id (0-15)
// e.g., 0x0100, 0x0101, ...
#define PP_ADDR_CIRCUIT_MONITOR(panel_id)  ((0x0100) | ((panel_id) & 0x0F))

// Appliance tag: 0x0200 + tag_id (0-255)
#define PP_ADDR_APPLIANCE_TAG(tag_id)      ((0x0200) | ((tag_id) & 0xFF))

// Solar node: 0x0300 + node_id (0-15)
#define PP_ADDR_SOLAR_NODE(node_id)        ((0x0300) | ((node_id) & 0x0F))

// ─── Message Types ────────────────────────────────────────────────

typedef enum __attribute__((packed)) {
    PP_MSG_HEARTBEAT         = 0x01,
    PP_MSG_CIRCUIT_DATA      = 0x02,
    PP_MSG_ARC_FAULT_ALERT   = 0x03,
    PP_MSG_APPLIANCE_DATA    = 0x04,
    PP_MSG_APPLIANCE_CMD     = 0x05,
    PP_MSG_SOLAR_DATA        = 0x06,
    PP_MSG_SOLAR_CMD         = 0x07,
    PP_MSG_CALIBRATION       = 0x08,
    PP_MSG_OTA_UPDATE        = 0x09,
    PP_MSG_OVERLOAD_ALERT    = 0x0A,
    PP_MSG_TIME_SYNC         = 0x0B,
    PP_MSG_ACK               = 0xFF,
} pp_msg_type_t;

// ─── Node Types ───────────────────────────────────────────────────

typedef enum __attribute__((packed)) {
    PP_NODE_HUB              = 0x01,
    PP_NODE_CIRCUIT_MONITOR  = 0x02,
    PP_NODE_APPLIANCE_TAG    = 0x03,
    PP_NODE_SOLAR            = 0x04,
} pp_node_type_t;

// ─── Frame Structure ──────────────────────────────────────────────

typedef struct __attribute__((packed)) {
    uint8_t  sof;           // 0xAA
    uint8_t  len;           // Payload length (0 - PP_MAX_PAYLOAD)
    uint16_t src;           // Source address
    uint16_t dst;           // Destination address (0xFFFF = broadcast)
    uint8_t  type;          // pp_msg_type_t
    uint16_t seq;           // Sequence number (wraps around)
    // ... payload follows (len bytes) ...
    // uint16_t crc;        // CRC16-CCITT at end (after payload)
} pp_frame_header_t;

// ─── Heartbeat Payload ────────────────────────────────────────────

typedef struct __attribute__((packed)) {
    uint8_t  node_type;     // pp_node_type_t
    uint8_t  battery_pct;   // 0-100 (255 = mains powered)
    uint16_t uptime_min;    // Uptime in minutes
    uint8_t  num_circuits;  // Number of active circuits/channels
    uint8_t  firmware_ver;  // Firmware version (major.minor packed)
    uint8_t  signal_rssi;   // Signal strength (negative, -128..0)
    uint8_t  flags;          // Bit flags: bit0=error, bit1=calibrated, bit2=sd_card
} pp_heartbeat_payload_t;

// ─── Circuit Data Payload ──────────────────────────────────────────

#define PP_CIRCUITS_MAX  16

typedef struct __attribute__((packed)) {
    uint16_t voltage_mv;     // Mains voltage in millivolts
    uint16_t frequency_cph;  // Frequency in centi-Hz (e.g., 5000 = 50.00 Hz)
    uint8_t  num_active;     // Number of active circuits in this packet
    uint8_t  circuit_mask;  // Bitmask of which circuits are included (0-15 → bits)
} pp_circuit_data_header_t;

typedef struct __attribute__((packed)) {
    uint8_t  circuit_id;     // Circuit number (0-15)
    uint16_t current_ma;     // RMS current in milliamps
    uint16_t power_w;        // Real power in watts
    int16_t  power_factor;   // Power factor × 10000 (e.g., 9500 = 0.95, negative = inductive)
    uint16_t energy_wh;      // Cumulative energy in watt-hours (wraps at 65535)
} pp_circuit_reading_t;

// Total circuit data payload: 5 bytes header + 8 bytes × num_active readings
// For 16 circuits: 5 + 128 = 133 bytes (fits in single frame)

// ─── Arc Fault Alert Payload ──────────────────────────────────────

typedef struct __attribute__((packed)) {
    uint8_t  circuit_id;         // Circuit where arc was detected (0-15)
    uint8_t  confidence_pct;     // Confidence 0-100
    uint8_t  arc_type;           // 0=series, 1=parallel, 2=glowing contact
    uint32_t timestamp_unix;     // Unix timestamp of detection
    uint16_t duration_ms;        // Duration of arc burst in ms
    uint8_t  severity;           // 1=low, 2=medium, 3=high, 4=critical
} pp_arc_fault_payload_t;

// ─── Appliance Data Payload ───────────────────────────────────────

typedef struct __attribute__((packed)) {
    uint8_t  tag_id;            // Appliance tag ID
    uint16_t voltage_mv;        // Voltage in millivolts
    uint16_t current_ma;        // Current in milliamps
    uint16_t power_w;           // Real power in watts
    int16_t  power_factor;      // Power factor × 10000
    uint32_t energy_wh;         // Cumulative energy in watt-hours
    uint8_t  relay_state;       // 0=off, 1=on
    uint8_t  temperature_c;     // Internal temperature (°C)
} pp_appliance_data_payload_t;

// ─── Appliance Command Payload ────────────────────────────────────

typedef struct __attribute__((packed)) {
    uint8_t  tag_id;            // Target appliance tag ID
    uint8_t  relay_cmd;         // 0=off, 1=on, 2=toggle, 3=schedule
    uint8_t  schedule_type;     // 0=none, 1=on_at, 2=off_at, 3=on_duration
    uint32_t schedule_time;     // Unix timestamp for schedule (if applicable)
    uint16_t duration_min;      // Duration in minutes (if applicable)
} pp_appliance_cmd_payload_t;

// ─── Solar Data Payload ───────────────────────────────────────────

typedef struct __attribute__((packed)) {
    uint16_t pv_voltage_mv;     // Solar panel voltage in millivolts
    uint16_t pv_current_ma;     // Solar panel current in milliamps
    uint16_t pv_power_w;        // Solar power in watts
    uint16_t batt_voltage_mv;   // Battery voltage in millivolts
    uint16_t load_current_ma;   // Load current in milliamps
    uint16_t load_power_w;      // Load power in watts
    uint8_t  soc_pct;           // Battery state of charge (0-100%)
    uint8_t  charge_mode;       // 0=standby, 1=buck, 2=float, 3=discharge
    uint8_t  mppt_duty_pct;     // MPPT duty cycle (0-100%)
    int8_t   heatsink_temp_c;   // Heatsink temperature °C
    uint8_t  fan_speed_pct;      // Fan speed (0-100%)
    uint16_t energy_produced_wh;// Cumulative solar energy produced (Wh)
    uint16_t energy_consumed_wh;// Cumulative load energy consumed (Wh)
} pp_solar_data_payload_t;

// ─── Solar Command Payload ────────────────────────────────────────

typedef struct __attribute__((packed)) {
    uint8_t  target_duty_pct;   // Target MPPT duty cycle (0-100, 255=auto)
    uint8_t  mode_override;     // 0=auto, 1=forced_buck, 2=forced_float, 3=forced_off
    uint8_t  emergency;          // 0=normal, 1=emergency_shutdown
} pp_solar_cmd_payload_t;

// ─── Calibration Payload ──────────────────────────────────────────

typedef struct __attribute__((packed)) {
    uint16_t node_addr;         // Target node address
    uint8_t  cal_type;          // 0=CT_zero_offset, 1=CT_gain, 2=voltage_offset, 3=voltage_gain
    uint16_t param_id;          // Parameter ID
    int32_t  value;             // Calibration value (scaled)
} pp_calibration_payload_t;

// ─── OTA Update Payload ───────────────────────────────────────────

#define PP_OTA_CHUNK_SIZE   128

typedef struct __attribute__((packed)) {
    uint16_t target_node;       // Target node address
    uint16_t total_chunks;      // Total number of chunks
    uint16_t chunk_index;       // This chunk's index (0-based)
    uint32_t firmware_crc32;    // CRC32 of entire firmware
    uint8_t  data[PP_OTA_CHUNK_SIZE]; // Firmware data
    uint8_t  data_len;          // Actual data length in this chunk
} pp_ota_payload_t;

// ─── Overload Alert Payload ───────────────────────────────────────

typedef struct __attribute__((packed)) {
    uint8_t  circuit_id;        // Overloaded circuit (0-15)
    uint16_t current_ma;        // Measured current
    uint16_t threshold_ma;      // Configured threshold
    uint8_t  overload_pct;      // Current as percentage of threshold (0-255 = 0-200%)
    uint32_t timestamp_unix;    // Unix timestamp
} pp_overload_payload_t;

// ─── Time Sync Payload ────────────────────────────────────────────

typedef struct __attribute__((packed)) {
    uint32_t unix_time;         // Current Unix timestamp
    uint16_t timezone_offset;   // Timezone offset in minutes from UTC
} pp_time_sync_payload_t;

// ─── ACK Payload ──────────────────────────────────────────────────

typedef struct __attribute__((packed)) {
    uint16_t acked_seq;         // Sequence number being acknowledged
    uint8_t  status;            // 0=OK, 1=ERROR, 2=BUSY, 3=UNKNOWN_MSG
} pp_ack_payload_t;

// ─── Functions ─────────────────────────────────────────────────────

/**
 * Compute CRC16-CCITT over a byte buffer.
 * Used for frame integrity verification.
 */
uint16_t pp_crc16_ccitt(const uint8_t *data, uint16_t len);

/**
 * Build a PowerPulse frame ready for transmission.
 * Returns total frame length (header + payload + CRC).
 * Buffer must be at least PP_MAX_FRAME bytes.
 */
uint16_t pp_frame_build(uint16_t src, uint16_t dst, pp_msg_type_t type,
                        uint16_t seq, const uint8_t *payload, uint8_t payload_len,
                        uint8_t *out_buf, uint16_t out_buf_size);

/**
 * Parse a received PowerPulse frame.
 * Returns 0 on success, negative on error.
 * Fills in header fields and sets *payload to point into the buffer.
 */
int pp_frame_parse(const uint8_t *frame, uint16_t frame_len,
                   pp_frame_header_t *header, const uint8_t **payload,
                   uint16_t *payload_len);

/**
 * Get human-readable string for message type.
 */
const char *pp_msg_type_str(pp_msg_type_t type);

/**
 * Get human-readable string for node type.
 */
const char *pp_node_type_str(pp_node_type_t type);

#endif // POWERPULSE_PROTOCOL_H