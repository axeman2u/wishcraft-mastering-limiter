# Wishcraft Mastering Limiter — Behavior Spec (Reference for JUCE Port)

This is the source of truth for "did the port stay faithful." Keep this file in the project
folder and point Claude Code at it in every session — if a translated behavior doesn't match
something on this page, that's a bug, not a stylistic difference.

## Signal Path Order (do not reorder)
Selective Clipper → Input Gain (Drive) → Limiter → True Peak Limiter → Safety Clip → Output
Input Gain sits **after** the Selective Clipper and **before** the Limiter — it's the "drive
into the limiter" control, not a plain input trim.

## Fixed Constants — NOT exposed as parameters
These were settled by ear and deliberately removed from the slider list. The JUCE port must
NOT turn these back into user-facing parameters:
- `CHAR_MAX_MS = 5.0` — Selective Clipper excursion window

## Selective Clipper (formerly "Character")
- Duration-gated pre-clip stage with sidechain-filtered detection
- Auto Makeup Gain: **permanently on**, not a switch
- Delta Listen Mode compares against the GAINED dry signal (post-Input-Gain reference), and
  respects whatever the Selectivity slider is actually set to — it must NOT force 100%/Aggressive
- Dry-path buffers must be delay-matched sample-for-sample to the wet path, or the delta
  comparison is meaningless (this was a previously-fixed bug — don't reintroduce it)
- `DELTA_GAIN_DB = 6.0` — fixed base gain compensation for Delta's raw (typically very
  quiet) difference signal. **No longer un-adjustable**: aggressive clipping made this
  boost startlingly loud, so a **Delta Trim** knob (`delta_trim_db`, not a JSFX slider,
  ±12 dB, default 0.0 dB) was added on top of it per explicit user request, added to
  DELTA_GAIN_DB in dB before conversion to linear gain. 0.0 dB reproduces the original
  fixed-6dB behavior exactly. Only visible in the GUI while Delta Listen Mode is on.
