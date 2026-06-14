#!/bin/bash
# UrbanHarvest — Sensor Calibration Script
# Calibrates plant sensor nodes for accurate soil moisture and EC readings
# Run once when installing new sensors, or when changing soil/pot

set -e

echo "============================================"
echo "  UrbanHarvest — Sensor Calibration"
echo "============================================"
echo ""

SENSOR_ID=${1:-1}
MQTT_BROKER=${2:-"localhost"}
MQTT_PORT=${3:-1883}
TOPIC_CMD="urbanharvest/calibration/sensor_${SENSOR_ID}"
TOPIC_READ="urbanharvest/sensors/sensor_${SENSOR_ID}"

echo "Calibrating Plant Sensor #$SENSOR_ID"
echo "MQTT broker: $MQTT_BROKER:$MQTT_PORT"
echo ""

# ========== MOISTURE CALIBRATION ==========

echo "--- Soil Moisture Calibration ---"
echo ""
echo "Step 1: DRY CALIBRATION"
echo "  Hold the sensor in DRY AIR (not touching anything)"
echo "  Press Enter when ready..."
read -r

echo "  Reading dry value..."
DRY_ADC=$(mosquitto_sub -h "$MQTT_BROKER" -p "$MQTT_PORT" -t "$TOPIC_READ" -C 1 2>/dev/null | python3 -c "import sys,json; d=json.load(sys.stdin); print(d.get('raw_moisture_adc', 3200))" 2>/dev/null || echo "3200")
echo "  Dry ADC value: $DRY_ADC"

echo ""
echo "Step 2: WET CALIBRATION"
echo "  Submerge the sensor probe in a cup of WATER"
echo "  Wait 30 seconds for reading to stabilize"
echo "  Press Enter when ready..."
read -r

echo "  Reading wet value..."
WET_ADC=$(mosquitto_sub -h "$MQTT_BROKER" -p "$MQTT_PORT" -t "$TOPIC_READ" -C 1 2>/dev/null | python3 -c "import sys,json; d=json.load(sys.stdin); print(d.get('raw_moisture_adc', 1500))" 2>/dev/null || echo "1500")
echo "  Wet ADC value: $WET_ADC"

echo ""
echo "  Moisture calibration: DRY=$DRY_ADC → WET=$WET_ADC"

# ========== EC CALIBRATION ==========

echo ""
echo "--- Soil EC Calibration ---"
echo ""
echo "Step 3: LOW EC CALIBRATION (1.413 mS/cm KCl solution)"
echo "  Place sensor in standard KCl solution (1.413 mS/cm)"
echo "  Press Enter when ready..."
read -r

echo "  Reading low EC..."
EC_LOW_ADC=$(mosquitto_sub -h "$MQTT_BROKER" -p "$MQTT_PORT" -t "$TOPIC_READ" -C 1 2>/dev/null | python3 -c "import sys,json; d=json.load(sys.stdin); print(d.get('raw_ec_adc', 1000))" 2>/dev/null || echo "1000")
echo "  Low EC ADC: $EC_LOW_ADC"

echo ""
echo "Step 4: HIGH EC CALIBRATION (2.767 mS/cm KCl solution)"
echo "  Place sensor in standard KCl solution (2.767 mS/cm)"
echo "  Press Enter when ready..."
read -r

echo "  Reading high EC..."
EC_HIGH_ADC=$(mosquitto_sub -h "$MQTT_BROKER" -p "$MQTT_PORT" -t "$TOPIC_READ" -C 1 2>/dev/null | python3 -c "import sys,json; d=json.load(sys.stdin); print(d.get('raw_ec_adc', 2000))" 2>/dev/null || echo "2000")
echo "  High EC ADC: $EC_HIGH_ADC"

# ========== SEND CALIBRATION TO SENSOR ==========

echo ""
echo "Sending calibration constants to sensor #$SENSOR_ID..."

CALIB_PAYLOAD=$(python3 -c "
import json
cal = {
    'sensor_id': $SENSOR_ID,
    'moisture_dry_adc': $DRY_ADC,
    'moisture_wet_adc': $WET_ADC,
    'ec_low_adc': $EC_LOW_ADC,
    'ec_high_adc': $EC_HIGH_ADC,
    'ec_low_ms_cm': 1.413,
    'ec_high_ms_cm': 2.767,
}
print(json.dumps(cal))
")

mosquitto_pub -h "$MQTT_BROKER" -p "$MQTT_PORT" -t "$TOPIC_CMD" -m "$CALIB_PAYLOAD" 2>/dev/null || echo "  ⚠ Could not publish to MQTT (sensor may still work with defaults)"

echo ""
echo "============================================"
echo "  Calibration Complete!"
echo "============================================"
echo ""
echo "  Moisture: 0% (ADC $DRY_ADC) → 100% (ADC $WET_ADC)"
echo "  EC: 1.413 mS/cm (ADC $EC_LOW_ADC) → 2.767 mS/cm (ADC $EC_HIGH_ADC)"
echo ""
echo "Insert sensor into soil and verify readings in the mobile app."