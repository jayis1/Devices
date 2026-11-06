#!/bin/sh
set -eu
cc -std=c11 -Wall -Wextra -Werror -c firmware/common/protocol.c -o /tmp/daylightsync-protocol.o
cc -std=c11 -Wall -Wextra -Werror -c firmware/window-sentinel/main.c -o /tmp/daylightsync-window.o
cc -std=c11 -Wall -Wextra -Werror -c firmware/shade-node/main.c -o /tmp/daylightsync-shade.o
cc -std=c11 -Wall -Wextra -Werror -c firmware/outdoor-node/main.c -o /tmp/daylightsync-outdoor.o
printf 'C reference modules compile successfully\n'
