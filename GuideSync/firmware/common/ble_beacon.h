/*
 * GuideSync — BLE Beacon Scanner (Header)
 * Scans for Nav Beacon BLE advertisements for indoor positioning (NavNet)
 */
#ifndef GUIDESYNC_BLE_BEACON_H
#define GUIDESYNC_BLE_BEACON_H

#include <stdint.h>
#include <stddef.h>

#define GS_BEACON_MAX_PER_SCAN   32

/* Beacon scan result */
typedef struct {
    uint16_t uuid_short;    /* Short UUID (last 2 bytes of beacon UUID) */
    int8_t   rssi;          /* Received signal strength (dBm) */
    uint8_t  distance_dm;   /* Estimated distance (decimeters) */
    uint8_t  tx_power;      /* Beacon TX power (for distance estimation) */
} gs_beacon_result_t;

/* Beacon scan context */
typedef struct {
    gs_beacon_result_t results[GS_BEACON_MAX_PER_SCAN];
    uint8_t count;
} gs_beacon_scan_t;

/* Platform BLE scan interface */
typedef struct {
    void (*scan_start)(void);
    void (*scan_stop)(void);
    int  (*scan_get_results)(gs_beacon_result_t *results, uint8_t max_count);
    void (*delay_ms)(uint32_t ms);
} gs_beacon_scan_if_t;

/* Initialize beacon scanner */
int gs_beacon_scanner_init(const gs_beacon_scan_if_t *scan_if);

/* Perform a BLE scan and collect beacon results (2-second scan window) */
int gs_beacon_scan(gs_beacon_scan_t *scan);

/* Estimate distance from RSSI using log-distance path loss model */
uint8_t gs_beacon_rssi_to_dm(int8_t rssi, uint8_t tx_power);

/* Get nearest beacon from scan results */
int gs_beacon_get_nearest(const gs_beacon_scan_t *scan, gs_beacon_result_t *nearest);

#endif /* GUIDESYNC_BLE_BEACON_H */