# MobilitySync Architecture

## Design principles

1. **Camera-free privacy** for bedrooms, bathrooms, and daily living spaces.
2. **Intervene before a fall** by acting on transfer quality, walker slip, and fatigue accumulation.
3. **Graceful degradation** so core door assist and walker safety continue when internet is unavailable.
4. **Caregiver visibility without micromanagement** using concise scores and event summaries.

## Data flow

- Fixed nodes publish telemetry over 868 MHz TDMA to the hub.
- Walker and wearable exchange low-latency BLE cues for haptics and readiness.
- UWB ranging between walker / doorway / hub enables approach timing and room localization.
- Hub computes policy, publishes commands over MQTT, and syncs cloud summaries.

## Control loops

### Transfer loop
- Trigger: occupied mat with stand intent.
- Inputs: asymmetry, lean progression, unload rate, wearable HR/HRV.
- Outputs: haptic pause cue, walker brake hold, caregiver escalation on repeated failures.

### Walker loop
- Trigger: handle grip or wheel motion.
- Inputs: handle forces, IMU, wheel encoders, obstacle range.
- Outputs: brake pulse, navigation cue, threshold crossing mode.

### Door assist loop
- Trigger: UWB approach + route prediction.
- Inputs: walker position, door mode, obstruction sensors, time of day.
- Outputs: unlock/open/hold/close commands with safety interlocks.
