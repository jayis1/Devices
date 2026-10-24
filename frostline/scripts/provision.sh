#!/bin/sh
set -eu
: "${FROSTLINE_SITE_ID:=SET_SITE_ID}"
: "${FROSTLINE_REGION:=SET_REGION}"
printf 'Provisioning checklist only: site=%s region=%s\n' "$FROSTLINE_SITE_ID" "$FROSTLINE_REGION"
printf 'Generate unique device credentials out-of-band; no secrets are stored by this script.\n'
