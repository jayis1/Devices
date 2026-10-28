# Firmware build notes
Host protocol code is portable C99. Sensor mains depend on board SDK adapters and are not standalone firmware images yet. Compile the protocol with `gcc -std=c99 -Wall -Wextra -Werror -Ifirmware/common -c firmware/common/protocol.c`.
