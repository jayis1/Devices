"""Synthetic demonstration only; not validated predictive maintenance."""
import numpy as np
from sklearn.linear_model import LogisticRegression

def make_data(seed=17):
    rng=np.random.default_rng(seed)
    healthy=rng.normal([2.0, 240.0], [0.5, 10.0], size=(100,2))
    inspect=rng.normal([5.0, 190.0], [1.0, 20.0], size=(100,2))
    return np.vstack((healthy,inspect)), np.array([0]*100+[1]*100)
if __name__=='__main__':
    from sklearn.model_selection import train_test_split
    from sklearn.metrics import classification_report
    X,y=make_data(); a,b,c,d=train_test_split(X,y,test_size=.25,random_state=17,stratify=y)
    model=LogisticRegression().fit(a,c)
    print('synthetic toy split only; not real-world accuracy')
    print(classification_report(c,model.predict(a)))
