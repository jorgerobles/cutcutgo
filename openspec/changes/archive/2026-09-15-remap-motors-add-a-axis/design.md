## Context

The HAL (`hal/config.h`, `hal/motor.{h,c}`) defines five motors: `X`, `Y`, `TOOL1`, `TOOL2`, `ACCESSORY`, each a pin set (IN1/IN2 GPIO, PWM PPS mux, encoder A/B on port G, OCM channel). Current wiring of these pieces:

- `stepper.c` runs a custom 3-axis Bresenham over X/Y/Z; the Z leg is steered to `stepper_info.tool`, chosen by `selected_tool`: `0 → TOOL1` (`DEFAULT_B_STEPS_PER_MM`), `1 → TOOL2` (`DEFAULT_A_STEPS_PER_MM`), each with its own Z-direction inversion. So TOOL1/TOOL2 are two independent Z axes selected by active tool — matching the marker-Z / blade-Z reality — but the naming says "tool" (which reads as spindle semantics) and the step-rate defines already squat the `A_` name.
- `motor.c` maps `TOOL1`/`TOOL2` to `Z_AXIS` in GRBL terms; the change-notification encoder ISR hardcodes `HAL_MOTOR_TOOL1` when servicing encoder pins; `hal_motor_safety_checks()` walks a fixed list that includes TOOL1 but not ACCESSORY.
- `ACCESSORY` is declared and lookup-registered but never `hal_motor_init()`-ed at startup; it is only reachable through `$DBGMOTOR=ACC,...` in `hal/debug_probe.c`.
- Blade detection lives in `hal/sensors` (detector read/self-test) plus `blade_home.c` (detector polling + `blade_ref` integrity monitor over `sys_position[Z_AXIS]`). GRBL is `N_AXIS=3`; the gcode parser explicitly rejects `A` words (`gcode.c:300`).

See proposal.md for why this is being renamed and brought up now.

## Goals / Non-Goals

**Goals:**
- One mechanical truth in the code: five axis-named motors — X, Y, Z1 (marker Z), Z2 (blade Z), A (blade rotation) — with Z1/Z2 independent and tool-selected.
- A fully brought up in the motor driver (init, encoder, watchdog, idle de-energize) and drivable from the debug console.
- Hardware-verified Z2/A pin assignment before the rename is finalized.
- Spike evidence that A motion leaves blade detection intact, recorded under `docs/`.

**Non-Goals:**
- No `N_AXIS=4`, no G-code `A` words, no planner/report/homing changes — full axis promotion is a follow-up change gated on spike results.
- No physical pin reassignment beyond the verified Z2/A swap (if needed).
- No settings/NVM schema change; no behavior change for existing cut jobs.
- No new sensor work — blade detection is used as-is.

## Decisions

