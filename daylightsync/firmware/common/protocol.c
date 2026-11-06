#include <stdint.h>
#include <stddef.h>

#define DS_PROTOCOL_VERSION 1
#define DS_MAX_PAYLOAD 256

typedef enum { DS_ROOM=1, DS_OUTDOOR=2, DS_SHADE=3, DS_SET_SHADE=4 } ds_kind;
typedef struct { uint32_t seq; uint32_t timestamp; uint8_t kind; uint8_t position_pct; } ds_message;

/* Bounded command guard; product firmware must add authenticated transport,
 * hardware limit switch inputs, watchdog, and motor-current supervision. */
int ds_command_valid(const ds_message *m, uint32_t now) {
    if (!m || m->kind != DS_SET_SHADE || m->position_pct > 100) return 0;
    if (m->timestamp > now + 5 || now > m->timestamp + 10) return 0;
    return 1;
}
