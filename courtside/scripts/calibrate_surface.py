#!/usr/bin/env python3
"""Print a guided wetness calibration record; no device configuration is changed."""
import argparse

parser = argparse.ArgumentParser()
parser.add_argument('--dry', type=int, required=True, help='ADC reading on known dry surface')
parser.add_argument('--wet', type=int, required=True, help='ADC reading on intentionally wetted test coupon')
a = parser.parse_args()
if not (0 <= a.dry <= 32767 and 0 <= a.wet <= 32767) or a.dry == a.wet:
    parser.error('readings must be distinct values between 0 and 32767')
print(f'dry={a.dry} wet={a.wet} midpoint={((a.dry+a.wet)//2)}')
print('Record site, sensor placement, material and calibration date; independently verify threshold.')
