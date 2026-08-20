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
- **Gain Match** (JSFX slider14 `limiter_auto_gain`, displayed name changed from "Auto
  Gain" — param ID unchanged for automation/state compatibility — per explicit user
  request): **user-toggleable** (unlike Selective Clipper's own Auto Makeup Gain, which
  stays permanently on). Was strictly cut-only, targeting unity relative to the raw
  pre-processing dry/source signal — but that source is typically much quieter than a
  driven, limited "mastered" result, so engaging it dropped output well below the user's
  actual monitoring level, forcing a volume-knob round-trip on every A/B (reported by
  the user, who owns and drives this decision). **Now targets TP Limit**
  (`output_ceiling_db`'s smoothed value) instead, and can both cut AND boost toward it —
  **asymmetric range, -24dB cut / +6dB boost, NOT ±24dB**. The boost side must stay
  conservative: `applyGainMatch`'s peak-follower starts at (and decays back to, after any
  quiet passage) 0.0 while the target is a fixed nonzero value from sample one, so during
  the ~5ms attack ramp before the follower catches up, target/tinyPeak briefly computes a
  huge ratio. A wide boost clamp lets that transient through as an audible, distorted-
  sounding spike (confirmed via an offline peak/RMS test that reproduced a real user-
  reported "sounds distorted, like signal and delta combined" bug: with +24dB allowed, a
  steady tone spiked to peak=15.68 in the first 50ms before settling to a sane ~1.0; with
  +6dB, the same transient tops out at a mild ~2.0). Do not widen the boost side back
  toward symmetry without addressing the underlying ramp-up asymmetry first. See
  `Source/DSP/PeakFollowerMakeup.h`'s `applyGainMatch()` and `Source/DSP/Limiter.h`'s
  class-level doc comment. This keeps different Threshold settings loudness-matched to
  each other (preserving the original "fair A/B, no
  louder-sounds-better bias" purpose) while keeping Gain Match on/off close to the
  user's monitoring level regardless of state.
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
- "Gain Match" (renamed from "Auto Gain", `limiter_auto_gain`): now targets TP Limit
  instead of the raw dry/source signal, and can boost as well as cut -- see `## Limiter
  Core`. Do not resurrect "Auto Gain" as this control's GUI label; it undersells that
  the feature is specifically about matching loudness for a fair comparison, not adding
  makeup gain.

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

## Trial Build
Not part of the normal free/full release — a special build for handing out to beta
testers, per explicit user request, to create real friction toward giving feedback.
- Gated entirely behind `WISHCRAFT_TRIAL_BUILD` (CMake option, default OFF). The normal
  Release build built and packaged by default never defines this, so
  `Source/TrialLicense.h`'s check and `Source/GUI/TrialOverlay.h`'s blocking overlay
  don't exist in that binary at all — this cannot leak into the real product by accident.
- **Not hardened DRM** — proportionate friction for known beta testers, not a defense
  against a determined attacker. Documented as such in TrialLicense.h; don't oversell its
  robustness in any user-facing copy.
- Installer-written marker file, not a compiled-in fixed date: `TRIAL=1
  ./Packaging/macOS/build_installer.sh` (default `TRIAL_DAYS=30`, override via env var)
  and `/DTRIAL=1` passed to `ISCC.exe` (also requires the CMake build itself configured
  with `-DWISHCRAFT_TRIAL_BUILD=ON` — the .iss only packages, it doesn't build) each
  write a tamper-evident marker file at install time, **only if one doesn't already
  exist**:
  - macOS: `/Library/Application Support/Wishcraft Mastering Limiter/.trial`, written by
    a `postinstall` script attached to the VST3 component package (runs as root — this
    project's macOS packaging has no uninstaller at all, so nothing removes it either).
  - Windows: `%ProgramData%\Wishcraft Mastering Limiter\.trial`, written via a
    PowerShell script invoked from the `[Code]` section's `CurStepChanged`. Written via
    `Exec`, not a `[Files]` entry, so Inno's uninstaller has no record of it and won't
    remove it.
  - Content: `installDateISO\nhexHash`, where `hexHash = SHA256(secret + "|" +
    installDateISO)`. The secret string is a fixed literal duplicated in three places —
    `Source/TrialLicense.h`, the macOS `postinstall` script, and the Windows PowerShell
    snippet — and **must be kept identical across all three** or every trial install
    reads as tampered. Verified byte-for-byte hash agreement between bash's `shasum` and
    JUCE's `SHA256` via an offline test during development.
  - Missing, corrupt/tampered (hash mismatch), or backdated (system clock earlier than
    the recorded install date) marker file all resolve to the SAME blocked state as a
    genuinely expired trial — no distinct messaging that would hint at which check
    tripped.
- `PluginProcessor` checks trial status **once**, in the constructor (message thread,
  not the audio thread) — `processBlock` only ever reads a cached atomic
  (`trialBlocked`) and clears the buffer immediately if blocked, before touching
  anything else.
- GUI: a small always-visible "TRIAL — N days remaining" label near the title while
  active; a non-dismissible full-canvas overlay (`TrialExpiredOverlay`) once blocked,
  with an email button pre-addressed to wishcraftmusicstudio@gmail.com. The overlay is
  purely explanatory — audio is already silenced independently in `processBlock`
  regardless of whether the overlay is visible or even instantiated correctly.

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
