## Context

> **Terminology update (b1aa622, remap-motors-add-a-axis):** firmware motors renamed
> TOOL1→`Z1` (marker Z), TOOL2→`Z2` (blade Z), ACCESSORY→`A` (blade-rotation wormgear on the Z2 head).
> **Discrepancy flagged:** this document identifies the blade motor as TOOL1, while the machine model in
> `openspec/changes/remap-motors-add-a-axis/` maps blade Z to Z2 (tool 2). Reconcile during the on-hardware
> pin verification recorded in `docs/spike-a-axis-blade-detection.md` before archiving this change.

CutCutGo is GRBL 1.1 on the Cricut Maker 1 (PIC32MX470F512L @96 MHz, no FPU, no endstops). Z axis is the TOOL1 blade motor (`settings.c` maps `Z_AXIS` to TOOL A steps_per_mm); its only reference today is stall detection in `limits.c` (`sys_position[Z_AXIS] = 0` on assumed contact), so blade position truth is weak after power-up or stalls.

Known hardware (reverse-engineered schematics): J500 head connector carries I2C `SDA_1`/`SCL_1` (MCU RA2/RA3), `POWER_STATE_OUT`, and extra encoder channels; several MCU pins are `UNKNOWN_*`. The carriage hosts a blade detector and an optical mark detector whose exact wiring/protocol are unmapped. Harmony config currently enables only clk/evic/gpio/usb (no I2C driver).

Constraints: physical safety (host-side simulator before machine), GRBL 1.1 compatibility, -Werror XC32 via Docker, no FPU.

## Goals / Non-Goals

**Goals:**
- Map both detectors by inspection + on-device probing; record capabilities in `docs/`.
- HAL drivers with self-test and bounded timeouts; simulator models with fault injection.
- Blade-axis reference cycle that sets a true Z reference from a blade-detector transition, integrated into `$H`/warmup with stall fallback.
- Blade state visible to system layer and host for interlocks and debugging.

**Non-Goals:**
- Calibration cycle (deferred to follow-up change consuming these capabilities).
- Tool EEPROM identification, Print-Then-Cut, pressure estimation.
- Persisting the blade reference in NVM (reference is re-established at runtime; persistence belongs to the calibration change).

## Decisions

1. **Inspection-first spike.** Debug firmware build performs I2C scan (0x08–0x77) on RA2/RA3, toggles `POWER_TRIGGER`/reads `POWER_STATE_OUT`, and samples `UNKNOWN_*`/AN pins while actuating blade and marks. Findings written to a hardware note; driver constants derive from it.
   - Alternative: assume community pin guesses → rejected (false sensor state can drive the blade motor wrongly; core value is measured truth).

2. **Sensor HAL seam.** `hal/sensors.{h,c}` exposes per-detector init/read/selftest with status codes; one backend file per detector isolates I2C-vs-GPIO differences. Simulator implements the same interface.
   - Alternative: direct peripheral calls in consumers → rejected (two consumers already: blade reference now, calibration later; both need simulator equivalence).

3. **Blade reference as a supervised single-axis cycle.** A state machine (patterned on probing/homing supervised motion) drives Z at reduced feed until a debounced blade-detector transition, optionally repeats a slower second pass for precision, then sets `sys_position[Z_AXIS]` to the recorded reference. Travel clamped; stall supervision active; reset aborts to safe state.
   - Alternative: treat detector as a classic limit switch inside `limits.c` → rejected for now (detector semantics — presence vs position, polarity — come from inspection; a dedicated cycle keeps `limits.c` stall logic intact as fallback).

4. **Homing/warmup integration with fallback.** `$H` and `STATE_WARMUP` use the detector-based reference for Z when the detector self-tests OK; otherwise stall homing runs unchanged and a report flag notes degraded reference.
   - Alternative: hard-replace stall homing → rejected (detector may prove unusable; machine must still home).

5. **Blade state exposure.** On-change `[BLADE:...]` unsolicited reports plus a query extension; system layer reads the same state for future interlocks (job start, calibration).
   - Alternative: poll-only hidden state → rejected (host visibility needed to debug on-machine behavior safely).

6. **Debouncing and consistency checks.** Detector transitions are debounced in time and cross-checked against encoder travel plausibility (a transition with no motion, or motion with no expected transition, flags loss of reference instead of silently trusting the sensor).

## Risks / Trade-offs

- [Detector binary-only or noisy] → slow second pass + debounce; precision limited to transition repeatability, measured and reported as spread.
- [I2C bus shared with other head devices] → scan documents all addresses; driver talks only to the detected detector; bus error → fault status, fallback path.
- [False reference sets wrong Z] → travel clamps, low feed, consistency cross-check, stall fallback retained; simulator coverage of fault paths mandatory before machine run.
- [Inspection inconclusive on one detector] → that detector's driver ships as unsupported stub with clear status; blade reference degrades to stall; mark detector simply waits for the calibration change.

## Open Questions

- Exact head-PCB routing of each detector (I2C device vs analog/discrete pin).
- Blade detector semantics: presence vs plunger position; polarity.
- Whether `POWER_STATE_OUT` participates in blade detection.
- Board variant (X1 vs X2) differences.
