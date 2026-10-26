#!/bin/sh
set -eu
# Build using the installed Zephyr workspace; board target is installation-specific.
west build -b nrf52840dk_nrf52840 firmware/machine-sensor
