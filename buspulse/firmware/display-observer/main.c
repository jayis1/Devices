#include "../common/protocol.h"
/* Integration stub: board initialization, sensor drivers and radio are not implemented. */
int main(void){uint8_t frame[BP_FRAME_LEN];return bp_encode(1,0,0,0,0,0,frame);}
