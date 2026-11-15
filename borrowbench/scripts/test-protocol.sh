#!/bin/sh
set -eu
printf '%s\n' 'Firmware examples are host-testable only; ESP-IDF hardware drivers are not included.'
cc -std=c11 -Wall -Wextra -Werror firmware/common/protocol_test.c -o /tmp/borrowbench-protocol-test
/tmp/borrowbench-protocol-test
printf '%s\n' 'Portable protocol bounds test passed.'
