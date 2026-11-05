#!/usr/bin/env python3
"""Guided flow calibration math; requires a measured reference volume."""
import argparse
p = argparse.ArgumentParser()
p.add_argument('--reference-litres', type=float, required=True)
p.add_argument('--sensor-litres', type=float, required=True)
a = p.parse_args()
if a.reference_litres <= 0 or a.sensor_litres <= 0:
    p.error('volumes must be positive')
print(f"scale_factor={a.reference_litres/a.sensor_litres:.6f}")
print('Record temperature, pipe material, and test conditions; this does not certify accuracy.')
