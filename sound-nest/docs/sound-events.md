# SoundNest Sound Event Classification Catalog

## 40 Sound Classes

The SoundNest TinyML classifier recognizes 40 environmental sound categories organized into 9 groups.

### Alarm Category (0x01-0x05)

| Code | Name | Description | Alert Priority |
|------|------|-------------|----------------|
| 0x01 | Smoke Alarm | Smoke detector beep pattern | Critical |
| 0x02 | CO Alarm | Carbon monoxide detector alarm | Critical |
| 0x03 | Burglar Alarm | Security system alarm | Critical |
| 0x04 | Car Alarm | Vehicle alarm siren | Low |
| 0x05 | Timer Alarm | Kitchen timer, alarm clock | Info |

### Door Category (0x10-0x13)

| Code | Name | Description | Alert Priority |
|------|------|-------------|----------------|
| 0x10 | Doorbell | Doorbell chime | High |
| 0x11 | Door Knock | Knocking on door | Medium |
| 0x12 | Door Open | Door opening | Info |
| 0x13 | Door Close | Door closing | Info |

### Human Category (0x20-0x25)

| Code | Name | Description | Alert Priority |
|------|------|-------------|----------------|
| 0x20 | Speech | Human speech, conversation | Low (triggers privacy mask) |
| 0x21 | Crying Baby | Infant crying | High |
| 0x22 | Cough | Person coughing | Info |
| 0x23 | Sneeze | Person sneezing | Info |
| 0x24 | Laugh | Laughter | Info |
| 0x25 | Shout | Loud shouting | Medium |

### Animal Category (0x30-0x32)

| Code | Name | Description | Alert Priority |
|------|------|-------------|----------------|
| 0x30 | Dog Bark | Dog barking | Medium |
| 0x31 | Cat Meow | Cat vocalization | Info |
| 0x32 | Bird Chirp | Bird song/chirping | Info |

### Kitchen Category (0x40-0x44)

| Code | Name | Description | Alert Priority |
|------|------|-------------|----------------|
| 0x40 | Microwave | Microwave oven running/humming | Info |
| 0x41 | Blender | Electric blender | Info |
| 0x42 | Dishwasher | Dishwasher cycle | Info |
| 0x43 | Kettle | Electric kettle boiling | Info |
| 0x44 | Faucet | Water running from faucet | Info |

### Home Category (0x50-0x56)

| Code | Name | Description | Alert Priority |
|------|------|-------------|----------------|
| 0x50 | Vacuum | Vacuum cleaner | Info |
| 0x51 | Washer | Washing machine cycle | Info |
| 0x52 | Dryer | Tumble dryer | Info |
| 0x53 | Fan | Electric fan | Info |
| 0x54 | AC Unit | Air conditioner compressor | Info |
| 0x55 | TV | Television audio | Low (triggers masking) |
| 0x56 | Music | Music playback | Low (triggers masking) |

### Traffic Category (0x60-0x64)

| Code | Name | Description | Alert Priority |
|------|------|-------------|----------------|
| 0x60 | Car Horn | Vehicle horn | Medium |
| 0x61 | Siren | Emergency vehicle siren | High |
| 0x62 | Engine | Vehicle engine noise | Info |
| 0x63 | Motorcycle | Motorcycle engine | Info |
| 0x64 | Bicycle Bell | Bicycle bell | Info |

### Nature Category (0x70-0x73)

| Code | Name | Description | Alert Priority |
|------|------|-------------|----------------|
| 0x70 | Rain | Rainfall | Info |
| 0x71 | Thunder | Thunderclap | Medium |
| 0x72 | Wind | Wind noise | Info |
| 0x73 | Running Water | Stream/tap water | Info |

### Work Category (0x80-0x82) + Alert Category (0x90-0x92)

| Code | Name | Description | Alert Priority |
|------|------|-------------|----------------|
| 0x80 | Phone Ring | Telephone ringtone | High |
| 0x81 | Notification | Device notification sound | Info |
| 0x82 | Keyboard | Typing on keyboard | Info |
| 0x90 | Glass Break | Breaking glass | Critical |
| 0x91 | Crash | Impact/crash sound | High |
| 0x92 | Gunshot | Gunshot | Critical |

## Special Classes

| Code | Name | Description |
|------|------|-------------|
| 0x00 | Silence | No significant sound detected (< 30 dB(A)) |
| 0xFF | Unknown | Sound detected but class unclear |

## Alert Priority Mapping

| Priority | Level | Haptic Pattern | LED Color | Destinations |
|----------|-------|---------------|-----------|--------------|
| 0 | Info | None | Green flash | App only |
| 1 | Low | 1 short buzz | Green | App + Tag |
| 2 | Medium | 2 short buzzes | Orange | App + Tag + Display |
| 3 | High | 3 buzzes | Yellow flash | App + Tag + Display + Buzzer |
| 4 | Critical | Long buzz | Red flash | All + Phone call |

## Masking Auto-Triggers

| Sound Class | SPL Threshold | Masking Mode | Volume |
|-------------|--------------|--------------|--------|
| Speech | > 65 dBA | Privacy | 60% |
| TV | > 70 dBA | Pink Noise | 50% |
| Music | > 70 dBA | Pink Noise | 50% |
| Dog Bark | > 75 dBA | Brown Noise | 40% |
| Traffic | > 65 dBA | Nature (Rain) | 50% |
| Vacuum | > 70 dBA | Pink Noise | 55% |