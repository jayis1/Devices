#!/usr/bin/env python3
"""Manual sensor calibration worksheet helper; does not alter node calibration."""
import argparse
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--reference-c', type=float, required=True)
p.add_argument('--sensor-c', type=float, required=True)
a = p.parse_args()
print(f"Observed offset (reference - sensor): {a.reference_c-a.sensor_c:+.2f} C")
print("Record traceable reference, stabilization time, and operator; verify across range.")
