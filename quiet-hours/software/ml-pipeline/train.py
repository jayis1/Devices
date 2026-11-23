#!/usr/bin/env python3
"""Synthetic-only threshold demo; not trained or evidence of efficacy."""
import argparse, random

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--demo', action='store_true')
    args = parser.parse_args()
    if not args.demo:
        parser.error('no real labeled dataset is supplied; use --demo for synthetic smoke test')
    random.seed(7)
    values = [random.randint(250, 750) for _ in range(20)]
    print(f'synthetic_samples={len(values)} mean_db10={sum(values)//len(values)}')
    print('No model fit, evaluation, or performance claim.')
if __name__ == '__main__': main()
