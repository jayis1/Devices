/**
 * BME280 temp/humidity sensor for ambient spectral correction.
 * License: MIT
 */

#include "bme280.h"

static bool g_initialized = false;

void bme280_init(void)
{
    /* Production: I²C init on GPIO41(SDA)/GPIO42(SCL), @100kHz
     * Read chip ID (0x60), configure oversampling, mode */
    g_initialized = true;
}

void bme280_read(float *temp_c, float *humidity_pct)
{
    if (!g_initialized || temp_c == NULL || humidity_pct == NULL) return;
    /* Production: read registers 0xFA-0xFE, apply compensation formulas */
    *temp_c = 22.0f;
    *humidity_pct = 45.0f;
}