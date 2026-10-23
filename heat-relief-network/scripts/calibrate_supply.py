#!/usr/bin/env python3
"""Calibrate supply-node load-cell zero/scale from known reference weights."""
import argparse

def main() -> None:
    p = argparse.ArgumentParser()
    p.add_argument("--zero-count", type=int, required=True)
    p.add_argument("--known-grams", type=float, required=True)
    p.add_argument("--loaded-count", type=int, required=True)
    a = p.parse_args()
    if a.known_grams <= 0 or a.loaded_count == a.zero_count:
        p.error("known weight must be positive and counts must differ")
    print(f"counts_per_gram={(a.loaded_count-a.zero_count)/a.known_grams:.6f}")

if __name__ == "__main__":
    main()
