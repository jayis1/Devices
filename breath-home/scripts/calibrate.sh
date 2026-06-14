#!/bin/bash
# BreathHome - Sensor Calibration Script
# Calibrates room sensor nodes against reference instruments.
# Run this when installing a new sensor node or after replacing sensors.

set -e

echo "========================================="
echo "  BreathHome Sensor Calibration"
echo "========================================="
echo ""
echo "This script calibrates room sensor nodes against"
echo "reference instruments. You will need:"
echo "  - Reference PM2.5 monitor (e.g., TSI 8530)"
echo "  - Reference CO2 meter (e.g., Telaire 7001)"
echo "  - Reference thermometer/hygrometer"
echo "  - Known formaldehyde source"
echo ""
echo "Make sure sensors are powered on and warmed up for at least"
echo "5 minutes (SPS30 fan) / 2 minutes (SCD41) before calibrating."
echo ""

# Configuration
MQTT_HOST=${MQTT_HOST:-"breathhome.local"}
MQTT_PORT=${MQTT_PORT:-1883}
MQTT_USER=${MQTT_USER:-"breathhome"}
MQTT_PASS=${MQTT_PASS:-"breathhome_secret_2026"}

# Check dependencies
command -v mosquitto_pub >/dev/null 2>&1 || { echo "Error: mosquitto_pub not found (install mosquitto-clients)"; exit 1; }

echo "Step 1: Discover sensor nodes"
echo "-------------------------------"
echo "Sending discovery command to all nodes..."
mosquitto_pub -h $MQTT_HOST -p $MQTT_PORT -u $MQTT_USER -P $MQTT_PASS \
  -t "breathhome/calibration/discover" -m '{"command":"discover"}'

echo ""
echo "Enter the Node ID of the sensor you want to calibrate:"
read -r NODE_ID

echo ""
echo "Calibrating Node ID: $NODE_ID"
echo ""

# SPS30 particulate sensor calibration
echo "Step 2: SPS30 Particulate Sensor Calibration"
echo "----------------------------------------------"
echo "Place the reference PM2.5 monitor next to the sensor node."
echo "Wait 2 minutes for readings to stabilize."
echo ""
echo "Enter the reference PM2.5 reading (μg/m³):"
read -r REF_PM25

mosquitto_pub -h $MQTT_HOST -p $MQTT_PORT -u $MQTT_USER -P $MQTT_PASS \
  -t "breathhome/calibration/$NODE_ID" -m "{\"command\":\"calibrate\",\"sensor\":\"sps30\",\"parameter\":\"pm25\",\"reference\":$REF_PM25}"

echo "PM2.5 calibration offset sent."

# SCD41 CO2 sensor calibration
echo ""
echo "Step 3: SCD41 CO2 Sensor Calibration"
echo "--------------------------------------"
echo "Place the reference CO2 meter next to the sensor node."
echo "For best results, calibrate in well-ventilated outdoor air (≈420 ppm)."
echo ""
echo "Enter the reference CO2 reading (ppm):"
read -r REF_CO2

echo "Choose calibration method:"
echo "  1) Forced calibration (set current reading to known value)"
echo "  2) ASC reset (reset automatic self-calibration baseline)"
read -r CO2_METHOD

if [ "$CO2_METHOD" = "1" ]; then
  echo "Performing forced calibration at $REF_CO2 ppm..."
  mosquitto_pub -h $MQTT_HOST -p $MQTT_PORT -u $MQTT_USER -P $MQTT_PASS \
    -t "breathhome/calibration/$NODE_ID" -m "{\"command\":\"calibrate\",\"sensor\":\"scd41\",\"parameter\":\"co2\",\"method\":\"frc\",\"reference\":$REF_CO2}"
elif [ "$CO2_METHOD" = "2" ]; then
  echo "Resetting ASC baseline..."
  mosquitto_pub -h $MQTT_HOST -p $MQTT_PORT -u $MQTT_USER -P $MQTT_PASS \
    -t "breathhome/calibration/$NODE_ID" -m "{\"command\":\"calibrate\",\"sensor\":\"scd41\",\"parameter\":\"co2\",\"method\":\"asc_reset\"}"
fi

echo "CO2 calibration command sent."

# BME688 temperature/humidity calibration
echo ""
echo "Step 4: BME688 Temperature/Humidity Calibration"
echo "------------------------------------------------"
echo "Place the reference thermometer/hygrometer next to the sensor."
echo "Wait 5 minutes for thermal equilibrium."
echo ""
echo "Enter the reference temperature (°C):"
read -r REF_TEMP
echo "Enter the reference humidity (%RH):"
read -r REF_RH

mosquitto_pub -h $MQTT_HOST -p $MQTT_PORT -u $MQTT_USER -P $MQTT_PASS \
  -t "breathhome/calibration/$NODE_ID" -m "{\"command\":\"calibrate\",\"sensor\":\"bme688\",\"parameter\":\"temp_rh\",\"reference_temp\":$REF_TEMP,\"reference_rh\":$REF_RH}"

echo "Temperature/humidity calibration offsets sent."

# SGP41 VOC/NOx sensor calibration
echo ""
echo "Step 5: SGP41 VOC/NOx Sensor Calibration"
echo "------------------------------------------"
echo "The SGP41 uses automatic baseline compensation."
echo "For best results, run the sensor in clean outdoor air for 30 minutes."
echo ""
echo "To reset the VOC baseline:"
read -p "Reset SGP41 baseline? (y/N): " RESET_SGP41

if [ "$RESET_SGP41" = "y" ] || [ "$RESET_SGP41" = "Y" ]; then
  mosquitto_pub -h $MQTT_HOST -p $MQTT_PORT -u $MQTT_USER -P $MQTT_PASS \
    -t "breathhome/calibration/$NODE_ID" -m "{\"command\":\"calibrate\",\"sensor\":\"sgp41\",\"method\":\"baseline_reset\"}"
  echo "SGP41 baseline reset sent. Wait 64 readings (≈5 minutes) for re-conditioning."
fi

echo ""
echo "========================================="
echo "  Calibration Complete!"
echo "========================================="
echo ""
echo "  Node ID: $NODE_ID"
echo "  PM2.5 reference: $REF_PM25 μg/m³"
echo "  CO2 reference: $REF_CO2 ppm"
echo "  Temperature reference: $REF_TEMP °C"
echo "  Humidity reference: $REF_RH %RH"
echo ""
echo "  Calibration data has been stored on the sensor node"
echo "  and will persist across reboots."
echo ""
echo "  Verify calibration by monitoring the sensor readings"
echo "  in the BreathHome dashboard for the next 30 minutes."
echo ""