# CaptionBridge — Local Meeting Captioning

Status: engineering concept and host-simulated prototype; not a certified assistive-listening or speech-recognition product. Invented and maintained by jayis1.

CaptionBridge provides a meeting organizer with a local-first caption display for small community rooms. Two USB microphone sentinels report audio-level and health metadata to a room hub; the hub accepts a separately supplied transcription result from an operator-controlled speech-to-text adapter and serves live captions to a nearby display/mobile client. No microphone audio is stored or sent by this prototype. This is distinct from EchoSync, which classifies non-speech household alerts and delivers haptics; CaptionBridge is specifically an operator-mediated live speech-caption workflow for meetings.

## System contract

The system helps a meeting host make spoken discussion easier to follow in a room with intermittent connectivity. The hub runs locally, timestamps and sequences caption segments, and serves them over a local API. Sentinel nodes are room-positioned USB microphone interfaces; current firmware models health/level telemetry only, not audio transport or transcription. Caption text enters through an explicit local adapter/operator interface. Cloud sync and ML inference are optional and absent from the runnable prototype. The deterministic fallback is manual typed captions. If the hub or source fails, the display reports stale/unavailable rather than implying complete captions.

```text
Mic Sentinel A -- BLE telemetry --┐
                                  ├-- Room Hub -- local HTTP -- Display / app
Mic Sentinel B -- BLE telemetry --┘                 ^
                                   operator text / future local ASR adapter
```

## Nodes and preliminary hardware

| Node | Prototype selection | Interfaces | Power / role |
|---|---|---|---|
| Room Hub | Raspberry Pi 4 Model B (2 GB), Wi-Fi/Ethernet, USB | USB host, Ethernet/Wi-Fi, local FastAPI | Certified 5 V USB-C supply; local API and session coordination |
| Mic Sentinel ×2 | Raspberry Pi Pico W (RP2040/CYW43439), SPH0645LM4H-B I²S microphone breakout (design proposal) | I²S BCLK GP10, WS GP11, SD GP12; USB 5 V; BLE/Wi-Fi capability depends on SDK | USB powered; telemetry prototype only; audio acquisition not implemented |
| Display/client | Browser on tablet/laptop or React Native operator stub | HTTP on trusted LAN | Existing device; no dedicated display BOM |

Pin assignments are proposals, not validated board schematics. Check Pico W board revision, SDK radio support, microphone breakout voltage/pullups and pin mux against current manufacturer documentation before wiring. The microphone part is a design candidate; the host prototype does not sample it. No mains wiring or audio recording is implemented.

## Message and caption contract

Telemetry v1 JSON is bounded to 512 UTF-8 bytes: `{ "v":1, "node":"mic-a", "seq":4, "uptime_ms":1200, "kind":"mic_health", "audio_peak":0.12, "valid":true }`. `seq` is uint32 and compared with wrap-safe ordering; duplicate or older packets are rejected. Audio peak is normalized 0..1, not an SPL measurement. Caption segments are `{ "session":"room-1", "seq":1, "text":"Welcome", "final":true }`, bounded to 2000 characters. Captions carry no speaker identity. An operator starts/stops a session; sessions are memory-only in the demo and cleared on restart. Text can be sensitive: deploy only on a trusted local network and obtain participant consent.

The prototype does not implement BLE pairing, audio transport, speech recognition, user authentication, cloud MQTT, or encryption. Do not expose it beyond localhost/trusted LAN. A production deployment requires authenticated clients, TLS, session authorization, signed firmware and retention controls. This is not a replacement for qualified interpreters or guaranteed real-time captions.

## Contents

- `firmware/common/protocol.h`: shared telemetry bounds and sequence helper.
- `firmware/mic-sentinel/main.c`, `firmware/hub/main.c`: host-simulatable logic, no board drivers.
- `schematic/README.md`: pin/net intent; no KiCad files or validated electrical schematic are claimed.
- `hardware/bom/`: preliminary node-level BOMs.
- `software/dashboard/`: local FastAPI caption session API with tests.
- `software/ml-pipeline/`: explicitly synthetic word-error-rate evaluation harness; no trained model.
- `software/mobile-app/`: React Native display/operator stub.
- `docs/`: architecture, API and privacy/failure notes.
- `scripts/`: local development runner.

## Run and verification

From `software/dashboard`: `python -m pip install -r requirements.txt && pytest -q`. Host C checks: `cc -std=c11 -Wall -Wextra -Werror firmware/mic-sentinel/main.c -o /tmp/caption-bridge-sentinel` and similarly for `firmware/hub/main.c`. Hardware capture, BLE, ASR, KiCad/ERC, mobile build and field usability are not implemented or tested. No accuracy or accessibility-compliance claims are made.

## Safety and limitations

Microphone placement and room acoustics affect intelligibility; a health heartbeat is not evidence that a microphone hears all speakers. The host should verify caption completeness and provide a manual fallback. No raw audio capture, recording, speaker recognition, automated meeting minutes, or cloud upload is part of this prototype. Physical privacy indicators and consent workflow are required before any audio capture is added. Invented and maintained by jayis1.
