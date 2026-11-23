#include <stdint.h>
#include <stdbool.h>

/* Platform-neutral event logic; Zephyr GPIO/BLE adapter is not yet supplied. */
typedef enum { QH_NEUTRAL=0, QH_QUIET=1, QH_OVERRIDE=2 } qh_mode_t;
static qh_mode_t mode = QH_NEUTRAL;
static uint32_t override_until_ms;
void qh_button_event(bool quiet_pressed, bool override_pressed, uint32_t now_ms) {
    if (override_pressed) { mode = QH_OVERRIDE; override_until_ms = now_ms + 3600000u; }
    else if (quiet_pressed) mode = mode == QH_QUIET ? QH_NEUTRAL : QH_QUIET;
    if (mode == QH_OVERRIDE && (int32_t)(now_ms - override_until_ms) >= 0)
        mode = QH_NEUTRAL;
}
qh_mode_t qh_current_mode(void) { return mode; }
