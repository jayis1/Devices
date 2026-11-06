import argparse, csv, random
p=argparse.ArgumentParser(description='Demo synthetic LineWise baseline data only')
p.add_argument('--rows',type=int,default=300); a=p.parse_args(); random.seed(7)
# Synthetic toy label; not measured and not suitable for real decisions.
with open('synthetic.csv','w',newline='') as f:
 w=csv.writer(f); w.writerow(['rain_tips_10m','wind_mm_s','relative_humidity_pct','line_useful_demo'])
 for _ in range(a.rows):
  rain=random.randint(0,4); wind=random.randint(0,9000); rh=random.randint(20,100)
  w.writerow([rain,wind,rh,int(rain==0 and wind<6500 and rh<90)])
print(f'generated {a.rows} clearly synthetic rows')
