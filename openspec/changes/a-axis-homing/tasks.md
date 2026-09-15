## 1. Host-side prerequisites (sim first, physical-safety constraint)

- [x] 1.1 Port the ISL29125 priming fix into `hal/sensors_isl.c`: `mark_detector_read()` and `blade_detector_read()` read reg 0x09 before 0x0A (port of spike commit `28d5272`); verify existing sim sensor suites still pass
- [x] 1.2 Land `docs/a-axis-homing-encoder.md` in-tree (from `feat/spike-firmware`); verify it documents the signature, thresholds, and safety constraints
- [x] 1.3 Add an A-homing state-machine seam in `hal/a_home.{h,c}` (compile-the-real-code, callback-injected: pulse-move, read-reflectance, read-encoder, read-abort) with states `SCAN → SEEK → PEAK` (+ blade-down/abort/latch/drift guards)
- [x] 1.4 Add a host plant model (synthetic optical signature: slot <15, index >55, base 40) + Unity suite `tests/test_a_home.c`; verify clean-rev, blade-down, notrans, frozen-encoder (latch), Z2-drift, fault, and abort cases pass (36/36 total)

## 2. Firmware wiring

- [ ] 2.1 Add `$HA` (home A) to `system_execute_line()` and an `$AQ`-style A report (`a_home_report()`); verify `$HA` returns `STATUS_IDLE_ERROR` when not idle
- [ ] 2.2 Wire the firmware callbacks: pulse-move via `HAL_MOTOR_A` manual mode (speed 2000, 150–600 ms, cooldown ≥1 s), read-reflectance via `mark_detector_read`, read-encoder via `HAL_MOTOR_A` counter, abort on `sys.abort`/reset
- [ ] 2.3 Enforce the raised-blade precondition: verify Z2 top-stall via a short jog (dz ≤ 150) before any A pulse; refuse with `[AHOME:BLADE_DOWN]` otherwise
- [ ] 2.4 Report homing result `[AHOME:ok steps=… deg=…]` or a specific error code (NOTRANS/LATCH/DRIFT/BLADE_DOWN); de-energize A on every exit path

## 3. Build + artifact

- [ ] 3.1 Full Docker `-Werror` build clean + all Unity suites green; verify `FIRMWARE_<hash>.uf2` committed per repo convention

## 4. On-machine supervised validation

- [ ] 4.1 Supervised `$HA` run: confirm index accuracy (± one fine-pulse) and the reported deg matches the mechanical index; record transcript in `docs/`
- [ ] 4.2 Retune thresholds with a dense revolution pass (150 ms × 24) if the unit signature drifts; update the named constants and re-commit
- [ ] 4.3 Verify the abort paths on-machine where safe (blade-down refusal; frozen-encoder detection via blocked wormgear with immediate power-down)

## 5. Degrees promotion (`N_AXIS=4`, gated on homing solid)

- [ ] 5.1 `config.h` `N_AXIS=4`, `defaults.h`/`settings.c` add A (steps_per_degree ≈ 48.2, max rate, accel), `gcode.c` parse `A` words
- [ ] 5.2 Extend the custom Bresenham tracer + planner to carry A → `HAL_MOTOR_A`; verify X/Y/Z-only jobs behave identically (host sim + on-machine)
- [ ] 5.3 Status report / protocol include A position in degrees; homing gates A-word motion until A is homed
- [ ] 5.4 Host-sim tracer/planner regression + on-machine A-word job; commit `FIRMWARE_<hash>.uf2`

## 6. Closure

- [ ] 6.1 `openspec validate --strict` passes; full `-Werror` rebuild + all Unity suites green; final `FIRMWARE_<hash>.uf2` committed
