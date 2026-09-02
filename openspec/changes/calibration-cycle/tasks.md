## 1. Calibration core (prerequisite: head-sensors change merged)

- [ ] 1.1 Implement fixed-point (Q16) statistics helper: median, MAD, outlier rejection; Unity tests including determinism and overflow extremes
- [ ] 1.2 Implement the calibration state machine module (idle-gated entry, sub-cycles, supervised low-feed motion via existing probing-style motion, abort handling) consuming `hal/sensors.h`
- [ ] 1.3 Implement Z sub-cycle: sampled blade-detector contact passes with retracts on top of the blade-axis reference, median+MAD Z offset; refuse-to-store on degraded reference flag
- [ ] 1.4 Implement X/Y sub-cycle: bidirectional mark scans, mark-center location, offset computation with median+MAD
- [ ] 1.5 Unity host tests for both sub-cycles on `sensors_sim`: success, detector fault, mark-not-found, unsupported-detector paths

## 2. NVM persistence

- [ ] 2.1 Define the calibration NVM record layout (offsets, spread, sample count, schema version, CRC16) on the existing flash-backed eeprom path
- [ ] 2.2 Implement write-once-at-cycle-end (idle only) and startup load with CRC/schema validation, zero-offset fallback and invalid flag
- [ ] 2.3 Unity tests: round-trip, corrupt-CRC fallback, no-write-on-abort

## 3. Protocol surface and reporting

- [ ] 3.1 Add extension commands (`$CA` full cycle, Z and X/Y sub-cycle variants) to the protocol parser with idle-only gating
- [ ] 3.2 Emit `[CAL:...]` reports: sub-cycle start, per-sample, final values with spread, abort and unsupported notices
- [ ] 3.3 Verify standard GRBL 1.1 behavior unchanged (regression check with existing sender-facing responses)

## 4. Verification and docs

- [ ] 4.1 Full host-side simulator run of the complete cycle end-to-end (Unity), all abort/fault paths green
- [ ] 4.2 Build firmware with Docker toolchain, -Werror clean; commit `FIRMWARE_<hash>.uf2` per repo convention
- [ ] 4.3 Add calibration usage notes to `docs/` referencing the head-sensors hardware note for detector capabilities
