#include "../common/protocol.h"
#include <stdbool.h>
// Audio is processed in RAM after physical mic-enable and discarded after intent/reject.
bool publish_intent(uint8_t intent, uint8_t zone, uint8_t confidence) { if(confidence<85) return false; hs_frame_t f={.version=1,.type=HS_TELEMETRY,.len=3}; f.payload[0]=intent; f.payload[1]=zone; f.payload[2]=confidence; return radio_send((uint8_t*)&f, 18+f.len+HS_TAG_LEN); }
