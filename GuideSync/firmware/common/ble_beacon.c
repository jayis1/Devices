/*
 * GuideSync — BLE Beacon Scanner (Implementation)
 * Scans for Nav Beacon BLE advertisements for indoor positioning (NavNet)
 *
 * Distance estimation uses log-distance path loss model:
 *   d = 10^((TxPower - RSSI) / (10 * n))
 * where n = 2.0 (free space) to 3.0 (indoor)
 */
#include "ble_beacon.h"
#include <math.h>

static const gs_beacon_scan_if_t *g_scan_if = NULL;

int gs_beacon_scanner_init(const gs_beacon_scan_if_t *scan_if)
{
    if (!scan_if) return -1;
    g_scan_if = scan_if;
    return 0;
}

/* Perform a BLE scan and collect beacon results */
int gs_beacon_scan(gs_beacon_scan_t *scan)
{
    if (!scan || !g_scan_if) return -1;

    memset(scan, 0, sizeof(*scan));

    /* Start scanning for 2 seconds */
    g_scan_if->scan_start();
    g_scan_if->delay_ms(2000);
    g_scan_if->scan_stop();

    /* Collect results */
    scan->count = (uint8_t)g_scan_if->scan_get_results(
        scan->results, GS_BEACON_MAX_PER_SCAN);

    return scan->count > 0 ? 0 : -1;
}

/* Estimate distance from RSSI (returns decimeters) */
uint8_t gs_beacon_rssi_to_dm(int8_t rssi, uint8_t tx_power)
{
    /* Log-distance path loss: d(m) = 10^((tx_power - rssi) / (10 * n))
     * n = 2.7 for typical indoor environment
     * Returns distance in decimeters (0.1 m units)
     */
    if (rssi == 0 || tx_power == 0) return 255; /* Invalid */

    float exponent = ((float)tx_power - (float)rssi) / (10.0f * 2.7f);
    float dist_m = powf(10.0f, exponent);

    /* Clamp: 0.1 m to 30 m (255 dm cap) */
    if (dist_m < 0.1f) dist_m = 0.1f;
    if (dist_m > 30.0f) dist_m = 30.0f;

    return (uint8_t)(dist_m * 10.0f);
}

/* Get nearest beacon from scan results */
int gs_beacon_get_nearest(const gs_beacon_scan_t *scan, gs_beacon_result_t *nearest)
{
    if (!scan || !nearest || scan->count == 0) return -1;

    uint8_t min_dist_dm = 255;
    int min_idx = -1;

    for (uint8_t i = 0; i < scan->count; i++) {
        if (scan->results[i].distance_dm < min_dist_dm) {
            min_dist_dm = scan->results[i].distance_dm;
            min_idx = i;
        }
    }

    if (min_idx < 0) return -1;

    *nearest = scan->results[min_idx];
    return 0;
}