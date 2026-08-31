# PregnancySync Radio Protocol

## Timing

- Superframe: 60 seconds
- 8 uplink slots for sleepy nodes
- 2 downlink/control slots
- slot length: 180 ms
- guard interval: 20 ms

## Reliability

- CRC16 on every frame
- sequence numbers modulo 256
- ACK/NACK for config, OTA, and alerts
- telemetry summaries retried 3 times before local retention

## Payload examples

### Belly band summary
- movement_count_10m
- movement_variability
- posture_pct_left
- posture_pct_supine
- skin_temp_c
- ehg_activity_index
- battery_mv

### BP cuff summary
- systolic_mmHg
- diastolic_mmHg
- map_mmHg
- pulse_rate_bpm
- waveform_quality
- motion_artifact_score

### Strip reader result
- protein_level
- glucose_level
- ketone_level
- specific_gravity
- nitrite_positive
- hydration_bottle_ml

### Sleep pad summary
- hours_recorded
- supine_minutes
- left_side_minutes
- respiration_rate
- restlessness_index
