#ifndef MB_NODE_KIND
#define MB_NODE_KIND 1
#endif
#include <stdint.h>
/* SDK adapter TODO: sample the node-specific sensor and send via ESP-NOW. */
int main(void) { uint32_t sequence=0; (void)sequence; return MB_NODE_KIND==0; }
