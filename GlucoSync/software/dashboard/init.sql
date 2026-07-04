-- GlucoSync Database Schema (TimescaleDB / PostgreSQL)

CREATE TABLE IF NOT EXISTS users (
    id TEXT PRIMARY KEY,
    email TEXT UNIQUE NOT NULL,
    password_hash TEXT NOT NULL,
    diabetes_type TEXT NOT NULL DEFAULT 'T2D',
    weight_kg REAL NOT NULL DEFAULT 80,
    target_glucose INTEGER DEFAULT 100,
    hypo_threshold INTEGER DEFAULT 70,
    hyper_threshold INTEGER DEFAULT 180,
    created_at TIMESTAMPTZ DEFAULT NOW()
);

CREATE TABLE IF NOT EXISTS glucose_readings (
    id SERIAL PRIMARY KEY,
    user_id TEXT NOT NULL,
    glucose_mgdl INTEGER NOT NULL,
    trend REAL DEFAULT 0,
    sensor_state INTEGER DEFAULT 0,
    confidence INTEGER DEFAULT 0,
    forecast_30 INTEGER DEFAULT 0,
    forecast_60 INTEGER DEFAULT 0,
    hypo_risk INTEGER DEFAULT 0,
    risk_score INTEGER DEFAULT 0,
    iob REAL DEFAULT 0,
    cob REAL DEFAULT 0,
    hr INTEGER DEFAULT 0,
    activity INTEGER DEFAULT 0,
    created_at TIMESTAMPTZ DEFAULT NOW()
);

SELECT create_hypertable('glucose_readings', 'created_at', if_not_exists => TRUE);

CREATE TABLE IF NOT EXISTS meals (
    id SERIAL PRIMARY KEY,
    user_id TEXT NOT NULL,
    food_class_id INTEGER,
    food_confidence INTEGER DEFAULT 0,
    carb_grams INTEGER,
    portion_grams INTEGER,
    glycemic_index INTEGER,
    spectral_bands INTEGER,
    created_at TIMESTAMPTZ DEFAULT NOW()
);

SELECT create_hypertable('meals', 'created_at', if_not_exists => TRUE);

CREATE TABLE IF NOT EXISTS insulin_events (
    id SERIAL PRIMARY KEY,
    user_id TEXT NOT NULL,
    pen_type INTEGER,      -- 0=basal, 1=bolus
    pen_id INTEGER,
    estimated_units INTEGER,
    confidence INTEGER,
    injection_dur_ms INTEGER,
    created_at TIMESTAMPTZ DEFAULT NOW()
);

SELECT create_hypertable('insulin_events', 'created_at', if_not_exists => TRUE);

CREATE TABLE IF NOT EXISTS activity_log (
    id SERIAL PRIMARY KEY,
    user_id TEXT NOT NULL,
    hr INTEGER DEFAULT 0,
    hrv_rmssd INTEGER DEFAULT 0,
    activity_class INTEGER,
    intensity INTEGER DEFAULT 0,
    confidence INTEGER DEFAULT 0,
    created_at TIMESTAMPTZ DEFAULT NOW()
);

SELECT create_hypertable('activity_log', 'created_at', if_not_exists => TRUE);

CREATE TABLE IF NOT EXISTS emergency_contacts (
    id SERIAL PRIMARY KEY,
    user_id TEXT NOT NULL,
    name TEXT NOT NULL,
    phone TEXT NOT NULL,
    relationship TEXT,
    created_at TIMESTAMPTZ DEFAULT NOW()
);

CREATE TABLE IF NOT EXISTS hub_status (
    id SERIAL PRIMARY KEY,
    hub_id TEXT,
    battery INTEGER,
    nodes INTEGER,
    glucose INTEGER,
    iob REAL DEFAULT 0,
    created_at TIMESTAMPTZ DEFAULT NOW()
);

SELECT create_hypertable('hub_status', 'created_at', if_not_exists => TRUE);

-- Indexes
CREATE INDEX idx_glucose_user_time ON glucose_readings (user_id, created_at DESC);
CREATE INDEX idx_meals_user_time ON meals (user_id, created_at DESC);
CREATE INDEX idx_insulin_user_time ON insulin_events (user_id, created_at DESC);
CREATE INDEX idx_activity_user_time ON activity_log (user_id, created_at DESC);