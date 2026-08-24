# FoodAllergySync Architecture

## Design goals

1. Prevent common ingredient and cross-contact mistakes before exposure.
2. Preserve a verifiable chain of custody for packed meals.
3. Keep rescue medication accessible and environmentally safe.
4. Operate locally during internet outages.
5. Share a single family-safe state across caregivers.

## Data flow

1. Nodes publish encrypted telemetry to the hub over Sub-GHz TDMA.
2. Hub normalizes packets into MQTT topics:
   - `foodallergy/<home_id>/meal-scanner/events`
   - `foodallergy/<home_id>/strip-reader/results`
   - `foodallergy/<home_id>/safelunch/telemetry`
   - `foodallergy/<home_id>/epipen/readiness`
3. FastAPI persists the latest state to PostgreSQL and emits websocket updates.
4. ML jobs enrich events with risk scores and coaching actions.
5. Mobile app subscribes to websocket snapshots and renders alerts.

## Control loops

### Ingredient loop
- Trigger: barcode scan or OCR capture
- Edge step: image crop + ingredient token extraction
- Cloud/edge step: allergen parser + policy match
- Action: green / amber / red guidance

### Cross-contact loop
- Trigger: strip insertion
- Edge step: optical acquisition
- ML step: StripQuant + CrossContact inference
- Action: safe / re-clean / isolate kitchen tools

### Lunch integrity loop
- Trigger: scheduled departure + lunchbox close
- Edge step: NFC safe-meal verification
- ML step: LunchSafe forecast
- Action: pack reminder, swap alert, spoilage warning

### Readiness loop
- Trigger: daily departure routine
- Edge step: injector presence + temp check
- ML step: ReadyGuard risk
- Action: nudged reminder or urgent alarm

## Threat model

- Prevent raw audio/video upload by default.
- Use rotating session nonces on radio packets.
- Keep child profiles isolated by role-based access.
- Require explicit caregiver invitation for profile sharing.
