-- CardioSync TimescaleDB schema
-- Run on container init

-- Enable TimescaleDB extension
CREATE EXTENSION IF NOT EXISTS timescaledb;

-- Users table
CREATE TABLE IF NOT EXISTS users (
    id SERIAL PRIMARY KEY,
    username VARCHAR(255) UNIQUE NOT NULL,
    email VARCHAR(255) UNIQUE NOT NULL,
    password_hash VARCHAR(255) NOT NULL,
    full_name VARCHAR(255),
    date_of_birth DATE,
    created_at TIMESTAMPTZ DEFAULT NOW(),
    emergency_contact_1 VARCHAR(50),
    emergency_contact_2 VARCHAR(50),
    chads_vasc_score INTEGER DEFAULT 0
);

-- ECG events table (arrhythmia detections)
CREATE TABLE IF NOT EXISTS ecg_events (
    id SERIAL PRIMARY KEY,
    user_id INTEGER REFERENCES users(id),
    event_type VARCHAR(50) NOT NULL,  -- AFib, PVC, VT, Bradycardia
    confidence FLOAT,
    heart_rate_bpm INTEGER,
    ecg_strip JSONB,                   -- 10s ECG strip as array
    motion_artifact BOOLEAN DEFAULT FALSE,
    timestamp TIMESTAMPTZ NOT NULL DEFAULT NOW()
);

-- Blood pressure records
CREATE TABLE IF NOT EXISTS bp_records (
    id SERIAL PRIMARY KEY,
    user_id INTEGER REFERENCES users(id),
    systolic INTEGER NOT NULL,
    diastolic INTEGER NOT NULL,
    map INTEGER NOT NULL,
    heart_rate INTEGER,
    position_ok BOOLEAN,
    quality INTEGER,
    schedule_id INTEGER,              -- 0=on-demand, 1=AM, 2=PM, 3=post-activity
    timestamp TIMESTAMPTZ NOT NULL DEFAULT NOW()
);

-- PPG heart rate (continuous from smart ring)
CREATE TABLE IF NOT EXISTS ppg_hr (
    id SERIAL PRIMARY KEY,
    user_id INTEGER REFERENCES users(id),
    heart_rate INTEGER NOT NULL,
    spo2 INTEGER,
    skin_temp_c10 INTEGER,
    motion_artifact BOOLEAN DEFAULT FALSE,
    timestamp TIMESTAMPTZ NOT NULL DEFAULT NOW()
);

-- HRV records (every 5 min from smart ring)
CREATE TABLE IF NOT EXISTS hrv_records (
    id SERIAL PRIMARY KEY,
    user_id INTEGER REFERENCES users(id),
    rmssd_ms INTEGER,
    sdnn_ms INTEGER,
    pnn50 FLOAT,
    source VARCHAR(20),               -- 'ecg' or 'ppg'
    timestamp TIMESTAMPTZ NOT NULL DEFAULT NOW()
);

-- ECG continuous stream (hypertable for time-series)
CREATE TABLE IF NOT EXISTS ecg_stream (
    id SERIAL PRIMARY KEY,
    user_id INTEGER REFERENCES users(id),
    sample_value INTEGER NOT NULL,
    sample_rate INTEGER DEFAULT 250,
    motion_artifact BOOLEAN DEFAULT FALSE,
    timestamp TIMESTAMPTZ NOT NULL DEFAULT NOW()
);

-- Convert to TimescaleDB hypertables (partition by time)
SELECT create_hypertable('ecg_stream', 'timestamp', if_not_exists => TRUE);
SELECT create_hypertable('ppg_hr', 'timestamp', if_not_exists => TRUE);
SELECT create_hypertable('hrv_records', 'timestamp', if_not_exists => TRUE);

-- Risk assessments
CREATE TABLE IF NOT EXISTS risk_assessments (
    id SERIAL PRIMARY KEY,
    user_id INTEGER REFERENCES users(id),
    stroke_risk_30d FLOAT NOT NULL,    -- 0-100%
    afib_burden_pct FLOAT,             -- % time in AFib (last 24h)
    bp_category VARCHAR(50),
    hrv_trend VARCHAR(50),
    sleep_apnea_risk FLOAT,
    pots_detected BOOLEAN DEFAULT FALSE,
    assessment_data JSONB,              -- detailed risk factors
    timestamp TIMESTAMPTZ NOT NULL DEFAULT NOW()
);

-- Emergency alerts log
CREATE TABLE IF NOT EXISTS emergency_alerts (
    id SERIAL PRIMARY KEY,
    user_id INTEGER REFERENCES users(id),
    alert_type VARCHAR(50) NOT NULL,
    heart_rate INTEGER,
    location_lat FLOAT,
    location_lon FLOAT,
    contacts_notified TEXT[],
    delivery_status VARCHAR(50),       -- sent, delivered, failed
    timestamp TIMESTAMPTZ NOT NULL DEFAULT NOW()
);

-- Device registration
CREATE TABLE IF NOT EXISTS devices (
    id SERIAL PRIMARY KEY,
    user_id INTEGER REFERENCES users(id),
    device_type VARCHAR(50) NOT NULL,  -- hub, ecg_patch, bp_cuff, smart_ring
    mac_address VARCHAR(17) UNIQUE,
    firmware_version VARCHAR(20),
    last_seen TIMESTAMPTZ DEFAULT NOW(),
    battery_pct INTEGER,
    status VARCHAR(20) DEFAULT 'online'
);

-- Indexes for fast queries
CREATE INDEX idx_ecg_events_user_ts ON ecg_events (user_id, timestamp DESC);
CREATE INDEX idx_bp_records_user_ts ON bp_records (user_id, timestamp DESC);
CREATE INDEX idx_ppg_hr_user_ts ON ppg_hr (user_id, timestamp DESC);
CREATE INDEX idx_hrv_user_ts ON hrv_records (user_id, timestamp DESC);
CREATE INDEX idx_ecg_stream_user_ts ON ecg_stream (user_id, timestamp DESC);
CREATE INDEX idx_risk_user_ts ON risk_assessments (user_id, timestamp DESC);