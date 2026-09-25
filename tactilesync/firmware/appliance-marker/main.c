#include "../common/tactilesync_protocol.h"
#include <stdint.h>
extern int board_marker_pressed(void); extern void board_haptic_pattern(uint8_t); extern void board_send_appliance(uint8_t);
int main(void) { for (;;) if (board_marker_pressed()) { board_haptic_pattern(2); board_send_appliance(1); } }
