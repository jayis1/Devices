#!/usr/bin/env python3
"""Manual PM sensor check: compare at least three stable reference points."""
def calibration_record(points):
    if len(points) < 3 or any(ref < 0 or reading < 0 for ref, reading in points):
        raise ValueError('need >=3 nonnegative (reference, sensor) pairs')
    return {'pairs': points, 'status': 'record only; no calibration claim'}
if __name__ == '__main__':
    print('Use a traceable reference instrument; record environment and serials.')
