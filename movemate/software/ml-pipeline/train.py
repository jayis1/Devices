"""Synthetic demonstration of lexical lookup ranking; not a trained/validated model."""
import re
examples=[("kettle mugs", "Kitchen box: kettle and mugs"),("winter coat", "Bedroom box: winter coats"),("screwdriver", "Workshop box: screwdriver set")]
def tokens(s): return set(re.findall(r"[a-z0-9]+",s.lower()))
def score(q, text):
    a=tokens(q); b=tokens(text)
    return len(a & b)/max(1,len(a))
if __name__=="__main__":
    q="kettle"
    ranked=sorted(((score(q,t),t) for _,t in examples),reverse=True)
    print("synthetic-demo query:",q)
    print("top result:",ranked[0][1])
    print("No accuracy claim: examples are synthetic and tiny.")
