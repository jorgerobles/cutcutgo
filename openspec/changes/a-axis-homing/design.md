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
   with a plant model.

3. **Reflectance read ports the priming fix.** `mark_detector_read()` (and
   `blade_detector_read`) gain a read of reg 0x09 before 0x0A (commit `28d5272`
   from the spike branch). `refl = value_q16 >> 8`. The `a_home` seam takes the
   raw `int32_t value_q16`, so host and firmware share the exact threshold math.

4. **Thresholds are named constants, not magic numbers.** `AHOME_SLOT_REFL=15`,
   `AHOME_INDEX_REFL=55`, base dead-band 25–50, `AHOME_SLOT_TO_INDEX=3500`,
   `AHOME_STEPS_PER_REV=17350`, pulse 150–600 ms @ 2000, cooldown 1000 ms,
   `AHOME_Z2_DZ_TOL=150`. Calibration is a later tuning pass; architecture does
   not depend on exact values.

5. **Homing is a custom command, not the limit-switch `$H`.** `$HA` runs the
   optical homing (A has no limit switch). `$H` is unchanged for X/Y/Z. A
   homing result is reported via `[AHOME:…]` and the A position is folded into
   `blade_home_report`-style output / status report when A is live.

6. **Raised-blade precondition is enforced in firmware.** Before any A pulse,
   the sequence verifies Z2 is at top stall via a short verification jog
   (`dz ≤ 150`). This satisfies the archived `a-axis-control` "blade raised
   before A rotation" requirement in production, not just the spike.

7. **Degrees promotion = `N_AXIS=4` with A as a rotary axis.** `steps_per_degree
   ≈ 48.2` (17350/360). The custom Bresenham tracer and planner gain an A leg
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
