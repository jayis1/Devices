# SoundNest Acoustic Primer

## Sound Fundamentals for SoundNest Users

### Sound Pressure Level (SPL)

Sound is measured in decibels (dB), a logarithmic scale:

| Sound | Level (dB) | SoundNest Alert |
|-------|-----------|-----------------|
| Threshold of hearing | 0 | — |
| Whisper | 20-30 | — |
| Quiet library | 40 | Quiet environment |
| Normal conversation | 50-65 | — |
| Traffic (inside car) | 70-85 | — |
| Vacuum cleaner | 70-80 | Auto-masking trigger |
| Lawn mower | 85-95 | Dose warning |
| Nightclub | 95-110 | Dose danger |
| Jackhammer | 100-110 | Critical dose |
| Siren (nearby) | 110-120 | Critical alert |
| Pain threshold | 130 | Critical alert |

### A-Weighting vs C-Weighting vs Z-Weighting

SoundNest measures sound using three weighting curves:

- **A-weighting (dB(A))**: Approximates human hearing. Emphasizes 1-6kHz, de-emphasizes bass and treble. Used for dose calculations and most display values.
- **C-weighting (dB(C))**: Flatter response, attenuates only very low and very high frequencies. Used for peak measurements and bass-heavy sounds.
- **Z-weighting (dB(Z))**: Flat/no weighting. Represents actual sound pressure. Used for spectral analysis.

### Sound Dose

Sound dose measures cumulative noise exposure:

- **Reference level**: 85 dB(A) for 8 hours = 100% daily dose
- **Exchange rate**: 3 dB (ISO standard) — every 3 dB increase halves the allowed time
- **Example**: 88 dB(A) for 4 hours = 100% dose (same as 85 dB(A) for 8 hours)

SoundNest tracks your daily dose continuously and alerts you at:
- 50%: Yellow warning (consider reducing exposure)
- 100%: Orange warning (you've reached the daily limit)
- 200%: Red critical (hearing protection recommended)

### Tinnitus

Tinnitus affects 50M Americans. SoundNest provides personalized masking:

1. **Audiometric test**: Match your tinnitus frequency (typically 2-10kHz)
2. **Masking theory**: Narrowband noise 1 octave below tinnitus frequency
3. **Adaptive level**: Masking volume adjusts based on ambient noise
4. **Sleep mode**: Gradual 30-minute fade-out for bedtime

### Sound Masking

Masking works by raising the noise floor so intrusive sounds are less noticeable:

- **White noise**: Equal energy per frequency band. Good for general masking.
- **Pink noise**: Equal energy per octave. More pleasant than white noise. Best for speech masking.
- **Brown noise**: Deeper, rumbling sound. Excellent for sleep.
- **Nature sounds**: Most pleasant, but less effective at masking speech.

### Directional Masking

SoundNest's Smart Masking Speakers use a parabolic reflector to create a directional sound cone (±30°). This allows:

- Targeted masking at a window while keeping the rest of the room quiet
- Privacy masking that prevents eavesdropping in adjacent rooms
- Focused tinnitus masking at your bedside without disturbing others