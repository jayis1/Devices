#include <stdbool.h>
#include <stdint.h>
#define MAX_TEMP_C 65
static bool relay=false;
void dock_tick(bool arm, bool off, int temp_c, bool ct_ok, uint32_t elapsed_s) { if(!arm || off || temp_c>MAX_TEMP_C || !ct_ok || elapsed_s>900) relay=false; gpio_set_relay(relay); }
bool dock_start(uint16_t seconds) { if(!gpio_arm() || gpio_off() || seconds==0 || seconds>900) return false; relay=true; return true; }
