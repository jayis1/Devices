#!/bin/sh
set -eu
cc -std=c11 -Wall -Wextra -Werror -I../ -o /tmp/liftlink-test test_protocol.c ../protocol.c
/tmp/liftlink-test
