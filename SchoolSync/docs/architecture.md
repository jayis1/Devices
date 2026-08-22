# SchoolSync Architecture

## Design goals

1. Reduce forgotten items without requiring cameras or constant manual checklist maintenance.
2. Detect lateness risk early enough to change outcomes.
3. Verify physical handoffs at doorway and transit stages.
4. Provide low-friction support for families with ADHD, autism, split custody, and high schedule variability.

## Event flow

1. Hub loads today's schedule and derives required-item set.
2. Lunchbox Dock reports packed/not-packed and thermal forecast.
3. Backpack Tag reports carry state, pocket activity, and acknowledgment events.
4. Doorway Sentinel confirms bag+lunch exiting together and triggers final reminder if needed.
5. Transit Beacon confirms correct vehicle/route handoff and sends fallback alerts over LTE if necessary.

## Timing windows

- T-60 to T-20 min: gentle reminders, weather and schedule prompts
- T-20 to T-5 min: checklist validation, readiness score updates every minute
- T-5 to T+10 min: high-confidence doorway and handoff verification, lateness escalation
- Return-home window: arrival confirmation and lunchbox return detection

## Power and resilience

- Hub UPS maintains schedule logic during short outages.
- Critical alerts can route through Transit Beacon LTE if home network fails.
- Backpack Tag supports 5+ days typical battery life with UWB duty-cycling.

## Data model

- child_profile
- schedule_event
- routine_event
- node_health
- lunch_forecast
- transit_handoff
- alert_event
