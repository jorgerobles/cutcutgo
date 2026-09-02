## Context

CutCutGo is GRBL 1.1 ported to the Cricut Maker 1 (PIC32MX470F512L @96 MHz, no FPU). The machine has no endstops; homing is stall-based over quadrature encoders. Today there is no calibration: blade-tip Z offset and X/Y sensing offsets are unknown.

This change is sequenced after the `head-sensors` change, which delivers: the hardware-inspection record for the mark detector and blade detector, the `hal/sensors.{h,c}` drivers (init/read/selftest with timeouts), the `sensors_sim` host-side models, and the blade-axis reference cycle. This design consumes those capabilities and adds only the calibration layer on top.

Constraints: physical safety (no machine test before host-side simulator passes), NVM = flash with erase-before-write (never during motion), GRBL 1.1 protocol compatibility, -Werror XC32 build via Docker.

## Goals / Non-Goals

**Goals:**
- Implement a repeatable calibration cycle: blade-tip Z offset via blade detector, X/Y sensing offsets via mark detector over fiducial marks.
- Robust statistics (median + MAD) in fixed point; results persisted in NVM (CRC + schema version) and applied at startup.
- Expose the cycle via additive GRBL extensions without breaking standard senders.
- Validate entirely on the host simulator (reusing `sensors_sim`) before any machine run.

**Non-Goals:**
- Hardware inspection, detector drivers, simulator models (delivered by `head-sensors`).
- Blade-axis referencing/homing (delivered by `head-sensors` as `blade-axis-reference`).
- Full Print-Then-Cut support; tool EEPROM identification; changing homing/stall logic or the planner.
- Automatic periodic recalibration; the cycle is user-invoked.

## Decisions

1. **Consume head-sensors capabilities; no sensor code here.** The calibration module calls `hal/sensors.h` only. Detector capabilities (signal type, sampling limits) come from the inspection record produced by `head-sensors`; procedures degrade gracefully per the recorded capability envelope (binary vs analog mark detector).
   - Alternative: duplicate a thin sensor access here → rejected (two sources of truth, divergent simulator behavior).

2. **Calibration as a GRBL system state + state machine.** A `calibration` module registers a cycle state machine invoked from the protocol layer (like homing/probing cycles): it issues supervised motion via `mc_*`/planner, samples detectors at a fixed rate, and completes with a result record. Reuses the probing infrastructure pattern (`G38.x` style supervised motion) rather than inventing a new motion path.
   - Alternative: host-driven calibration (sender computes offsets) → rejected (must work with any GRBL sender; firmware owns position truth).

3. **Statistics: median + MAD in Q16 fixed point.** N samples per quantity (default 7), median as estimator, MAD to reject outliers beyond a configurable threshold before storing. No FPU, so all math is integer/fixed-point, matching existing PID conventions.

4. **NVM record with CRC + schema version.** Calibration values (Z offset, X offset, Y offset, sample spread, timestamp/counter) live in one NVM record with CRC16 and schema version, written once at cycle end while idle (never during motion). Corrupt/missing record → defaults (zero offsets) and a report flag. Built on the existing flash-backed `eeprom.c` path until the planned NVM module lands.
   - Alternative: reuse GRBL `$` settings slots → rejected (settings space is sender-visible and version-fragile; calibration is a distinct record).

5. **Additive protocol surface.** New `$`-style extension command (e.g. `$CA` run-all, Z and X/Y sub-modes) plus `[CAL:...]` unsolicited reports for progress and final values. Standard GRBL 1.1 messages unchanged; unknown-command behavior for other senders unchanged.

6. **Z calibration builds on the blade reference.** The Z sub-cycle starts from the `blade-axis-reference` established by `head-sensors` and measures the offset between that reference and the calibrated blade-tip datum; if the reference is degraded (stall fallback flag), the Z sub-cycle refuses to store and reports degraded.
   - Alternative: independent Z reference inside calibration → rejected (duplicates reference logic and can conflict with homing).

## Risks / Trade-offs

- [Detector capability envelope narrower than hoped (e.g., binary-only mark detector)] → procedures threshold detector output; if a detector is recorded unsupported, that sub-cycle reports unsupported instead of guessing.
- [Wrong pin mapping or false readings drive motors wrongly] → calibration motion uses low feedrates, stall supervision active, travel limits clamped; simulator coverage required before machine run; consumes self-tested drivers from head-sensors.
- [Flash wear from repeated calibration] → single record write per cycle, erase-before-write only when idle.
- [Fixed-point statistics overflow with int32 math] → Q16 with bounded sample counts; unit tests cover extremes.
- [head-sensors slips or its inspection finds a detector unusable] → this change's corresponding sub-cycle ships as unsupported-with-clear-report; X/Y and Z sub-cycles are independently optional.

## Open Questions

- Mark detector sampling rate/reflectance resolution (fixed by the head-sensors inspection record before implementation).
- Fiducial strategy on-machine: dedicated calibration sheet vs carriage reference mark (decided at planning time from the inspection record).
