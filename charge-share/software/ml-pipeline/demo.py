# Synthetic toy baseline demonstration only; no real-world model or claims.
import random
random.seed(7)
# Demonstrate transparent threshold rule against invented readings.
samples=[(random.randint(0,5200), random.randint(0,2500)) for _ in range(20)]
for mv,ma in samples[:3]:
    label='output-present' if mv >= 4500 else 'unknown-or-unavailable'
    print(f'{mv}mV {ma}mA -> {label}')
