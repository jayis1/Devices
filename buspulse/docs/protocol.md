# BusPulse protocol v1

Author: jayis1. Application frame: 18 bytes, big-endian multi-byte values. Offsets: 0 magic `0xB5`; 1 version `1`; 2 type `1` telemetry; 3 node ID; 4–5 sequence uint16; 6–9 Unix seconds uint32; 10 flags (bit 0 sensor valid); 11–12 measured value uint16 millivolts; 13–14 signed temperature centi-degrees C (`0x7fff` unavailable); 15 reserved zero; 16–17 CRC-16/CCITT-FALSE over bytes 0–15 (init `0xffff`, polynomial `0x1021`, xorout zero).

Reject wrong size, magic, version, type, or CRC. Sequence wraps modulo 65536; gateway deduplicates node+sequence and expires a node after three expected intervals. PHY CRC is additional. This prototype has no cryptographic authentication, provisioning, key rotation, or replay protection; frames are forgeable. Never use for control or safety decisions. Production requires authenticated encryption and managed keys.
