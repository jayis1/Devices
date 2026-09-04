#include <stdint.h>
#include <stdio.h>
#include "../common/protocol.h"

int main(void) {
    wsync_weather_t wx = {
        .rain_mm_x10 = 124,
        .soil_shallow_pct_x100 = 7110,
        .soil_mid_pct_x100 = 5840,
        .soil_deep_pct_x100 = 4280,
        .pressure_hpa_x10 = 9952,
        .temp_c_x100 = 1725,
        .freeze_risk_pct = 2,
        .dry_spell_days = 0,
    };
    printf("weather rain=%.1fmm soil=%u/%u/%u pressure=%.1fhPa freeze=%u\n",
           wx.rain_mm_x10 / 10.0,
           wx.soil_shallow_pct_x100,
           wx.soil_mid_pct_x100,
           wx.soil_deep_pct_x100,
           wx.pressure_hpa_x10 / 10.0,
           wx.freeze_risk_pct);
    return 0;
}
