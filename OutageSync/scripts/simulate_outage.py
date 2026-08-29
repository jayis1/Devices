from __future__ import annotations

from datetime import datetime, timezone

timeline = [
    {'minute': 0, 'event': 'brownout_detected'},
    {'minute': 18, 'event': 'grid_loss'},
    {'minute': 26, 'event': 'shed_noncritical'},
    {'minute': 74, 'event': 'generator_charge_window'},
    {'minute': 143, 'event': 'restore_staged'},
]

for item in timeline:
    print(f"{datetime.now(timezone.utc).isoformat()} minute={item['minute']} event={item['event']}")
