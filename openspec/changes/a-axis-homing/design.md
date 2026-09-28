## Context

`remap-motors-add-a-axis` (archived) left A in the motor driver: initialized at
boot, encoder serviced via the generalized CN dispatch, drivable manually. A is
not a GRBL axis (`N_AXIS=3`, `gcode.c` rejects `A` words).

Session 4 validated the missing reference: the blade holder has an absolute
optical encoder (wide + narrow polished chamfers, two slots) read by the
ISL29125 (I2C 0x44) on the GREEN channel. Reflectance `refl` = high byte of
reg 0x0A (after priming a read of 0x09). Signature per revolution: base 35–43 →
slot (7–23) → ≈3500 steps → wide-chamfer index peak (63) → narrow bump (39–47)
→ slot. Period ≈17350 steps/rev. See `docs/a-axis-homing-encoder.md`.

This change turns that signature into a homing reference and promotes A to a
real rotary axis in degrees.

### Session-5 corrections (2026-09-28, on-machine) — supersede session-4 numbers

`docs/a-axis-calibration.md` supersedes the session-4 values below. The hard
corrections, operator-verified on the machine:

- **steps/rev = 27,428** (76.19 st/°), NOT 17,350. 17,350 was a feature spacing,
  not a revolution (visual filo-sur→filo-norte anchor: 13,714 steps = 180.1°).
- **Speed is continuously controllable via OCxRS (PWM period)** — measured on-machine
  (armed): `2000`→~8,160 st/s, `2100`→~4,985, `2200`→~2,618, `2300`→~230,
  `2400`→~240. The slow band (`2300`–`2400`, ~230 st/s) is the creep regime near
  the wormgear static friction and gives the fine angular resolution (~34 steps
  per 150 ms pulse) needed to measure chamfer width. Session 4's "finer
  resolution via slower PWM (higher OCxRS)" is CORRECT; an earlier session-5 note
  claiming a "stall cliff / no slow band" was a misread of REFUSED commands
  (A unarmed after reboot), not a speed limit.
- **Reflectance signature drifts and is alignment-sensitive**: the chamfer
  specular peak (63 in session 4) disappears after fold/unfold (max ~39 today);
  the slot dip remains robust (35→19 ≈ 46% drop). refl base wanders 23–91 across
  sessions → **fixed thresholds are fragile**; `a_home` must use dynamic
  thresholds (measure base in the seek, trigger on relative drop).
- **Emitter is hardwired to the supply rail** (lights on power-up, independent
  of the MCU) → no LED-off ambient nulling; ambient is discounted in software
  (dynamic base / profile floor) or by shielding.
- **Refined homing algorithm (operator protocol)**: dense sampling (60–90
  samples/rev), the mean separates plateau/chamfer/slot, chamfer angular width
  (chord/secant from angle-per-step) distinguishes wide=East from narrow=West,
  then rotate to the nearest North slot and back 90° to centre it.
