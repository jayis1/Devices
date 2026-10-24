#!/usr/bin/env python3
"""Local-only prototype calibration checklist; does not command or modify hardware."""
import argparse

p = argparse.ArgumentParser()
p.add_argument('--node', choices=['shelf-scale', 'fridge-sentinel'], required=True)
p.add_argument('--reference', help='operator-recorded reference value and unit')
a = p.parse_args()
if not a.reference:
    p.error('record the traceable reference value explicitly; no default calibration is safe')
print(f'Node={a.node}; reference={a.reference}; record operator, date, device serial, offset, and independent verification in site log.')
