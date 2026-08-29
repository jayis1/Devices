#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

typedef struct {
    const char *name;
    uint16_t watts;
    uint8_t priority;
    bool medical;
} load_t;

static bool should_shed(const load_t *load, uint16_t reserve_minutes) {
    if (load->medical) return false;
    if (reserve_minutes < 90 && load->priority >= 4) return true;
    if (reserve_minutes < 45 && load->priority >= 2) return true;
    return false;
}

int main(void) {
    load_t loads[] = {
        {"router", 18, 1, false},
        {"fridge", 145, 1, false},
        {"tv", 120, 5, false},
        {"cpap", 42, 0, true},
        {"lamp", 11, 4, false},
    };
    uint16_t reserve_minutes = 70;
    for (unsigned i = 0; i < sizeof(loads) / sizeof(loads[0]); ++i) {
        printf("load=%s watts=%u priority=%u shed=%u\n",
               loads[i].name,
               loads[i].watts,
               loads[i].priority,
               should_shed(&loads[i], reserve_minutes) ? 1u : 0u);
    }
    return 0;
}
