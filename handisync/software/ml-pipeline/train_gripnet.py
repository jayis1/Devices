"""Baseline GripNet trainer. Input CSV columns: label plus 40 IMU/FSR feature columns."""
import argparse, pandas as pd
from sklearn.model_selection import train_test_split
from sklearn.ensemble import RandomForestClassifier
from sklearn.metrics import classification_report
p=argparse.ArgumentParser(); p.add_argument('csv'); a=p.parse_args()
df=pd.read_csv(a.csv); x=df.drop(columns=['label']); y=df.label
xt,xv,yt,yv=train_test_split(x,y,stratify=y,test_size=.2,random_state=42)
m=RandomForestClassifier(n_estimators=200,class_weight='balanced',random_state=42).fit(xt,yt)
print(classification_report(yv,m.predict(xv),zero_division=0))
