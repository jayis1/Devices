"""Synthetic demonstration only; never represents observed library behavior."""
import csv, random
random.seed(7)
with open('synthetic_events.csv','w',newline='') as f:
 w=csv.writer(f); w.writerow(['hour','dock_changes','staff_confirmed_returns'])
 for hour in range(24):
  changes=random.randint(0,8); w.writerow([hour,changes,random.randint(0,changes)])
print('Wrote synthetic_events.csv; no model or accuracy claim.')
