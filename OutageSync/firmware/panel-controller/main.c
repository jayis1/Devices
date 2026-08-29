#include <stdint.h>
#include <stdio.h>
#include "../common/protocol.h"

static uint8_t classify_grid(double vrms, double freq_hz, double thd_pct) {
    if (vrms < 205.0 || vrms > 250.0 || freq_hz < 49.5 || freq_hz > 60.5) return 2;
    if (thd_pct > 4.0 || vrms < 218.0 || vrms > 242.0) return 1;
    return 0;
}

int main(void) {
    double voltage_trace[] = {231.0, 227.5, 221.0, 208.4, 0.0};
    double freq_trace[] = {59.99, 59.97, 59.92, 59.61, 0.0};
    double thd_trace[] = {2.1, 3.2, 4.4, 6.7, 0.0};
    for (int i = 0; i < 5; ++i) {
        uint8_t cls = classify_grid(voltage_trace[i], freq_trace[i], thd_trace[i]);
        printf("sample=%d vrms=%.1f freq=%.2f thd=%.1f class=%u\n", i, voltage_trace[i], freq_trace[i], thd_trace[i], cls);
    }
    return 0;
}
