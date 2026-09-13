import argparse, json
import numpy as np
from sklearn.ensemble import IsolationForest
from sklearn.metrics import roc_auc_score
p=argparse.ArgumentParser();p.add_argument('--input',required=True,help='CSV: normal feature rows, optional label');p.add_argument('--out',default='asset_baseline.json');a=p.parse_args()
x=np.loadtxt(a.input,delimiter=',',skiprows=1,usecols=range(5));m=IsolationForest(contamination=.02,random_state=42).fit(x)
json.dump({'features':['rms','crest','band_1','band_2','temperature'],'offset':m.offset_,'threshold_note':'calibrate per asset'},open(a.out,'w'))
print('trained rows=',len(x),'features=',x.shape[1])
