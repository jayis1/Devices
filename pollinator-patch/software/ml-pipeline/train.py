"""Synthetic descriptive demonstration; does not train an ecological model."""
import argparse, random
def main():
    p=argparse.ArgumentParser(); p.add_argument('--synthetic',action='store_true'); args=p.parse_args()
    if not args.synthetic: p.error('only --synthetic demonstration data is supported')
    random.seed(7); xs=[random.randint(0,4095) for _ in range(20)]
    print(f'synthetic samples={len(xs)} median_raw={sorted(xs)[len(xs)//2]} (not a model)')
if __name__=='__main__': main()
