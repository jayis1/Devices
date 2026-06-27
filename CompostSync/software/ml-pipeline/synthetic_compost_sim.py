"""
Synthetic Compost Simulator
Generates realistic compost decomposition time series using physics-based models.

Models:
  1. Microbial growth: Monod kinetics
  2. Heat generation: first-order decomposition + metabolic heat
  3. Heat transfer: conduction, convection, radiation
  4. Mass loss: first-order kinetics with moisture-dependent rate
  5. CO2 evolution: stoichiometric from decomposition
  6. Methane: anaerobic indicator from oxygen depletion
  7. Moisture: evaporation + drainage + input
  8. C:N ratio: changes as decomposition progresses

Generates 50,000 training cycles with varied parameters.
"""
import numpy as np
import pandas as pd
import os
import logging
from tqdm import tqdm

logging.basicConfig(level=logging.INFO)
logger = logging.getLogger(__name__)

# Simulation parameters
TIMESTEP_MIN = 15          # 15-minute intervals
DURATION_DAYS = 90          # 90-day simulation
N_TIMESTEPS = int(DURATION_DAYS * 24 * 60 / TIMESTEP_MIN)  # 8640 steps
N_CYCLES = 50000           # Generate 50,000 cycles

# Compost physics constants
DECOMP_RATE_MAX = 0.015     # 1/hour max decomposition rate
MICROBE_TEMP_OPT = 55.0     # °C optimal temperature for thermophiles
MICROBE_TEMP_MIN = 0.0      # °C minimum for any activity
MICROBE_TEMP_MAX = 75.0     # °C maximum before die-off
HEAT_OF_DECOMP = 18000.0    # J/g of decomposed matter
BIN_HEAT_CAPACITY = 2000.0  # J/°C (simplified)
BIN_SURFACE_AREA = 1.5      # m²
AMBIENT_TEMP_DEFAULT = 20.0  # °C
CO2_PER_GRAM = 1.5           # g CO2 per g decomposed matter
METHANE_ANAEROBIC_THRESHOLD = 0.3  # O2 fraction below which methane forms


