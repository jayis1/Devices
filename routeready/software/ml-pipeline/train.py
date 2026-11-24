import random
# Synthetic toy workload data only; no real user/site measurements or model claims.
def main():
 random.seed(19); xs=[max(0,random.gauss(5,1.5)) for _ in range(100)]
 print({'synthetic_records':len(xs),'median_minutes':round(sorted(xs)[50],1),'status':'illustrative baseline only'})
if __name__=='__main__': main()
