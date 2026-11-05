#!/usr/bin/env python3
"""Bench calibration helper; requires operator-supplied known mass in grams."""
def factor(raw_delta, known_g):
 if raw_delta==0 or known_g<=0: raise ValueError("nonzero raw delta and positive mass required")
 return raw_delta/known_g
if __name__=="__main__": print("Use known certified test mass; enter readings in a controlled calibration workflow.")