- **ROOT CAUSE of the refl drift — CFG2 IR compensation was 0 (session-5 late
  finding, 2026-09-28).** The ISL29125 has active IR compensation (CFG2 0x02:
  bit7 IRCOM + bits[5:0] ALSCC, 0–63 codes). `isl_setup()` wrote CFG2=0x00 (no
  compensation) → ambient IR (sun/room lights/emitter heat) leaked into the GREEN
  channel → the base wandered 23↔115 and features were washed out. Setting
  CFG2=0x3F (ALSCC=63 max, bit7=0 to avoid the Cricut variant's IRCOM NACK)
  restores the signature: plateau ~55, slot dip ~11 (80% drop), chamfer ~59.
  **`isl_setup()` SHALL write CFG2=0x3F, not 0x00.** (CFG1 stays 0x05 = RGB,
  16-bit, RNG=0/375-lux.)
- **Adaptive homing search (split & refine)**: coarse fast scan locates features,
  then for each detected transition (refl variation within a small arc, e.g. 5°)
  reverse to just before the edge and re-scan at fine resolution (slow PWM) to
  pin the exact chamfer leading/trailing edges; chamfer width = edge spacing ×
  angle-per-step. Fine sampling only at edges — not the whole revolution.
- **Encoder direction bug (A axis, session-6) — RESOLVED.** `motor_encoder.c`
  compared `direction == motor->direction` (the *current* command, which changes
  each jog), so `current_steps` was monotonic (always increment or always
  decrement) — the encoder counted "progress toward command", not position. The
  holder↔pinion gear inversion only flipped the sign; it was never bidirectional.
  **Fix (spike, committed)**: compare against the fixed reference `M_CW` instead,
  making `current_steps` a true bidirectional position counter
  (`cw`→holder CW→pinion CCW→decrement, `ccw`→increment). Verified on-machine.
  (An earlier hypothesis — swap `MOTOR_A_DRIVER_IN1`↔`IN2` — was wrong and
  reverted.) Required for the split&refine (bidirectional motion).

## Goals / Non-Goals

**Goals:**
- Port the validated homing algorithm into GRBL as a host-testable state
  machine with the thermal-safety pulse drive and raised-blade precondition.
- Trigger homing via `$HA` and report the resolved A position.
- Promote A to a real axis in degrees: G-code `A` words, planner/tracer
  support, `steps_per_degree`, position reporting, homing gating.

**Non-Goals:**
- No change to the existing X/Y/Z cutting path (A absent in a block ⇒ identical
  behavior).
- No sensor/threshold redesign — thresholds from session 4 are calibration
  constants, tunable later with a denser revolution.
- No NVM/settings schema change beyond what the axis promotion requires.

## Decisions

1. **Homing lives in a new `hal/a_home.{c,h}`, compile-the-real-code seam.**
   Like `hal/motor_encoder.c`, `a_home.c` is hardware-independent logic driven
   through injected callbacks (pulse-move, read-reflectance, read-encoder,
   read-abort) so the whole state machine runs host-side in `tools/sim`.
   `blade_home.c` stays the Z2/blade-Z homing; the two mechanisms are distinct
   (A is pulsed + reflectance, Z2 is stall + planner).

2. **Pulse drive, never sustained PWM.** A is moved in discrete pulses
   (`hal_motor_set_manual(true)`, `set_speed(2000)`, `set_direction`,
   `delay(pulse_ms)`, `set_direction(STOP)`) with ≥1 s cooldown between pulses,
   exactly as validated in the spike. This is the A4950 thermal-latch
   mitigation. The pulse driver is a callback so the host test can drive it
   with a plant model. **Speed is selectable via OCxRS**: fast locate at
   `2200` (~2,600 st/s, ~390 steps/150 ms → ~70 samples/rev) and fine chamfer-width
   at `2300` (~230 st/s, ~34 steps/150 ms). There is no stall cliff; the slow band
   is a creep regime near static friction (session-5 correction).

3. **Reflectance read ports the priming fix.** `mark_detector_read()` (and
   `blade_detector_read`) gain a read of reg 0x09 before 0x0A (commit `28d5272`
   from the spike branch). `refl = value_q16 >> 8`. The `a_home` seam takes the
   raw `int32_t value_q16`, so host and firmware share the exact threshold math.

4. **Thresholds are dynamic, not fixed constants.** Session 4's `AHOME_SLOT_REFL=15`,
   `AHOME_INDEX_REFL=55` assumed a stable base band (35–43) and a strong
   specular chamfer peak — both proven unstable on-machine (session 5: base
   wanders 23–91, chamfer peak disappears after fold/unfold). The homing seam
   SHALL measure the base (median of the seek) at runtime and trigger the slot
   on a *relative* drop (e.g. `refl < base × 0.6`) and the chamfer on a
   *relative* rise. Named constants remain only for geometry
   (`AHOME_STEPS_PER_REV=27428`, `AHOME_SLOT_TO_INDEX` scaled to 27,428/17,350),
   pulse 150–600 ms @ 2000, cooldown 1000 ms, `AHOME_Z2_DZ_TOL=150`.

5. **Homing is a custom command, not the limit-switch `$H`.** `$HA` runs the
   optical homing (A has no limit switch). `$H` is unchanged for X/Y/Z. A
   homing result is reported via `[AHOME:…]` and the A position is folded into
   `blade_home_report`-style output / status report when A is live.

6. **Raised-blade precondition is enforced in firmware.** Before any A pulse,
   the sequence verifies Z2 is at top stall via a short verification jog
   (`dz ≤ 150`). This satisfies the archived `a-axis-control` "blade raised
   before A rotation" requirement in production, not just the spike.

7. **Degrees promotion = `N_AXIS=4` with A as a rotary axis.** `steps_per_degree
   ≈ 76.19` (27,428/360). The custom Bresenham tracer and planner gain an A leg
   steered to `HAL_MOTOR_A`; `gcode.c` parses `A` words; status reports include
   A. Detailed per-file changes are worked out at implementation time — the
   homing landing (this change's first phase) is independent of the axis
   promotion.

8. **Abort conditions are explicit and safe.** Thermal latch / jam (A encoder
   frozen across consecutive pulses), no reflectance variation within 1.5 rev,
   Z2 drift > 150 during rotation, and operator reset all de-energize A and
   leave the machine in a defined idle/alarm state.

## Risks / Trade-offs

- [Reflectance signature is unit- or alignment-dependent] → Thresholds are
  calibration constants; homing aborts loudly (never a wrong home) if the
  signature is absent, and a denser revolution pass (150 ms × 24) will retune.
- [`N_AXIS=4` blast radius touches the cutting path] → Gated behind the homing
  landing and host-sim regression; X/Y/Z-only jobs must be byte-identical in
  behavior (non-goal #1), and the rename's sim suites act as the regression
  net.
- [Thermal latch mid-homing leaves A energized] → Frozen-encoder abort plus the
  pulse/cooldown schedule; recovery is a power-cycle (documented, not auto).
- [Host sim plant is not the real optics] → The plant models a synthetic
  signature (base/slot/index/bump) from the measured constants; real-threshold
  validation stays on-machine and is a distinct task.

## Migration Plan

1. Port the priming fix + land `docs/a-axis-homing-encoder.md` (host-only,
   sim-verified).
2. Implement `hal/a_home.c` seam + host plant model + Unity suite (all green).
3. Wire `$HA` into `system.c` + `$AQ`-style report; Docker `-Werror` build;
   commit `FIRMWARE_<hash>.uf2`.
4. On-machine supervised: raised-blade verify, `$HA` run, confirm index ±
   accuracy; retune thresholds if the unit signature drifts.
5. Degrees promotion (`N_AXIS=4`), phased after homing is solid; host-sim
   tracer/planner regression then on-machine A-word jobs.
6. Rollback at any point = `git revert`; no NVM state, machine restores by
   reflashing a prior UF2.
