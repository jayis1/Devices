/**
 * UrbanHarvest - Shared Mesh Protocol Implementation
 * CRC, packet construction, and utility functions
 * Used by all nodes
 */

#include "mesh_protocol.h"

/**
 * mesh_unpack_soil_data - Unpack soil data from received packet payload
 */
void mesh_unpack_soil_data(const uint8_t *payload, soil_data_payload_t *out)
{
    out->moisture_pct     = payload[0];
    out->ec_x10_hi       = payload[1];
    out->ec_x10_lo       = payload[2];
    out->temp_c_offset    = payload[3];
    out->par_x10_hi      = payload[4];
    out->par_x10_lo      = payload[5];
    out->health_index     = payload[6];
    out->leaf_wet_pct     = payload[7];
    out->battery_x20     = payload[8];
    out->health_category  = payload[9];
    out->leaf_wet_h_x10_hi = payload[10];
    out->leaf_wet_h_x10_lo = payload[11];
}

/**
 * mesh_unpack_weather_data - Unpack weather data from received packet payload
 */
void mesh_unpack_weather_data(const uint8_t *payload, weather_data_payload_t *out)
{
    uint16_t idx = 0;
    out->temp_x10       = (int16_t)((payload[idx] << 8) | payload[idx+1]); idx += 2;
    out->rh_x10         = (uint16_t)((payload[idx] << 8) | payload[idx+1]); idx += 2;
    out->pressure_x10   = (uint16_t)((payload[idx] << 8) | payload[idx+1]); idx += 2;
    out->wind_x10       = (uint16_t)((payload[idx] << 8) | payload[idx+1]); idx += 2;
    out->wind_dir       = payload[idx]; idx++;
    out->rain_x100      = (uint16_t)((payload[idx] << 8) | payload[idx+1]); idx += 2;
    out->uv_x10         = (uint16_t)((payload[idx] << 8) | payload[idx+1]); idx += 2;
    out->light_lux      = (uint16_t)((payload[idx] << 8) | payload[idx+1]); idx += 2;
    out->solar_v_x20    = payload[idx]; idx++;
    out->bat_v_x20      = payload[idx]; idx++;
    out->bat_soc        = payload[idx]; idx++;
    out->pressure_trend = payload[idx]; idx++;
    out->rain_predicted  = payload[idx];
}

/**
 * mesh_unpack_growpod_status - Unpack grow pod status from payload
 */
void mesh_unpack_growpod_status(const uint8_t *payload, growpod_status_payload_t *out)
{
    uint16_t idx = 0;
    out->pump_running        = payload[idx++];
    out->nutrient_a_ml_x10   = (uint16_t)((payload[idx] << 8) | payload[idx+1]); idx += 2;
    out->nutrient_b_ml_x10   = (uint16_t)((payload[idx] << 8) | payload[idx+1]); idx += 2;
    out->ph_dose_ml_x10      = (uint16_t)((payload[idx] << 8) | payload[idx+1]); idx += 2;
    out->fan_speed_pct       = payload[idx++];
    out->heater_on           = payload[idx++];
    out->humidifier_on       = payload[idx++];
    out->light_on            = payload[idx++];
    out->red_pwm             = payload[idx++];
    out->blue_pwm            = payload[idx++];
    out->white_pwm           = payload[idx++];
    out->far_red_pwm         = payload[idx++];
    out->water_temp_c_offset  = payload[idx++];
    out->disease_class       = payload[idx++];
    out->disease_conf_pct    = payload[idx++];
}

/**
 * mesh_decode_soil_moisture - Convert payload moisture byte to percentage
 */
float mesh_decode_soil_moisture(uint8_t moisture_byte)
{
    return (float)moisture_byte;  /* Already 0-100% */
}

/**
 * mesh_decode_ec - Convert payload EC bytes to mS/cm
 */
float mesh_decode_ec(uint8_t hi, uint8_t lo)
{
    uint16_t raw = ((uint16_t)hi << 8) | lo;
    return (float)raw / 10.0f;
}

/**
 * mesh_decode_temp - Convert payload temp byte to °C
 */
float mesh_decode_temp(uint8_t temp_offset)
{
    return (float)temp_offset - 40.0f;
}

/**
 * mesh_decode_par - Convert payload PAR bytes to µmol/m²/s
 */
float mesh_decode_par(uint8_t hi, uint8_t lo)
{
    uint16_t raw = ((uint16_t)hi << 8) | lo;
    return (float)raw / 10.0f;
}

/**
 * mesh_decode_leaf_wet_hours - Convert payload leaf wetness bytes to hours
 */
float mesh_decode_leaf_wet_hours(uint8_t hi, uint8_t lo)
{
    uint16_t raw = ((uint16_t)hi << 8) | lo;
    return (float)raw / 10.0f;
}

/**
 * mesh_decode_battery_voltage - Convert payload battery byte to voltage
 */
float mesh_decode_battery_voltage(uint8_t bat_x20)
{
    return (float)bat_x20 / 20.0f;
}