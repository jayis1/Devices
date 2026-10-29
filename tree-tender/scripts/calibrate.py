#!/usr/bin/env python3
"""Record soil-probe dry/wet ADC endpoints; writes local calibration JSON."""
import json, sys
if len(sys.argv) != 3:
    raise SystemExit('usage: calibrate.py DRY_ADC WET_ADC')
dry, wet = map(int, sys.argv[1:])
if dry == wet or min(dry, wet) < 0:
    raise SystemExit('endpoints must be distinct nonnegative ADC readings')
print(json.dumps({'dry_adc':dry,'wet_adc':wet,'note':'site-specific; validate against gravimetric samples'}))
