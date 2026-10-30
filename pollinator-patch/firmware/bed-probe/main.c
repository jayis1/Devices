#include <stdint.h>
/* Board integration hook only; no sensor or BLE driver included. */
#define PP_SOIL_ADC_GPIO 0
#define PP_SOIL_TEMP_GPIO 4
#define PP_BATTERY_ADC_GPIO 5
int main(void) { volatile uint16_t soil=0,battery_mv=0; volatile int16_t temp_centi=0; (void)soil;(void)battery_mv;(void)temp_centi; /* sample, frame, bonded-BLE send, sleep */ return PP_SOIL_ADC_GPIO==PP_BATTERY_ADC_GPIO; }
