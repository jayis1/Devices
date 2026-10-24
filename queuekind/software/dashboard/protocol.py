import struct

MAGIC = b"\x51\x4b"

def crc16(data: bytes) -> int:
    crc = 0xFFFF
    for byte in data:
        crc ^= byte << 8
        for _ in range(8):
            crc = ((crc << 1) ^ 0x1021) & 0xFFFF if crc & 0x8000 else (crc << 1) & 0xFFFF
    return crc

def decode(frame: bytes) -> dict:
    if len(frame) < 10 or len(frame) > 18 or frame[:2] != MAGIC or frame[2] != 1:
        raise ValueError("invalid header/length")
    length = frame[7]
    if length > 8 or len(frame) != 10 + length or frame[3] not in (1, 2, 3, 4):
        raise ValueError("invalid type/payload length")
    expected, = struct.unpack_from("<H", frame, len(frame) - 2)
    if crc16(frame[:-2]) != expected:
        raise ValueError("CRC mismatch")
    seq, = struct.unpack_from("<H", frame, 5)
    return {"type": frame[3], "node": frame[4], "seq": seq, "payload": frame[8:-2]}

if __name__ == "__main__":
    # Same semantic vector exercised by firmware/common/test_protocol.c
    body = MAGIC + bytes((1, 1, 1)) + struct.pack("<H", 1) + bytes((0,))
    frame = body + struct.pack("<H", crc16(body))
    assert decode(frame) == {"type": 1, "node": 1, "seq": 1, "payload": b""}
    print("protocol vector passed", frame.hex())
