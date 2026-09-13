## 1. Host-side simulator first (physical-safety constraint)

- [x] 1.1 Rename motor symbols/defines in `tools/sim` stubs to `Z1`/`Z2`/`A` (mirror HAL vocabulary) and rename `DEFAULT_A_STEPS_PER_MM`/`DEFAULT_B_STEPS_PER_MM` to `DEFAULT_Z2_STEPS_PER_MM`/`DEFAULT_Z1_STEPS_PER_MM`; verify all Unity suites still build (`make -C tools/sim test` or repo-equivalent) and pass: test_blade_ref, test_sensors, test_plant, test_motion_smoke
  - Note: inventory showed `tools/sim` has NO HAL-motor stubs (generic plant model); rename applied to firmware defines only (`defaults.h`, `settings.c`, `stepper.c`). Sim suites 22/22 pass. Pre-existing tool1/tool-2 rate cross-association in `st_select_tool()` vs `st_prep_buffer()` preserved and flagged in `docs/spike-a-axis-blade-detection.md`.
- [x] 1.2 Add a simulator test asserting encoder events on one motor do not perturb other motors' counters (encoder-isolation regression net for the CN-ISR generalization); verify it passes and fails if counters are cross-wired
  - Delivered as `tools/sim/tests/test_encoder_dispatch.c` (5 cases) over the REAL dispatch extracted to `hal/motor_encoder.c` (host-compilable seam; `limits_set_state`/`hal_motor_set_direction` recorded via harness stubs). Also lands the generalized CN dispatch (core of 4.1) — firmware-side verification pending Docker build. Suites: 27/27 pass.

## 2. Firmware rename (logical, pins unchanged)

- [x] 2.1 In `hal/config.h`: rename `MOTOR_TOOL1_*` → `MOTOR_Z1_*`, `MOTOR_TOOL2_*` → `MOTOR_Z2_*`, `MOTOR_ACCESSORY_*` → `MOTOR_A_*` (same physical pins), update the header comment block (marker Z / blade Z / blade rotation), add `HAL_MOTOR_SPEED_A` in `hal/motor.h` with a conservative initial value; verify full Docker XC32 build fails only on now-undefined references (expected intermediate state)
- [x] 2.2 In `hal/motor.{h,c}`: rename `HAL_MOTOR_TOOL1/TOOL2/ACCESSORY` → `HAL_MOTOR_Z1/Z2/A`, update instances, `hal_motor_lookup_init()`, `grbl_axis` comments (Z1/Z2 both map to `Z_AXIS`, tool-selected), and update `grbl/stepper.c` (`stepper_info.tool` selection uses Z1 (marker, `selected_tool==0`) / Z2 (blade, `selected_tool==1`) with per-tool Z inversion preserved), `grbl/protocol.c` motor-state check, `hal/debug_probe.c` `$DBGMOTOR` names to `Z1`,`Z2`,`A`; verify grep for `TOOL1|TOOL2|ACCESSORY` over `cutcutgo.X/src` returns no hits and full Docker build is `-Werror` clean
- [x] 2.3 Commit the rename as one atomic commit and build the artifact; verify `FIRMWARE_<hash>.uf2` produced via Docker toolchain and committed per repo convention
- [ ] 2.4 On-machine supervised sanity: flash rename build, run existing homing + a short pen-draw job, confirm `[BLADE:*]` reports unchanged; verify no behavioral drift vs previous firmware (status report, homing, blade detection baseline)

## 3. Z2/A pin verification (gates the pin map)

- [x] 3.1 Write the supervised jog procedure (short `$DBGMOTOR` pulses at low ms on the two candidate pin sets) into the spike note skeleton under `docs/` before touching the machine; verify procedure includes abort conditions and per-step expectations
- [ ] 3.2 Execute the verification session: jog each candidate set once, record which moves the blade Z carriage vs which spins the wormgear; verify observation is written into `docs/` note with pin sets identified
- [ ] 3.3 If swapped: swap `MOTOR_Z2_*`/`MOTOR_A_*` pin sets in `hal/config.h`, rebuild, re-jog to confirm; verify commanding Z2 moves the carriage and A spins the wormgear; commit the (possibly empty) pin-map finalization separately

## 4. A motor bring-up

- [x] 4.1 Generalize the encoder change-notification ISR in `hal/motor.c` to dispatch via `ga_motor_lookup` instead of hardcoded `HAL_MOTOR_TOOL1`, preserving the Z-encoder state machine exactly; verify sim encoder-isolation test (1.2) passes with the firmware logic mirrored host-side
- [x] 4.2 Initialize A at startup in `grbl/stepper.c` (`hal_motor_init(&HAL_MOTOR_A, HAL_MOTOR_PWM)`, default speed, watchdog threshold) and add A to `hal_motor_safety_checks()`; verify A is idle/de-energized after boot and appears in stall-check walk (debug build inspection)
- [ ] 4.3 Verify `$DBGMOTOR=A,CW,<ms>` / `A,CCW,<ms>` move the wormgear on a supervised bench connection; verify stop + de-energize at command end and immediate stop on soft reset; commit bring-up + build `FIRMWARE_<hash>.uf2`

## 5. Spike: A motion with live blade detection

- [ ] 5.1 Run the spike protocol from design.md decision 6: blade-detection baseline → A rotation both directions while streaming detector state → confirm no stream interruption, no `BLADE:LOSS`, Z counters untouched; verify transcript captured into `docs/` spike note
- [x] 5.2 Validate A stall watchdog host-side (sim: block A, exceed threshold, assert stop + de-energize + stall report); verify test added to sim suite and passes
- [ ] 5.3 Finalize the `docs/` spike note: confirmed Z2/A pin assignment, A motion observations (direction/speed/encoder counts), blade-detection results, and recommendation for/against promoting A to a full G-code axis; verify note complete per `a-axis-control` spec scenario

## 6. Closure

- [ ] 6.1 Full `-Werror` Docker rebuild + all Unity suites green; verify final `FIRMWARE_<hash>.uf2` committed
- [x] 6.2 Add terminology note (TOOL1→Z1, TOOL2→Z2, ACCESSORY→A) to pending changes' docs that reference old names (`head-sensors`, `blade-axis-reference`); verify `openspec validate --strict` passes for this change