- The Output Peak meter/Over indicators MUST track the actual gained Delta signal
  (`(dryGainedDelayed - charOnlyDelayed) * deltaGainLin`, i.e. the same math the final
  downsampled output uses, evaluated at oversampled resolution) while Delta Listen Mode
  is active — NOT the raw unsubtracted/unboosted charOnly reference. An earlier version
  metered the raw reference, which never moved with DELTA_GAIN_DB or Delta Trim at all,
  so the meter stayed pinned even as the actual audible level changed by 10+ dB (a
  previously-fixed bug — don't reintroduce it).

## Limiter Core
- Lookahead + program-dependent release using a **two-time-constant history blend** (not a
  single-time-constant release)
- Displayed as **"Threshold"** in the GUI (param ID stays `ceiling_db`, unchanged for
  automation/state compatibility) — it's a smoothed target the signal can transiently
  overshoot, not a hard-enforced ceiling. True-peak safety is the True Peak Limiter's job,
  not this control's.
- Default **-0.1 dB** (changed from the JSFX's -1.0 dB), matching TP Limit's default
  below — per user testing across varying-density/dynamic-range material, this pairing
  rendered true peak consistently between -1 and -0.5 dBTP.
- Limiter Auto Gain: **user-toggleable** (unlike Selective Clipper's Auto Gain), **strictly
  cut-only** — fixed 0.0 dB cap, decoupled from any peak-ceiling formula. It must never
  restore level above unity, the way a compressor's makeup gain might.
- Stereo Link: blended **after** smoothing, using **min-gain-wins** logic

## True Peak Limiter
- New stage, inserted **between the Limiter and Safety Clip**, using **smooth gain
  reduction** rather than hard clipping — duplicates the effect of cascading a dedicated
  true-peak "catch" limiter plugin after this one
- Runs in the oversampled domain; reuses the Limiter's own lookahead-delay-and-apply
  pattern (fast one-pole gain-reduction envelope, applied to audio delayed by a short
  lookahead buffer) rather than scanning a peak window. The level fed into that envelope
  comes from a fast peak-and-hold follower (not a raw instantaneous sample read, which
  breaks down for sustained near-Nyquist content — see TruePeakLimiter.h)
- Always fully linked (shared gain = min of the two channels' required gain) — a
  safety/backstop stage, not a creative stereo tool
- Fixed internal lookahead/attack/release, not user-exposed (the Limiter's own
  Threshold/Release/Link already cover the musical/creative case)
- Targets the **TP Limit** control (`output_ceiling_db`, the repurposed former "Out
  Ceiling"/"Peak Ceiling" knob), adjustable up to 0.0 dB. Default **-0.1 dB** (changed
  from the JSFX's -1.0 dB) — see Limiter Core's matching Threshold default note above
- Includes a small **internal** reconstruction-margin constant (`reconstructionMarginDb`,
  0.6 dB — not user-facing) so residual detection error can't push the final
  reconstructed peak past the displayed TP Limit value on realistic content
- **Scope of the guarantee**: verified via an offline stress harness covering TWO
  separate, confirmed error sources (both folded into `reconstructionMarginDb`, currently
  2.0 dB):
  1. Near-Nyquist tone detection error — a sustained, full-scale tone parked within
     ~2-4% of Nyquist (the canonical true-peak-meter torture signal, not something real
     mastering material approaches) can still exceed TP Limit by a couple of dB; a known,
     industry-documented limitation of oversampled true-peak detection (ITU-R BS.1770's
     own spec documents a comparable-order-of-magnitude ~0.6 dB worst-case bound for the
     same class of signal).
  2. Decimation growth on rapidly gain-modulated broadband/transient content — confirmed
     via a repeated-transient stress test that TruePeakLimiter's own oversampled-domain
     output measures cleanly, but downsampling that exact output back to base rate
     reintroduces real growth (a plain unprocessed passthrough of the same broadband
     content round-trips with negligible error, so this is specifically provoked by
     multiplying broadband content by a fast-changing gain). This is the practically
     larger of the two, and drives most of the current margin. The current margin was
     calibrated against a real user report (a dense, loud mix through TP Limit -1.0 dB
     measuring +0.1 dBTP) rather than the single most adversarial synthetic case found
     during testing, which needed ~3.0 dB to fully close — real transient-heavy masters
     should be re-verified against a true-peak meter after mastering, same as with any
     true-peak limiter.
- Adds its own lookahead to the reported plugin latency (PDC), same as the Limiter's

## Sidechain EQ
- Detector-only (does not touch the audio path) — low/high shelf, fixed 300 Hz pivot
- Separate filter instances for the Selective Clipper's detector and the Limiter's detector —
  they must not share state

## Safety / Output
- Safety Clip is a **fixed 0.0 dBFS hard backstop**, fully decoupled from
  `output_ceiling_db`/the old `ISP_MARGIN_DB` formula — the True Peak Limiter now does the
  real true-peak-safety work; Safety Clip only exists to stop literal digital-domain overs
  and should trigger rarely
- Two-stage clip: oversampled-domain stage + a post-downsample backstop (unchanged
  placement, just a fixed target now instead of a derived one)
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
- Group labels: **GAIN**, **SELECTIVE CLIPPER**, **SHAPING EQ**, **LIMITER**, **UTILITY**,
  and meter columns **SELECTIVE CLIP**, **GAIN REDUCTION**, **OUTPUT**
- Listen Mode options: "Normal" / "Delta (what was removed)"
- **Delta Trim** knob in SELECTIVE CLIPPER: only visible while Delta Listen Mode is on
- Oversampling Factor (`os_choice`): three options, **2x** / **4x** / **8x**
- Do not resurrect the old "Character" label anywhere in the UI
- "SHAPING EQ" (renamed from "SIDECHAIN EQ"): the internal DSP concept is still
  detector filtering ("sidechain EQ" in the underlying Selective Clipper/Limiter
  code, `sc_low_shelf_db`/`sc_high_shelf_db`), but the GUI label was changed because
  "sidechain" implies compressor-style detector-only/inaudible behavior, and that's
  only true for the Selective Clipper's copy -- the Limiter's copy is NOT
  detector-only (see `## Limiter Core`) and audibly colors the output, matching the
  JSFX exactly. Do not resurrect "SIDECHAIN EQ" as the GUI label.
- "Threshold" (renamed from "Ceiling", the Limiter's own `ceiling_db`): a smoothed target
  the signal can transiently overshoot, not a hard-enforced ceiling -- do not resurrect
  "Ceiling" as this control's GUI label, since a real ceiling now exists elsewhere.
- "TP Limit" (renamed from "Out Ceiling"/"Peak Ceiling", `output_ceiling_db`): now drives
  the True Peak Limiter's genuine true-peak guarantee, not a name earlier stages
  couldn't back up. Do not resurrect "Peak Ceiling" or "Out Ceiling" as this control's
  GUI label.

## Manual (PDF) Distribution
- The installer copies the manual PDF directly alongside the plugin binaries themselves
  (macOS: `/Library/Audio/Plug-Ins/VST3/` and `.../Components/`; Windows: the shared
  `Common Files\VST3\` folder next to the .vst3) — NOT to a separate /Applications
  folder or Program Files subfolder a user has to remember. See
  Packaging/macOS/build_installer.sh and Packaging/Windows/installer.iss.
- The Help overlay's **"Manual (PDF)"** button (`Source/GUI/HelpOverlay.h`'s
  `findManualFile()`) locates it at runtime by checking those same fixed install
  locations (plus the equivalent per-user folders on macOS), so users never need to
  know where it lives — they open it from inside the plugin. Button is disabled with
  a tooltip if the file genuinely can't be found.

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
- [ ] True Peak Limiter holds the configured TP Limit within ~0.5 dB on realistic/stress
      content (full-scale alternating-sample signals, near-Nyquist sweep up to
      0.46×sample-rate) at every oversampling factor (2x/4x/8x) — the positive
      counterpart to the earlier `margin_test.cpp` finding that disproved the old
      hard-clip-only approach. See `## True Peak Limiter`'s "Scope of the guarantee" for
      the two known error sources (near-Nyquist tones, decimation growth on rapidly
      gain-modulated broadband/transient content) this margin covers and how it was sized.
