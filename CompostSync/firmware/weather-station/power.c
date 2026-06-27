/*
 * CompostSync Weather Station — Power Management
 * nRF52840 solar-powered
 * firmware/weather-station/power.c
 */
#include <stdint.h>
#include <stdbool.h>
#include "nrf.h"
#include "nrf_drv_saadc.h"
#include "nrf_log.h"

/* Solar charging state */
typedef struct {
    float battery_mah;
    float capacity_mah;
    float voltage;
    uint8_t pct;
} power_t;

static power_t pwr = {
    .battery_mah = 2000.0f,
    .capacity_mah = 2000.0f,
    .voltage = 4.2f,
    .pct = 100,
};

/* Read battery voltage via SAADC */
uint8_t power_read_battery_pct(void)
{
    /* nRF52840: VBAT is on ADC channel 7 (AIN7/P0.31) with 2x divider
     * Actual voltage = ADC_reading / 1023 * 3.6 * 2 */
    nrf_saadc_value_t adc_val;
    nrf_saadc_channel_config_t ch_cfg = {
        .resistor_p = NRF_SAADC_RESISTOR_DISABLED,
        .resistor_n = NRF_SAADC_RESISTOR_DISABLED,
        .gain = NRF_SAADC_GAIN1_6,
        .reference = NRF_SAADC_REFERENCE_INTERNAL, /* 0.6V */
        .acq_time = NRF_SAADC_ACQTIME_10US,
        .mode = NRF_SAADC_MODE_SINGLE_ENDED,
        .pin_p = NRF_SAADC_INPUT_VDD,
        .pin_n = NRF_SAADC_INPUT_DISABLED,
    };
    /* Simplified: use VDD as input */
    nrf_drv_saadc_channel_init(1, &ch_cfg);
    nrf_drv_saadc_sample_convert(1, &adc_val);

    /* Convert to voltage: ADC 10-bit, gain 1/6, ref 0.6V
     * V = ADC / 1023 * 0.6 * 6 */
    pwr.voltage = (float)adc_val / 1023.0f * 0.6f * 6.0f;

    /* Li-ion: 4.2V = 100%, 3.0V = 0% */
    if (pwr.voltage >= 4.2f) pwr.pct = 100;
    else if (pwr.voltage <= 3.0f) pwr.pct = 0;
    else pwr.pct = (uint8_t)((pwr.voltage - 3.0f) / 1.2f * 100);

    NRF_LOG_INFO("Battery: %.2fV (%d%%)", pwr.voltage, pwr.pct);
    return pwr.pct;
}

/* Estimate solar charging based on time of day */
bool power_is_charging(void)
{
    /* Would use RTC time; simplified: always report based on ADC */
    return pwr.voltage < 4.15f; /* charging if not full */
}