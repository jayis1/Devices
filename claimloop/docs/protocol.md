# Radio protocol and offline policy

Protocol v1 event fields are version (u8), node ID (ASCII, max 23), sequence (u32), kind (u8), signed integer value (i32), and monotonic milliseconds (u32). Wire serialization and byte order are not yet implemented. The API representation is JSON. Reject unknown versions/kinds, invalid ranges, and duplicate `(node_id, sequence)` records. Sequence wrap/reboot persistence is not implemented; nodes should persist sequence state or rotate a provisioned node ID before deployment. Monotonic time is node-local and must not be treated as UTC.

Initial concept uses BLE advertisements for one-way node telemetry; no pairing/security implementation exists, so this is not a trusted operational radio design. The current code provides event-state policy only; transport is an integration requirement. Gateway outages leave paper-based operation; no electronic claim processing occurs. Do not send commands to nodes.
