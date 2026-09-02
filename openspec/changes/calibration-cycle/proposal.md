## Why

The firmware has no calibration: Z (blade tip) offset and X/Y sensing offsets are unknown, so cut repeatability depends entirely on stall-homing and factory defaults. The machine's mark detector and blade detector make a real, sensor-based calibration cycle possible, but those sensors — and the blade-position awareness they enable — are delivered first by the `head-sensors` change (the blade motor needs them for position knowledge and homing). This change is therefore deferred to run after `head-sensors` and consumes its capabilities instead of redefining them.

## What Changes

- Add a calibration cycle that uses the blade detector (from `head-sensors`) to determine blade-tip Z offset and the mark detector to determine X/Y sensing offsets by reading fiducial marks.
- Compute calibration values with robust statistics (median + MAD outlier rejection) over multiple samples in fixed point.
- Persist calibration results in NVM (flash page, CRC + schema version) and apply them to the position system at startup.
- Expose the cycle through GRBL-1.1-compatible extensions (`$`/M codes) with progress and result reporting; no change to standard GRBL behavior.
- Out of scope here (delivered by `head-sensors`): hardware inspection, detector HAL drivers, simulator models, blade-axis referencing/homing.

## Capabilities

### New Capabilities
- `calibration-cycle`: the user-invokable calibration procedure — blade-tip Z via blade detector, X/Y offsets via mark detector, robust statistics, NVM persistence and GRBL-compatible command/report interface.

### Modified Capabilities
<!-- openspec/specs/ is empty; no existing capabilities change. Depends on capability `head-sensors` and `blade-axis-reference` from the head-sensors change. -->

## Impact

- Firmware: new calibration module consuming `hal/sensors.h` from the head-sensors change; NVM schema gains calibration entries (CRC + version).
- Protocol: new `$`/M-code extensions on top of GRBL 1.1 (additive, senders unaffected).
- Tooling: host-side simulator (Unity) reuses `sensors_sim` from head-sensors to validate the cycle without hardware; per physical-safety constraint, no machine test before simulator passes.
- Sequencing: blocked on the `head-sensors` change; its inspection findings fix the detector capabilities assumed here.
