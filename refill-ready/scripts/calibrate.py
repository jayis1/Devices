#!/usr/bin/env python3
"""Calculate fixture pulse calibration from a measured volume."""
import argparse

def main():
    p = argparse.ArgumentParser()
    p.add_argument('--pulses', required=True, type=int)
    p.add_argument('--litres', required=True, type=float)
    a = p.parse_args()
    if a.pulses <= 0 or a.litres <= 0:
        p.error('pulses and litres must be positive')
    print(f'{a.pulses / a.litres:.3f} pulses/litre (repeat calibration across expected flow range)')

if __name__ == '__main__': main()
