#include "../common/protocol.h"
/* Board SDK sampling integration is intentionally not claimed. */
int main(void) { return BD_PROTOCOL_VERSION == 1u ? 0 : 1; }
