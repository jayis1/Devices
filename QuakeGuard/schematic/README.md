# QuakeGuard Schematics

Each node has its own KiCad schematic file:

| Node | File | SoC |
|------|------|-----|
| Hub | `hub/quakeguard_hub.kicad_sch` | ESP32-S3-WROOM-1-N16R8 |
| Floor Node | `floor-node/quakeguard_floor.kicad_sch` | ESP32-S3-WROOM-1-N8R2 |
| Shutoff Controller | `shutoff-controller/quakeguard_shutoff.kicad_sch` | ESP32-C3-WROOM-02 |
| Structural Tag | `structural-tag/quakeguard_struct.kicad_sch` | RP2040 |

## Design Notes

- All nodes share the CC1101 868 MHz Sub-GHz radio on SPI.
- Hub and Floor Nodes use ESP32-S3 (240 MHz dual-core, tflite-micro capable).
- Shutoff Controller uses ESP32-C3 (lower power, sufficient for motor control).
- Structural Tag uses RP2040 (ultra-low power, 12-month CR2032 life).
- Power: Hub/Floor/Shutoff have 18650 UPS; Structural Tag is battery-only.

Open in KiCad 7+. Symbol/footprint libraries are stock KiCad + manufacturer-provided (Espressif, Adafruit, Waveshare).