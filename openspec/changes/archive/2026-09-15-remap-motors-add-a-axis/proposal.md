## Why

The HAL names its five motors after their physical roles in the original Cricut firmware (`X`, `Y`, `TOOL1`, `TOOL2`, `ACCESSORY`). This is misleading: the machine really has five axis-like motors — X, Y, **Z1 (marker Z)**, **Z2 (blade Z)**, and **A (blade-rotation wormgear mounted on top of the Z2 head)**. `TOOL1`/`TOOL2` naming suggests spindle-speed semantics, but in this fork both are full Z-type axes selected by the active tool. Worse, the A motor exists in the HAL but is never initialized or driven by the firmware, so blade rotation is unreachable; and the pin assignment between the current `TOOL2` and `ACCESSORY` sets is unverified — a swap there would mean commanding Z2 actually spins the wormgear.

## What Changes

- Remap the five motor instances to axis semantics: `HAL_MOTOR_TOOL1` → `HAL_MOTOR_Z1` (marker Z), `HAL_MOTOR_TOOL2` → `HAL_MOTOR_Z2` (blade Z), `HAL_MOTOR_ACCESSORY` → `HAL_MOTOR_A` (blade rotation). Z1 and Z2 are **two independent Z axes** (active-tool selected), not a ganged pair.
- Logical rename with same physical pins — **except** an explicit verification (and swap if required) of the pin sets currently assigned to Z2 and A, since the wormgear sits mechanically on top of the blade Z carriage and the current mapping is unconfirmed.
- Make A work at firmware level: initialize it like the other motors (PWM mode, encoder lookup + change-notification, speed, stall watchdog, manual/free-wheel handling) so it can be commanded from the debug console (`$DBGMOTOR=...` becomes `Z1`, `Z2`, `A`).
- Spike (bare metal, on-machine): a debug firmware build that moves A while blade detection (detector polling + blade-reference integrity monitor) keeps running, proving encoder-isolation and no false `BLADE:LOSS` — and incidentally confirming the Z2/A pin assignment by observation.
- G-code A-word integration (`N_AXIS=4`, planner/parser/report) is explicitly **out of scope** and deferred to a follow-up change gated on spike results.

## Capabilities

### New Capabilities
- `motor-axis-map`: the five-motor axis model of the machine (X, Y, Z1 marker-Z, Z2 blade-Z, A blade-rotation), HAL naming/pin-map semantics, active-tool Z selection, and verified Z2/A pin assignment.
- `a-axis-control`: A motor brought up as a first-class axis driver (init, encoder, watchdog, debug-console movement) with the requirement that A motion must not degrade blade detection.

### Modified Capabilities
<!-- openspec/specs/ is empty; no existing capabilities change. Pending changes (head-sensors, blade-axis-reference) reference the old TOOL1/TOOL2 names and will be reconciled on apply. -->

## Impact

- **Firmware**: `hal/config.h` (pin-define rename + possible Z2/A swap), `hal/motor.{h,c}` (instances, lookup table, CN encoder ISR that hardcodes `HAL_MOTOR_TOOL1`, stall-check list), `grbl/stepper.c` (`stepper_info.tool` selection → Z1/Z2 per active tool; A init; `DEFAULT_A_STEPS_PER_MM`/`DEFAULT_B_STEPS_PER_MM` names collide with new A semantics and get renamed per-axis), `grbl/protocol.c` (motor-state check), `hal/debug_probe.c` (motor names).
- **Safety**: A drives a wormgear coupled to the blade; idle de-energize and stall thresholds must be set conservatively. Spike protocol moves only A (+ supervised Z2 jog for pin verification) — no X/Y mass movement.
- **Tests**: host-side simulator in `tools/sim/tests` mirrors motor naming; rename ripples into sim headers/stubs.
- **Docs**: pending change docs referencing `TOOL1`/`TOOL2`/`ACCESSORY` need a terminology note; `docs/` gains the spike findings (Z2/A pin truth + blade-detection-under-A-motion evidence).
- **Build**: Docker XC32 toolchain, `-Werror` clean; firmware committed as `FIRMWARE_<hash>.uf2` per repo convention.
