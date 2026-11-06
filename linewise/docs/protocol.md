# LineWise protocol v1

Fixed 16-byte LE frame: byte0 version=1; byte1 kind (1 load grams, 2 rain tips, 3 wind mm/s); bytes2-3 node ID; bytes4-7 uint32 sequence; bytes8-11 signed int32 value; bytes12-13 reserved zero; bytes14-15 CRC-16/CCITT-FALSE over bytes0-13, little-endian. Bounds: exact 16 bytes, kinds 1..3. Sequence increases per boot but reboot persistence is not yet designed; gateway must treat reboot as a new epoch. CRC detects accidental corruption only, not forgery. BLE advertisements are unauthenticated. No actuation commands exist.
