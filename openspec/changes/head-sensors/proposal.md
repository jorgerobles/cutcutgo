## Why

> **Terminology update (b1aa622, remap-motors-add-a-axis):** firmware motors renamed
> TOOL1→`Z1` (marker Z), TOOL2→`Z2` (blade Z), ACCESSORY→`A` (blade-rotation wormgear on the Z2 head).
> **Discrepancy flagged:** this document identifies the blade motor as TOOL1, while the machine model in
> `openspec/changes/remap-motors-add-a-axis/` maps blade Z to Z2 (tool 2). Reconcile during the on-hardware
> pin verification recorded in `docs/spike-a-axis-blade-detection.md` before archiving this change.

The blade/knife motor (Z axis = TOOL1) has no absolute reference: Z homing is stall-only (`limits.c` guesses contact from lost encoder steps) and after power-up or a stall the firmware does not know the current blade position. The head carries two sensing elements — a blade detector and a mark detector — routed through J500 (I2C `SDA_1`/`SCL_1`, `POWER_STATE_OUT`, several `UNKNOWN_*` MCU pins) that are unmapped and unused in this fork. These sensors are the prerequisite for real blade-position knowledge and detector-based blade homing, and equally for the calibration cycle, which is therefore deferred to a follow-up change. Sensors first.

## What Changes

- Add a hardware-inspection stage that determines the capabilities of the mark detector and the blade detector (schematic/PCB analysis, I2C address scan, GPIO probing with a debug firmware build) and records findings in a hardware note under `docs/`.
- Add HAL drivers for both detectors (init/read/selftest, bounded timeouts) with the concrete backend (I2C vs GPIO/analog) chosen by inspection results.
- Add host-side simulator models for both detectors so all logic is validated without hardware.
- Add a blade-axis reference cycle: supervised low-feed Z (TOOL1) motion to a blade-detector transition that sets a known `sys_position[Z_AXIS]` reference, replacing stall-guessing as the primary Z reference.
- Integrate the detector reference into homing/warmup (`$H`, `STATE_WARMUP`) with stall homing retained as fallback, and expose blade state (detected/not/fault/unknown) to the system layer and host.
- Defer the calibration cycle: it becomes a separate follow-up change building on these capabilities.

## Capabilities

### New Capabilities
- `head-sensors`: discovery/inspection of the mark detector and blade detector, HAL drivers with self-test and timeouts, and simulator equivalence.
- `blade-axis-reference`: blade-position awareness and Z-axis referencing/homing using the blade detector, fallback to stall homing, and blade-state reporting.

### Modified Capabilities
<!-- openspec/specs/ is empty; no existing capabilities change. -->

## Impact

- Firmware: new `hal/sensors.{h,c}` plus per-detector backend files; Z homing path in `limits.c`/`motion_control.c` gains a detector-based reference; protocol gains additive extension command and blade-state reports.
- Tooling: Unity host build gains `sensors_sim.{h,c}`; blade reference cycle validated host-side before any machine run (physical-safety constraint).
- Docs: hardware note with per-detector capabilities (pins/address, protocol, signal type, polarity, sampling limits).
- Dependencies: I2C peripheral library addition to the Harmony config if detectors sit behind the J500 I2C bus.
- Sequencing: the `calibration-cycle` change is deferred and will consume these capabilities instead of defining them.
