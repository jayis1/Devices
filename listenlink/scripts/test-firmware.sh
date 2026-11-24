#!/bin/sh
set -eu
cc -std=c11 -Wall -Wextra -Werror firmware/common/protocol.c firmware/common/test_protocol.c -o /tmp/listenlink-test
/tmp/listenlink-test
