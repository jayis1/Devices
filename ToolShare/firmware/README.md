# Firmware status

Portable, host-tested codec is in `common/`. Node `main.c` files are SDK integration placeholders and do not implement BLE, sensor drivers, calibration, or event buffering yet. Compile the codec test with `gcc -std=c11 -Wall -Wextra -Werror -Icommon tests/test_protocol.c common/protocol.c -o /tmp/toolshare-test`.
