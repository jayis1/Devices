import json

def evaluate(reference: str, hypothesis: str) -> dict[str, float]:
    """Word error rate helper. This does not train or validate a caption model."""
    ref = reference.split()
    hyp = hypothesis.split()
    row = list(range(len(hyp) + 1))
    for i, word in enumerate(ref, start=1):
        current = [i]
        for j, guess in enumerate(hyp, start=1):
            current.append(min(current[-1] + 1, row[j] + 1, row[j - 1] + (word != guess)))
        row = current
    denom = max(1, len(ref))
    return {"wer": row[-1] / denom, "reference_words": float(len(ref))}

if __name__ == "__main__":
    print(json.dumps(evaluate("sample reference text", "sample hypothesis text"), sort_keys=True))
