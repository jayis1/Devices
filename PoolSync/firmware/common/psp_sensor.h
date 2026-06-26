/**
 * @file psp_sensor.h
 * @brief PoolSync sensor abstraction layer — unified API for all sensors
 */

#ifndef PSP_SENSOR_H
#define PSP_SENSOR_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * CHEMISTRY SENSOR INTERFACE
 * ============================================================ */

typedef struct {
    float    ph;              /* pH 0.00–14.00 */
    float    orp_mv;          /* ORP mV */
    float    free_cl_ppm;    /* Free chlorine ppm */
    float    temperature_c;   /* Water temp °C */
    float    conductivity_us; /* Conductivity µS/cm */
    float    turbidity_ntu;   /* Turbidity NTU */
    uint64_t timestamp_ms;    /* Measurement time */
    uint8_t  valid_mask;      /* Bitmask: which readings are valid */
} psp_chem_reading_t;

/* Valid mask bits */
#define PSP_CHEM_VALID_PH           (1 << 0)
#define PSP_CHEM_VALID_ORP          (1 << 1)
#define PSP_CHEM_VALID_CL          (1 << 2)
#define PSP_CHEM_VALID_TEMP        (1 << 3)
#define PSP_CHEM_VALID_CONDUCTIVITY (1 << 4)
#define PSP_CHEM_VALID_TURBIDITY   (1 << 5)

/* Chemistry limits for alarm triggering */
#define PSP_PH_MIN        6.8f
#define PSP_PH_MAX        8.2f
#define PSP_PH_IDEAL_MIN  7.2f
#define PSP_PH_IDEAL_MAX  7.6f
#define PSP_ORP_MIN       650.0f   /* mV — minimum for sanitation */
#define PSP_CL_MIN        1.0f     /* ppm */
#define PSP_CL_MAX        5.0f     /* ppm */
#define PSP_CL_IDEAL_MIN  2.0f
#define PSP_CL_IDEAL_MAX  4.0f

/**
 * Initialize chemistry sensor hardware (ISFET, ORP, amperometric, etc.)
 */
int psp_chem_init(void);

/**
 * Read all chemistry sensors
 * Sensors are excited sequentially: pH → ORP → Cl → temp → conductivity → turbidity
 * Each sensor is powered on for stabilization, then read, then powered down
 */
int psp_chem_read(psp_chem_reading_t *reading);

/**
 * Perform 2-point pH calibration with buffer solutions
 * @param ph4_reading  Raw ADC reading in pH 4.0 buffer
 * @param ph7_reading  Raw ADC reading in pH 7.0 buffer
 */
int psp_chem_calibrate_ph(uint16_t ph4_reading, uint16_t ph7_reading);

/* ============================================================
 * CAMERA SENSOR INTERFACE
 * ============================================================ */

typedef struct {
    float    clarity_score;   /* 0.0–1.0 */
    float    green_channel;   /* Normalized green intensity */
    float    turbidity_ntu;   /* Estimated turbidity */
    uint8_t  algae_risk;      /* 0=none, 1=low, 2=medium, 3=high */
    uint16_t image_hash;      /* CRC16 of captured image */
} psp_clarity_reading_t;

/**
 * Initialize camera (IMX477 + IR-cut + TSL2591)
 */
int psp_camera_init(void);

/**
 * Capture image and compute on-device clarity score
 * Uses histogram analysis + green channel detection
 */
int psp_camera_capture_clarity(psp_clarity_reading_t *reading);

/**
 * Detect motion (PIR trigger)
 * @return true if motion detected (person near pool)
 */
bool psp_camera_motion_detected(void);

/* ============================================================
 * EQUIPMENT SENSOR INTERFACE
 * ============================================================ */

typedef struct {
    bool     pump_on;         /* Pump relay state */
    bool     heater_on;       /* Heater relay state */
    bool     pool_light_on;   /* Pool light relay state */
    bool     spa_light_on;   /* Spa light relay state */
    bool     valve1_on;       /* Valve 1 relay state */
    bool     valve2_on;       /* Valve 2 relay state */
    bool     blower_on;       /* Spa blower relay state */
    bool     spare_on;        /* Spare relay state */
    float    flow_lpm;        /* Flow rate L/min (YF-S201) */
    float    pressure_kpa;    /* Filter pressure kPa (MPX5010DP) */
    float    current_a;       /* AC current A (ACS712) */
    uint8_t  pump_dosing;    /* 0=idle, 1=dosing acid, 2=dosing chlorine, 3=dosing clarifier */
} psp_equip_reading_t;

/**
 * Initialize equipment controller hardware
 */
int psp_equip_init(void);

/**
 * Read all equipment sensors
 */
int psp_equip_read(psp_equip_reading_t *reading);

/**
 * Set relay state
 * @param device_id  0=pump, 1=heater, 2=pool_light, 3=spa_light, 4=valve1, 5=valve2, 6=blower, 7=spare
 * @param on         true=on, false=off
 */
int psp_equip_set_relay(uint8_t device_id, bool on);

/**
 * Run peristaltic pump for dosing
 * @param pump_id    0=acid, 1=chlorine, 2=clarifier
 * @param volume_ml  Volume in mL to dispense
 * @param timeout_s  Maximum run time (safety)
 * @return Actual volume dispensed (from flow sensor verification)
 */
float psp_equip_dose(uint8_t pump_id, float volume_ml, uint16_t timeout_s);

/**
 * Check for GFCI fault
 * @return true if ground fault detected
 */
bool psp_equip_gfci_fault(void);

/**
 * Check for entrapment condition (pressure differential)
 * @return true if suction entrapment risk detected
 */
bool psp_equip_entrapment_detected(void);

/* ============================================================
 * SOLAR MONITOR INTERFACE
 * ============================================================ */

typedef struct {
    float irradiance_wm2;    /* Solar irradiance W/m² */
    float panel_current_a;   /* Solar pump current */
    float panel_temp_c;      /* Panel temp °C */
    float roof_temp_c;       /* Roof temp °C */
} psp_solar_reading_t;

int psp_solar_init(void);
int psp_solar_read(psp_solar_reading_t *reading);

/* ============================================================
 * COMMON UTILITIES
 * ============================================================ */

/**
 * Get UTC timestamp (from RTC or NTP-synced source)
 */
uint32_t psp_get_timestamp(void);

/**
 * Get battery voltage in mV
 */
uint16_t psp_get_battery_mv(void);

/**
 * Get radio RSSI from last received packet
 */
int8_t psp_get_rssi(void);

#ifdef __cplusplus
}
#endif

#endif /* PSP_SENSOR_H */