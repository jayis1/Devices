# Mobile API stub

React Native client contract only; no native app scaffold or production authentication is included. POST `/v1/telemetry` with the v1 JSON schema; GET `/v1/sites/{site_id}/latest` returns samples. Configure TLS endpoint and user/device authorization before deployment. Show stale timestamps explicitly; never infer live status from cached values.