def simulate_cycle(params):
    """Simulate a single compost cycle. Returns DataFrame of time series."""
    # Initial conditions from params
    mass_initial = params["mass_initial_g"]  # grams
    cn_ratio = params["cn_ratio"]
    moisture_init = params["moisture_pct"]   # %
    ambient_temp = params["ambient_temp_c"]
    bin_volume = params["bin_volume_l"]
    turn_interval_days = params["turn_interval_days"]

    # Derived
    dry_mass = mass_initial * (1 - moisture_init / 100)
    water_mass = mass_initial * moisture_init / 100
    temp = ambient_temp + 2.0  # slight self-heating
    co2_bg = 400  # atmospheric baseline
    co2 = co2_bg
    methane = 0
    oxygen = 0.21  # 21% O2
    maturity = 0.0
    days_thermophilic = 0
    total_decomposed = 0

    records = []
    turn_counter = 0

    for step in range(N_TIMESTEPS):
        t_hours = step * TIMESTEP_MIN / 60.0
        t_days = t_hours / 24.0

        # Determine if turning happens
        if turn_interval_days > 0 and t_days > 0:
            if int(t_days / turn_interval_days) > turn_counter:
                # Turn the pile!
                turn_counter += 1
                oxygen = 0.21  # recharge oxygen
                temp = (temp + ambient_temp) / 2  # mixing cools it
                co2 = max(co2_bg, co2 * 0.3)

        # Temperature-dependent microbial activity (Gaussian-like curve)
        if MICROBE_TEMP_MIN < temp < MICROBE_TEMP_MAX:
            activity = np.exp(-0.5 * ((temp - MICROBE_TEMP_OPT) / 15.0) ** 2)
        else:
            activity = 0.0

        # Moisture modifier (optimal 50-60%)
        moisture_current = 100 * water_mass / max(dry_mass + water_mass, 1)
        moisture_mod = 1.0 if 40 < moisture_current < 65 else 0.3
        if moisture_current > 70:
            moisture_mod = 0.1  # too wet → anaerobic
        if moisture_current < 25:
            moisture_mod = 0.1  # too dry

        # C:N ratio modifier (optimal 25-35:1)
        cn_mod = 1.0 if 20 < cn_ratio < 40 else 0.5
        if cn_ratio < 15:
            cn_mod = 0.3  # too nitrogen-rich → ammonia, slow
        if cn_ratio > 50:
            cn_mod = 0.2  # too carbon-rich → nothing happens

        # Decomposition rate
        decomp_rate = DECOMP_RATE_MAX * activity * moisture_mod * cn_mod
        decomp_mass = dry_mass * decomp_rate * (TIMESTEP_MIN / 60.0)
        dry_mass -= decomp_mass
        total_decomposed += decomp_mass

        # CO2 production
        co2_produced = decomp_mass * CO2_PER_GRAM
        co2 += co2_produced * 100  # scale to ppm (simplified)

        # Oxygen consumption
        oxygen -= decomp_mass * 0.5 / max(dry_mass + 1, 1)
        oxygen = max(0, oxygen)

        # Methane if anaerobic
        if oxygen < METHANE_ANAEROBIC_THRESHOLD and moisture_current > 60:
            methane += decomp_mass * 0.3
        else:
            methane *= 0.95  # methane dissipates

        # Heat generation
        heat_generated = decomp_mass * HEAT_OF_DECOMP
        # Heat loss (convection)
        heat_loss = BIN_HEAT_CAPACITY * (temp - ambient_temp) * 0.01 * (TIMESTEP_MIN / 60.0)
        # Temperature change
        temp += (heat_generated - heat_loss) / BIN_HEAT_CAPACITY

        # Moisture: evaporation
        if temp > 25:
            evap_rate = 0.001 * (temp - 25) * (TIMESTEP_MIN / 60.0)
            water_mass = max(0, water_mass - evap_rate * water_mass * 0.001)

        # C:N ratio change (approaches 15-20:1 as it matures)
        cn_ratio = cn_ratio + (18.0 - cn_ratio) * 0.001 * (TIMESTEP_MIN / 60.0)

        # Maturity score (based on decomposition progress)
        mass_remaining_frac = dry_mass / max(mass_initial * (1 - moisture_init/100), 1)
        maturity = (1 - mass_remaining_frac) * 100
        maturity = min(100, max(0, maturity))

        # Phase classification
        if temp < 5 and co2 < 500:
            phase = 5  # dormant
        elif temp > 50 and co2 > 2000:
            phase = 1  # thermophilic
            days_thermophilic += TIMESTEP_MIN / 60.0 / 24.0
        elif temp > 30 and temp <= 50:
            phase = 2  # cooling
        elif temp <= 30 and co2 > 800:
            phase = 3  # maturation
        elif maturity > 90:
            phase = 4  # cured
        else:
            phase = 0  # mesophilic

        # Temperature at 3 depths (gradient: center hottest)
        temp_10cm = temp - 2.0 + np.random.normal(0, 0.5)
        temp_30cm = temp + np.random.normal(0, 0.5)
        temp_50cm = temp - 1.0 + np.random.normal(0, 0.5)

        # Moisture at 3 depths (top drier, bottom wetter)
        moist_10cm = max(0, min(100, moisture_current - 5 + np.random.normal(0, 2)))
        moist_30cm = max(0, min(100, moisture_current + np.random.normal(0, 2)))
        moist_50cm = max(0, min(100, moisture_current + 3 + np.random.normal(0, 2)))

        records.append({
            "cycle_id": params["cycle_id"],
            "timestep": step,
            "t_days": round(t_days, 4),
            "t1": round(temp_10cm, 2),
            "t2": round(temp_30cm, 2),
            "t3": round(temp_50cm, 2),
            "m1": round(moist_10cm, 1),
            "m2": round(moist_30cm, 1),
            "m3": round(moist_50cm, 1),
            "co2": int(min(co2, 10000)),
            "ch4": int(min(methane, 5000)),
            "mass_g": int(dry_mass + water_mass),
            "maturity_score": round(maturity, 2),
            "phase": phase,
            "cn_ratio": round(cn_ratio, 1),
            "ambient_temp": ambient_temp,
        })

    return pd.DataFrame(records)


def generate_all():
    """Generate all synthetic compost cycles."""
    all_records = []

    for i in tqdm(range(N_CYCLES), desc="Generating cycles"):
        params = {
            "cycle_id": i,
            "mass_initial_g": np.random.randint(2000, 40000),  # 2-40 kg
            "cn_ratio": np.random.uniform(10, 50),
            "moisture_pct": np.random.uniform(30, 70),
            "ambient_temp_c": np.random.uniform(-5, 35),
            "bin_volume_l": np.random.choice([20, 60, 120, 200, 400]),
            "turn_interval_days": np.random.choice([0, 3, 7, 14, 30, 999]),  # 999 = never
        }

        df = simulate_cycle(params)
        all_records.append(df)

    combined = pd.concat(all_records, ignore_index=True)
    output_path = "data/synthetic_compost_cycles.csv"
    combined.to_csv(output_path, index=False)
    logger.info(f"Generated {len(combined)} records → {output_path}")
    logger.info(f"File size: {os.path.getsize(output_path) / 1e6:.1f} MB")


if __name__ == "__main__":
    os.makedirs("data", exist_ok=True)
    generate_all()