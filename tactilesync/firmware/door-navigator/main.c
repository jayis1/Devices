#include "../common/tactilesync_protocol.h"
#include <stdint.h>
extern int board_reed_open(void); extern void board_haptic_pattern(uint8_t); extern void board_send_door(uint8_t state);
int main(void) { int previous=-1; for (;;) { int now=board_reed_open(); if(now!=previous) { previous=now; board_send_door((uint8_t)now); if(now) board_haptic_pattern(3); } } }
