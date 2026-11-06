"""Baseline flagging only; thresholds are examples, not preservation standards."""
def flag(sample, previous=None):
    flags = []
    if not 0 <= sample['relative_humidity_pct'] <= 100: flags.append('invalid_rh')
    if previous and abs(sample['relative_humidity_pct'] - previous['relative_humidity_pct']) > 15: flags.append('review_step_change')
    return flags
