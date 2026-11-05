import random
# Synthetic demonstration only; never use these values for staffing decisions.
def main():
    random.seed(7)
    synthetic_wait_minutes=[max(0, round(random.gauss(24, 8), 1)) for _ in range(100)]
    print({'synthetic_samples':len(synthetic_wait_minutes),'median_minutes':sorted(synthetic_wait_minutes)[50], 'status':'illustrative baseline; no trained model'})
if __name__=='__main__': main()
