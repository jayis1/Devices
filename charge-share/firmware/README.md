# Prototype firmware status: sensor acquisition and radio integration are stubs.

Host-test codec: `cc -std=c11 -Wall -Wextra -Werror -Ifirmware/common firmware/common/protocol.c firmware/common/test_protocol.c -o /tmp/charges-test && /tmp/charges-test`. Board-specific ESP-IDF projects are not included.
