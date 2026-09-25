# Architecture

TactileSync is local-first. The hub maintains a commissioned-node registry, event policy, tactile-pattern dictionary, and bounded event queue. A wearer’s Haptic Band receives semantic pattern IDs, not text, room names, or raw UWB data. Room Anchors contribute distance observations. The policy engine converts a stable zone estimate or node event into an opt-in notification.

## Failure behavior
- Unknown, replayed, invalid-version, or invalid-authentication frames are dropped.
- If UWB confidence is low, the hub withholds zone notifications rather than guessing.
- Battery-low events are distinct from household events.
- Internet loss leaves local BLE/UWB event handling available; cloud history is optional.
- A long press on the band enters quiet mode; physical commissioning is required to add a node.

## Data retention
Default local event retention is 24 hours; range diagnostics are retained for 15 minutes. Exports require an explicit local action. No raw audio, video, or speech is collected.
