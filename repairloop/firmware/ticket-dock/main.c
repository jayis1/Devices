#include <stdint.h>
/* Gateway owns durable ticket allocation; dock only requests and displays result. */
typedef enum { DOCK_IDLE, DOCK_WAITING, DOCK_CONFIRMED, DOCK_OFFLINE } dock_state;
static dock_state state = DOCK_IDLE;
void dock_request_started(int link_up) { state = link_up ? DOCK_WAITING : DOCK_OFFLINE; }
void dock_ticket_committed(void) { if (state == DOCK_WAITING) state = DOCK_CONFIRMED; }
dock_state dock_state_read(void) { return state; }
