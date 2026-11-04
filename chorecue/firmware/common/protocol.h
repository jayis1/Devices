#include <stdint.h>
#include <stddef.h>

typedef enum { CC_TASK_DONE=1, CC_SUPPLY_G=2, CC_AVAILABILITY=3 } cc_kind;
typedef struct { uint32_t seq; int32_t value; cc_kind kind; } cc_event;

/* Validate a decoded event. Transport/parser must enforce payload <=192 bytes,
 * node enrollment, JSON types, and authenticated identity before calling. */
int cc_validate(cc_event event);
