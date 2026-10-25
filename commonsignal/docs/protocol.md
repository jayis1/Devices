# Protocol v1

Wire layout exactly as `firmware/common/protocol.h`: offsets 0-1 sync A5 5A; 2 version=1; 3 type (1 heartbeat, 2 route, 3 equipment, 4 climate, 5 fault); 4-5 node ID uint16 little-endian; 6-7 sequence uint16 LE; 8-11 uptime seconds uint32 LE; 12 payload length; payload 0..96 bytes; trailing CRC16-CCITT big-endian? The implementation stores CRC little-endian. CRC covers header and payload, initial FFFF polynomial 1021. Reject invalid length/version/sync/CRC. Duplicate and out-of-order sequence handling belongs to receiver state. CRC is not authentication.
