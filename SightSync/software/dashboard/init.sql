-- SightSync TimescaleDB Schema
-- License: MIT

-- Enable TimescaleDB extension
CREATE EXTENSION IF NOT EXISTS timescaledb;

-- ── Users ──────────────────────────────────────────────────────────

CREATE TABLE IF NOT EXISTS users (
    id          UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    email       TEXT UNIQUE NOT NULL,
    password_hash TEXT NOT NULL,
    name        TEXT NOT NULL,
    age         INTEGER,
    created_at  TIMESTAMPTZ DEFAULT NOW()
);

-- ── Child profiles (for myopia tracking) ───────────────────────────

CREATE TABLE IF NOT EXISTS children (
    id              UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    user_id         UUID REFERENCES users(id),
    name            TEXT NOT NULL,
    birth_date      DATE,
    baseline_refraction FLOAT,  -- diopters
    axial_length_mm  FLOAT,
    created_at      TIMESTAMPTZ DEFAULT NOW()
);

-- ── Hypertable: Fatigue readings ────────────────────────────────────

CREATE TABLE IF NOT EXISTS fatigue_readings (
    time            TIMESTAMPTZ NOT NULL,
    user_id         UUID,
    fatigue_score   INTEGER,
    blink_rate      INTEGER,
    viewing_distance_mm INTEGER,
    ambient_lux     INTEGER,
    blue_dose_mj_cm2  INTEGER,
    posture_risk    INTEGER,
    dry_eye_risk    INTEGER,
    minutes_since_break INTEGER
);
SELECT create_hypertable('fatigue_readings', 'time', if_not_exists => TRUE);

-- ── Hypertable: Distance readings ──────────────────────────────────

CREATE TABLE IF NOT EXISTS distance_readings (
    time            TIMESTAMPTZ NOT NULL,
    user_id         UUID,
    distance_mm     INTEGER,
    near_work_flag  BOOLEAN,
    near_work_minutes INTEGER
);
SELECT create_hypertable('distance_readings', 'time', if_not_exists => TRUE);

-- ── Hypertable: Blink readings ──────────────────────────────────────

CREATE TABLE IF NOT EXISTS blink_readings (
    time            TIMESTAMPTZ NOT NULL,
    user_id         UUID,
    blinks_per_min  INTEGER,
    confidence      INTEGER,
    quality         INTEGER
);
SELECT create_hypertable('blink_readings', 'time', if_not_exists => TRUE);

-- ── Hypertable: Light exposure ──────────────────────────────────────

CREATE TABLE IF NOT EXISTS light_readings (
    time            TIMESTAMPTZ NOT NULL,
    user_id         UUID,
    ambient_lux     INTEGER,
    blue_light_mw   INTEGER,
    cct_estimate    INTEGER,
    blue_dose_today INTEGER
);
SELECT create_hypertable('light_readings', 'time', if_not_exists => TRUE);

-- ── Hypertable: Posture readings ───────────────────────────────────

CREATE TABLE IF NOT EXISTS posture_readings (
    time            TIMESTAMPTZ NOT NULL,
    user_id         UUID,
    pitch_centi     INTEGER,
    roll_centi      INTEGER,
    yaw_centi       INTEGER,
    forward_head    BOOLEAN,
    posture_risk    INTEGER
);
SELECT create_hypertable('posture_readings', 'time', if_not_exists => TRUE);

-- ── Myopia forecasts ────────────────────────────────────────────────

CREATE TABLE IF NOT EXISTS myopia_forecasts (
    id              UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    time            TIMESTAMPTZ DEFAULT NOW(),
    child_id        UUID REFERENCES children(id),
    risk_30day      INTEGER,
    risk_90day      INTEGER,
    refractive_delta FLOAT,
    near_work_today INTEGER,
    outdoor_today   INTEGER,
    recommendation  TEXT
);

-- ── Lamp policy ─────────────────────────────────────────────────────

CREATE TABLE IF NOT EXISTS lamp_policies (
    id              UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    user_id         UUID REFERENCES users(id),
    mode            TEXT DEFAULT 'circadian',
    schedule        JSONB,
    created_at      TIMESTAMPTZ DEFAULT NOW()
);

-- ── Retention policy: 90 days of raw data ───────────────────────────

SELECT add_retention_policy('fatigue_readings', INTERVAL '90 days', if_not_exists => TRUE);
SELECT add_retention_policy('distance_readings', INTERVAL '90 days', if_not_exists => TRUE);
SELECT add_retention_policy('blink_readings', INTERVAL '90 days', if_not_exists => TRUE);
SELECT add_retention_policy('light_readings', INTERVAL '90 days', if_not_exists => TRUE);
SELECT add_retention_policy('posture_readings', INTERVAL '90 days', if_not_exists => TRUE);