#!/bin/sh
set -eu
printf '%s\n' 'Prototype only. Set LIFTLINK_SITE and broker credentials in deployment environment.'
test -n "${LIFTLINK_SITE:-}" || { printf '%s\n' 'LIFTLINK_SITE is required' >&2; exit 2; }
printf 'Site configured: %s\n' "$LIFTLINK_SITE"
