# StepWise architecture

Two manual observation beacons publish status to one local gateway. Only deliberate button submissions produce reports; no inference about occupant behavior. Radio loss leaves the last report visible with its age, never as a fresh state. Gateway restart recovers SQLite records. Duplicate event IDs are idempotent; sequence regressions are rejected. No automatic actions exist. Manual inspection overrides electronic status. Data contains node ID, status enum, sequence and receipt timestamp; retain locally for a configurable short period (prototype target 30 days), then delete. Backups must follow the same deletion policy.

Power: each beacon USB 5 V; hub uses the Pi vendor-recommended regulated supply. No battery or mains interface. This is a user research prototype, not a product.
