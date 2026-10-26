import json, random
random.seed(7)
# Demonstration only: synthetic RMS windows, not representative field data.
def dataset(n=200):
    return [(random.gauss(500,120),1) for _ in range(n//2)]+[(abs(random.gauss(20,12)),0) for _ in range(n//2)]
if __name__ == '__main__':
    rows=dataset()
    correct=sum((x>120)==bool(y) for x,y in rows)
    print(json.dumps({'synthetic_samples':len(rows),'threshold_baseline_accuracy':correct/len(rows),'threshold_mg':120}))
