"""Synthetic threshold demonstration; no trained or validated predictor."""
def classify(lux, threshold=5.0):
    return "inspect" if lux < threshold else "normal"
if __name__ == "__main__":
    samples=[(0.4,"inspect"),(12.0,"normal")]
    for lux,label in samples: print(lux,label,classify(lux))
    print("Synthetic examples only; not measured performance.")
