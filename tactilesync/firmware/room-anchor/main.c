#include "../common/tactilesync_protocol.h"
#include <stdint.h>
extern uint16_t board_uwb_range_cm(void); extern void board_send_zone(uint16_t cm);
int main(void) { for (;;) { uint16_t range=board_uwb_range_cm(); if (range<3000u) board_send_zone(range); } }
