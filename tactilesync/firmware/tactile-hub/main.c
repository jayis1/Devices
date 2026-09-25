#include "../common/tactilesync_protocol.h"
#include <stdint.h>
extern int board_mesh_receive(ts_frame_t *); extern void board_publish_local(const ts_frame_t *); extern void board_publish_mqtt_tls(const ts_frame_t *);
int main(void) { ts_frame_t f; uint32_t last[256]={0}; const uint8_t key[16]={0}; for (;;) if (board_mesh_receive(&f) && ts_frame_validate(&f,last[f.source_id],key)) { last[f.source_id]=f.sequence; board_publish_local(&f); board_publish_mqtt_tls(&f); } }
