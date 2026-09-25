#include "../common/tactilesync_protocol.h"
#include <stdint.h>
extern int board_ble_receive(ts_frame_t *out); extern void board_haptic_pattern(uint8_t pattern); extern int board_ack_pressed(void); extern void board_send_ack(uint32_t sequence);
int main(void) { ts_frame_t frame; uint32_t last=0; const uint8_t key[16]={0}; for (;;) { if (board_ble_receive(&frame) && ts_frame_validate(&frame,last,key)) { last=frame.sequence; board_haptic_pattern(frame.payload[0]); } if (board_ack_pressed()) board_send_ack(last); } }
