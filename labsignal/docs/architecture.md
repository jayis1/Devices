# Architecture

LabSignal separates gateway placement, instrument observation, and consumable stock context. Nodes emit low-rate events over a regional Sub-GHz link. Gateway provides local display/cache and optional outbound-only sync. No actuator or student-facing identity exists. Manual calibration records remain authoritative; sensor signals are context only.

Failure states: sensor/radio stale => UNKNOWN; gateway/cloud outage => local operation or paper fallback; power loss => nodes silent, no physical process changed; corrupt/version mismatch => discard frame; duplicate sequence => ignore. Local TTL target 15 minutes for live status and 7 days for audit history. Provisioning/authentication and signed OTA are future work and must be completed before untrusted deployment.

Electrical and RF designs are conceptual. Confirm board pin mappings, SX1262 regional module approvals, current peaks, antenna clearance, ESD, and input front-end with datasheets and bench validation. Do not connect directly to mains or safety-critical instruments.
