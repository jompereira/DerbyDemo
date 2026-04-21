# Vehicle Tuning Notes

## Lateral Slip Graph — Sports Car Rear Wheels

**File:** `Source/DerbyDemo/SportsCar/DerbyDemoSportsWheelRear.cpp`

### Problem

Vehicle lost traction uncontrollably. Two root causes:

1. **Front/rear `FrictionForceMultiplier` imbalance** — front was `3.0`, rear was `1.6`. The large gap creates a heavy oversteer bias where the rear breaks loose far earlier than the front.

2. **Aggressive slip curve** — the original curve dropped 50% at 20° slip, which meant once the rear stepped out past ~8° it lost grip fast and could not self-correct.

3. **C++ constructor overrides editor** — `UChaosVehicleWheel` constructor values override anything set in the Blueprint/Details panel at PIE startup. Edit the curve in C++, not the editor.

### Lateral Slip Graph — How It Works

The `LateralSlipGraph` maps lateral slip angle (X, degrees) to a friction force multiplier (Y, 0–1). It feeds into the final lateral force alongside `FrictionForceMultiplier`.

- **X axis:** slip angle in degrees
- **Y axis:** friction multiplier (0 = no grip, 1 = full grip at that slip angle)
- A single key produces a flat constant — slip angle has no effect
- Y values above 1 are out of scale and can cause physics instability

### Changes Made

#### `FrictionForceMultiplier`

| | Before | After |
|---|---|---|
| Front | 3.0 | 3.0 (unchanged) |
| Rear | 1.6 | 2.2 |

#### Lateral Slip Curve (Rear)

| Slip Angle | Before (Y) | After (Y) |
|---|---|---|
| 0° | 0.0 | 0.0 |
| 6°–8° | 1.0 (peak at 8°) | 1.0 (peak at 6°) |
| 20° | 0.5 | 0.8 |
| 40° | 0.35 | 0.65 |

Peak moved from 8° to 6° for a more responsive feel. Falloff softened so the rear can self-correct rather than spinning out once it steps past peak.

### Tuning Reference

- **More oversteer:** lower rear `FrictionForceMultiplier`, steepen curve drop after peak
- **More understeer:** raise rear multiplier toward front value, flatten curve drop
- **Snap oversteer (drifty):** sharp drop at 15–20° (e.g. 0.5), low rear multiplier
- **Neutral/balanced:** rear multiplier ~80–90% of front, gentle slope to ~0.7 at 40°
