"""Synthetic demo only; no field-performance claim."""
import random
def main():
 random.seed(17); counts=[random.randint(1,30) for _ in range(100)]; baseline=sum(counts)/len(counts)
 print(f"synthetic samples={len(counts)} mean_items_per_hour={baseline:.2f}; not validated")
if __name__=='__main__':main()
