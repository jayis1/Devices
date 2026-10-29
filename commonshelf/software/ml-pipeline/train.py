import argparse, json, random

def main():
    p=argparse.ArgumentParser(description="Synthetic demo only; not food-safety ML")
    p.add_argument('--out', default='model-demo.json')
    a=p.parse_args(); random.seed(7)
    samples=[round(4.0+random.gauss(0,0.15),2) for _ in range(100)]
    print(f"synthetic n={len(samples)} mean={sum(samples)/len(samples):.2f}C; no labels or efficacy claim")
    with open(a.out,'w') as f: json.dump({'demo_only':True,'n':len(samples)},f)
if __name__=='__main__': main()
