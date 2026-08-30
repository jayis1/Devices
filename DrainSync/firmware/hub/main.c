#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include "../common/protocol.h"

static uint16_t g_seq = 1;
static uint32_t g_nonce = 0xD5123401u;

static void radio_send(const ds_frame_t *frame) {
    printf("hub tx type=%u dst=%u seq=%u crc=%u\n", frame->msg_type, frame->dst, frame->seq, frame->crc);
}

static void send_valve_command(uint16_t actuator_id, ds_command_t command) {
    ds_frame_t frame;
    ds_init_frame(&frame, DS_MSG_COMMAND, 0x1000u, actuator_id, g_nonce, g_seq++);
    frame.payload[0] = (uint8_t)command;
    frame.crc = ds_crc16_ccitt((const uint8_t *)&frame, 11u);
    radio_send(&frame);
}

int main(void) {
    bool backup_risk_high = true;
    if (backup_risk_high) {
        send_valve_command(0x1401u, DS_CMD_VALVE_CLOSE);
    }
    return 0;
}
