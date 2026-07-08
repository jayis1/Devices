-- QuakeGuard TimescaleDB initialization
-- TimescaleDB extension
CREATE EXTENSION IF NOT EXISTS timescaledb;

-- Enable compression on hypertables (after data exists)
-- ALTER TABLE events SET (timescaledb.compress);
-- SELECT add_compression_policy('events', INTERVAL '7 days');

-- Retention policies
-- SELECT add_retention_policy('events', INTERVAL '365 days');
-- SELECT add_retention_policy('structural_reports', INTERVAL '730 days');
-- SELECT add_retention_policy('gas_readings', INTERVAL '90 days');