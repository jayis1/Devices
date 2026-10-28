# QuietPath firmware

Portable protocol reference only. Sentinel application requires ESP-IDF, an I2S driver, SX1262 driver, clock provisioning, calibrated A-weighting DSP and board-specific build configuration. This is not complete node firmware. Run host tests: `cc -std=c99 -Wall -Wextra -Werror -Icommon common/quietpath.c common/test_quietpath.c -o /tmp/qp-test && /tmp/qp-test`.