1. **Full rename, no compatibility aliases.** Symbols/defines become `HAL_MOTOR_Z1/Z2/A` and `MOTOR_Z1_*/MOTOR_Z2_*/MOTOR_A_*`; the debug console accepts only `Z1`, `Z2`, `A`. Rationale: aliases keep the misleading vocabulary alive forever, and the codebase is small enough that `-Werror` catches every miss. Alternative (grep-replace + `#define TOOL1 Z1` shims) rejected.
2. **Verify-then-commit pin map.** The Z2/A pin sets stay where they are until a supervised debug build jogs each candidate set (short, low-rate `$DBGMOTOR` pulses) and we observe which drives the blade Z carriage vs the wormgear. Only then is `config.h` finalized (swap included if required) and the finding recorded in `docs/`. Rationale: the user flagged the swap as plausible and it is untestable from code alone. Alternative (trust current assignment) rejected — commanding Z2 and spinning the wormgear instead risks crashing the blade into the mat.
3. **A lives in the motor driver, not the motion core.** A gets the same per-motor bring-up as X/Y/Z (`hal_motor_init(PWM)`, default speed, watchdog threshold, added to `hal_motor_safety_checks()`), is lookup-registered (already), and is commanded only manually (debug console). It stays outside the Bresenham tracer and the planner. Rationale: this preserves the core-value constraint — nothing that moves cutting mass changes behavior — while making A real enough to spike and later promote. Alternatives: (a) do nothing until full-axis change — rejected, spike needs a drivable motor; (b) promote to `N_AXIS=4` now — rejected, large blast radius on the cutting path with zero spike evidence yet.
4. **Free the `A_` name first.** `DEFAULT_A_STEPS_PER_MM`/`DEFAULT_B_STEPS_PER_MM` (blade/marker Z step rates) rename to `DEFAULT_Z2_STEPS_PER_MM`/`DEFAULT_Z1_STEPS_PER_MM` in the same change; a new `HAL_MOTOR_SPEED_A` define appears for the wormgear. Rationale: two different "A" meanings in one firmware is a standing trap.
5. **Generalize the encoder CN ISR.** Replace the hardcoded `HAL_MOTOR_TOOL1` encoder servicing with dispatch through the existing `ga_motor_lookup` table so Z1, Z2, and A encoders are all serviced uniformly. The Z-encoder path (state machine, direction decode, `limits_set_state` interplay) must be preserved bit-for-bit; host simulator blade-ref tests must pass unchanged. Rationale: A needs live encoder feedback, and hardcoded single-motor servicing cannot provide it.
6. **Spike = existing debug firmware path, scripted protocol.** Reuse `debug_probe` (`$DBGMOTOR`, sensor commands) in a `-DDEBUG_PROBE`-style build rather than a throwaway bare-metal binary. Protocol, in order: (a) idle blade-detection baseline; (b) identify Z2 vs A by short jogs (decision 2); (c) rotate A both directions while streaming blade state — pass = no detector stream interruption, no `BLADE:LOSS`, Z position counters untouched; (d) stall watchdog for A validated host-side in the simulator, not by blocking the wormgear on the machine. Rationale: the repo's physical-safety constraint (host-side validation first, minimal supervised machine exposure) and debug_probe already exists.
7. **Rename ripples into the host simulator.** `tools/sim` stubs mirror HAL names; they are renamed in lockstep so Unity tests keep compiling and blade_ref/sensors/motion tests act as the regression net for the CN-ISR generalization (decision 5).

8. **Blade raised before A rotation (HARD directive from on-machine spike).** Rotating A with the blade down/engaged risks blade, mat and material damage; confirmed on first spike run. Every A motion path (spike sequence, future firmware) gates on a raised-blade state; raise is done via Z2. Operator gets a beep + explicit GO prompt before any motion — never auto-run jogs after flash.

## Risks / Trade-offs

- [Z2/A pin assignment is actually swapped and verification is skipped or misread] → Decision 2 makes verification a gating task before `config.h` is finalized; jogs are milliseconds-long; findings recorded in `docs/` for review.
- [CN ISR generalization subtly changes Z-encoder timing and breaks blade detection] → Keep the Z dispatch first and structurally identical; host sim blade_ref/sensors suites must pass untouched; on-machine blade baseline re-run after rename before any A motion.
- [Wormgear stalls/overheats when A is energized at defaults] → Idle de-energize like other axes, conservative `HAL_MOTOR_SPEED_A` (slow) and watchdog threshold tuned during spike; A is never commanded by G-code.
- [Rename touches pending changes' docs (`head-sensors`, `blade-axis-reference` still say TOOL1/TOOL2)] → Add a terminology note and reconcile those deltas at their archive time; specs here define the new vocabulary.
- [Rename is wide but mechanical; a missed reference breaks -Werror build] → Full Docker rebuild gates every commit; no behavior change intended in the rename commit, so any diff in `.text` beyond symbols is a review flag.

## Migration Plan

1. Host-side: simulator rename + CN-ISR generalization + Unity suites green (no hardware involved).
2. Firmware rename commit (logical, pre-verified pin layout unchanged) → Docker build `-Werror` clean → commit `FIRMWARE_<hash>.uf2`.
3. On-machine supervised session: Z2/A pin verification → finalize `config.h` (swap if needed) → A bring-up commit → new UF2.
4. Spike run (decision 6 protocol) → findings note in `docs/` → `a-axis-control` evidence complete.
5. Rollback at any point = `git revert` of the atomic commits; no NVM/settings state is written, so the machine restores by reflashing the previous UF2.

## Open Questions

- Exact `HAL_MOTOR_SPEED_A` and watchdog threshold values for the wormgear — tuned during the spike, does not affect the architecture.
- Whether Z2 also needs its encoder serviced by the generalized CN ISR for blade integrity on the blade tool (currently only the active tool's Z matters) — answered by reading the full ISR during implementation; does not change the dispatch design.
