"""Baseline rule, not trained model: CO2 is context only, not safety advice."""
def ventilation_context(co2_ppm):
    if co2_ppm is None or not 350 <= co2_ppm <= 10000: return 'unknown'
    if co2_ppm < 800: return 'lower'
    if co2_ppm < 1200: return 'elevated'
    return 'high'
if __name__ == '__main__':
    for value in (None, 650, 950, 1400): print(value, ventilation_context(value))
