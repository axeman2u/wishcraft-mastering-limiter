# Wishcraft Mastering Limiter — Behavior Spec (Reference for JUCE Port)

This is the source of truth for "did the port stay faithful." Keep this file in the project
folder and point Claude Code at it in every session — if a translated behavior doesn't match
something on this page, that's a bug, not a stylistic difference.

## Signal Path Order (do not reorder)
Selective Clipper → Input Gain (Drive) → Limiter → Safety Clip → Output
Input Gain sits **after** the Selective Clipper and **before** the Limiter — it's the "drive
into the limiter" control, not a plain input trim.

## Fixed Constants — NOT exposed as parameters
These were settled by ear and deliberately removed from the slider list. The JUCE port must
NOT turn these back into user-facing parameters:
- `CHAR_MAX_MS = 5.0` — Selective Clipper excursion window
- `DELTA_GAIN_DB = 6.0` — Delta listen mode gain compensation

## Selective Clipper (formerly "Character")
- Duration-gated pre-clip stage with sidechain-filtered detection
- Auto Makeup Gain: **permanently on**, not a switch
- Delta Listen Mode compares against the GAINED dry signal (post-Input-Gain reference), and
  respects whatever the Selectivity slider is actually set to — it must NOT force 100%/Aggressive
- Dry-path buffers must be delay-matched sample-for-sample to the wet path, or the delta
  comparison is meaningless (this was a previously-fixed bug — don't reintroduce it)

## Limiter Core
- Lookahead + program-dependent release using a **two-time-constant history blend** (not a
  single-time-constant release)
- Limiter Auto Gain: **user-toggleable** (unlike Selective Clipper's Auto Gain), and when on,
  capped by `min(Ceiling, True Peak Output Ceiling)` — it must never restore level past the
  hard safety limit
- Stereo Link: blended **after** smoothing, using **min-gain-wins** logic

## Sidechain EQ
- Detector-only (does not touch the audio path) — low/high shelf, fixed 300 Hz pivot
- Separate filter instances for the Selective Clipper's detector and the Limiter's detector —
  they must not share state

## Safety / Output
- Two-stage true-peak-aware clip: oversampled-domain stage + a post-downsample backstop
- Click-free parameter smoothing uses **one shared time constant** across all gain-type controls
- Denormal protection on every recursive state value
- Bypass is latency-compensated, has its own slider, and priority order is
  **Bypass > Delta > Normal**

## Metering (informational, but ballistics matter for feel)
- GR, True Peak, and Selective Clip Activity each get live ballistics plus a **1-second
  peak-hold with 20 dB/sec fall**
- Text readouts show the **held** value, not the live one
- True Peak has a two-tier Over indicator: yellow past Output Ceiling (accepted mastering-level
  clipping), red past true 0 dBTP (hard digital clipping) — both checked against the
  **peak-hold** value, not live
- Dynamic Range (combined L/R LRA-style min/max tracking) and short-term LUFS are both
  additional readouts, not gain-affecting — safe to stage late

## GUI Naming (current, post-rename)
- Group labels: **GAIN**, **SELECTIVE CLIPPER**, **SIDECHAIN EQ**, **LIMITER**, **UTILITY**,
  and meter columns **SELECTIVE CLIP**, **GAIN REDUCTION**, **OUTPUT**
- Listen Mode options: "Normal" / "Delta (what was removed)"
- Do not resurrect the old "Character" label anywhere in the UI

## CPU Optimization (already in the JSFX — preserve the intent, not necessarily the exact trick)
The current file conditionally computes the dry-path FIR downsampling only when needed
(gained-dry FIR only during Delta, raw-dry FIR only during Bypass) while keeping buffers always
filled so mode switches stay seamless with no stutter. The JUCE port doesn't need to replicate
this exact micro-optimization, but the **audio, latency, and feature behavior must stay
identical** regardless of which optimization strategy the JUCE version uses internally.

## Null-Test Checklist (run at every stage boundary)
- [ ] Identical latency reported to host at every oversampling factor
- [ ] Identical output with all processing at unity/off settings
- [ ] Identical Selective Clipper output in isolation (Limiter bypassed)
- [ ] Identical Limiter output in isolation (Selective Clipper bypassed)
- [ ] Bypass output matches raw input exactly (accounting for PDC)
- [ ] Delta mode output matches JSFX Delta mode on the same material
