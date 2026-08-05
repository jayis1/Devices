# SeizureSync — Clinical Documentation

## Clinical rationale

Epilepsy affects 65 million people worldwide. Sudden Unexpected Death in
Epilepsy (SUDEP) kills 1 in 1,000 adults with epilepsy per year — over
24,000 deaths annually — and is the leading epilepsy-related cause of
death in young adults. No existing consumer device addresses seizure
detection + SUDEP prevention + epilepsy management holistically.

SeizureSync is designed to:
1. **Detect seizures** in real time (95% sensitivity, 0.21 FP/day)
2. **Predict seizures** 5-8 minutes before onset (73% recall)
3. **Prevent SUDEP** via nocturnal apnea + prone position monitoring
4. **Alert caregivers** via Sub-GHz mesh (no internet required)
5. **Auto-dispatch 911** if caregiver doesn't respond (Twilio)
6. **Identify triggers** via personalized XGBoost + SHAP attribution
7. **Forecast risk** 24 hours ahead (LSTM)
8. **Generate neurologist reports** (HIPAA-compliant PDF)

## ILAE 2017 seizure classification

SeizureSync classifies detected seizures per the ILAE 2017 operational
classification:

| Class | Code | Description |
|---|---|---|
| Focal aware | 1 | Simple partial — consciousness preserved |
| Focal impaired | 2 | Complex partial — consciousness impaired |
| Focal-to-bilateral tonic-clonic | 3 | Secondary generalization |
| Generalized tonic-clonic | 4 | Primary generalized |
| Myoclonic | 5 | Brief jerks |
| Atonic | 6 | Loss of muscle tone (drop attacks) |
| Absence | 7 | Brief staring spells |

## SUDEP risk factors

SeizureSync monitors the following SUDEP risk factors (per MORTEMUS study):

1. **Nocturnal apnea density** — apnea events per night
2. **Prone position** — face-down sleeping (major SUDEP risk factor)
3. **Seizure frequency** — >3/month increases risk 3×
4. **Medication adherence** — <80% increases risk 1.8×

Annual SUDEP risk is computed via Bayesian logistic regression:
```
risk = σ(β₀ + β₁·seizure_freq + β₂·apnea_density + β₃·prone_eps + β₄·adherence)
```

## Seizure first aid (per seizure type)

The mobile app displays seizure-specific first aid instructions:

### Focal aware
- Stay calm, stay with the person
- Guide away from hazards
- Do not restrain
- Time the episode

### Focal impaired
- Stay with person, guide from hazards
- Do not restrain
- Time the episode
- Be reassuring during recovery

### Focal-to-bilateral tonic-clonic / Generalized tonic-clonic
- **Time the seizure**
- Protect from injury (cushion head, remove glasses)
- Do NOT restrain
- Do NOT put anything in mouth
- After: roll onto side (recovery position), check breathing
- Call 911 if: seizure > 5 min, no recovery, injury, difficulty breathing,
  first seizure, pregnancy

### Absence
- Stay calm, gently guide from hazards
- Time the episode
- Wait for full awareness to return

## Clinical validation targets

| Metric | Target | Method |
|---|---|---|
| Seizure detection sensitivity | 95% | EPILEPSIAE wrist-worn benchmark |
| False alarm rate | <0.21/day | Same |
| Pre-ictal recall | 73% | IEEG.org ECoG-autonomic paired |
| Pre-ictal lead time | 5-8 min | Same |
| SUDEP apnea detection | 88% | MORTEMUS-derived |
| ILAE classification accuracy | 89% | SemiologyNet validation |
| SUDEP risk calibration | ECE < 5% | Bayesian logistic regression |

## HIPAA compliance

- All PHI encrypted at rest (AES-256) and in transit (TLS 1.2+)
- BAA required with cloud hosting provider
- Audit logging for all data access
- Patient consent for data use (ML training opt-in)
- Right to deletion (GDPR + HIPAA)
- Minimum necessary data sharing
- Neurologist reports are password-protected PDFs

## Contraindications

- SeizureSync is a **monitoring** device, not a treatment
- Does NOT replace medical supervision or AED medication
- Does NOT detect all seizure types (e.g., subtle absence may be missed)
- Skin-sensitive individuals may react to AuraPatch adhesive
- Not for patients with pacemakers (EDA galvanic signal — consult cardiologist)