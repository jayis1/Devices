#!/usr/bin/env python3
"""Synthetic-only demonstration of a simple daily staple demand baseline."""
import random

def main():
    rng=random.Random(7)
    values=[max(0.0, 1.2+rng.gauss(0,0.35)) for _ in range(30)]
    print(f'synthetic days={len(values)} mean_units_per_day={sum(values)/len(values):.2f}')
    print('Not a trained or evaluated production model.')
if __name__=='__main__': main()
