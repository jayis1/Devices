#!/usr/bin/env python3
"""Record operator lux reference; no sensor interface is implemented."""
import argparse
p=argparse.ArgumentParser(); p.add_argument("reference_lux",type=float); p.add_argument("measured_lux",type=float); a=p.parse_args()
if a.reference_lux<0 or a.measured_lux<0: p.error("lux values must be nonnegative")
print(f"reference={a.reference_lux:.2f} measured={a.measured_lux:.2f} offset={a.reference_lux-a.measured_lux:.2f} lx")
