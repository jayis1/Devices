"""Synthetic-only anomaly demo. Metrics are not field performance."""
import random

def main():
    random.seed(7)
    samples = [20.0 + random.gauss(0, 0.5) for _ in range(100)] + [31.0]
    mean = sum(samples[:100]) / 100
    variance = sum((x-mean)**2 for x in samples[:100]) / 100
    sigma = variance ** 0.5
    print({'synthetic_samples':len(samples), 'last_z':round((samples[-1]-mean)/sigma,2), 'hint_only':True})

if __name__ == '__main__': main()
