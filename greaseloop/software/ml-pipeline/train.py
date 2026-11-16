import argparse, json, random

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--smoke', action='store_true')
    args = parser.parse_args()
    random.seed(7)
    samples = [{'mass_g': random.randint(0, 5000), 'label': 'synthetic'} for _ in range(16)]
    if args.smoke:
        print(json.dumps({'mode':'synthetic-pipeline-smoke-only','rows':len(samples),'min_g':min(x['mass_g'] for x in samples),'max_g':max(x['mass_g'] for x in samples)}))
    else:
        print('No training performed: supply consented, labeled real-world data and a validated evaluation plan.')

if __name__ == '__main__': main()
