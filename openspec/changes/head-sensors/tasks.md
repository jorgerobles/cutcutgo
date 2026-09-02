## 1. Hardware inspection (mark detector & blade detector)

- [x] 1.1 Analyze `schematics/CricutMaker-schematics.pdf` and the reversed PCB SVG for the head/carriage section; map J500 `SDA_1`/`SCL_1`, `POWER_STATE_OUT` and `UNKNOWN_*` MCU pins to candidate detector connections
- [x] 1.2 Add a debug firmware build (Docker toolchain) that initializes the I2C peripheral on RA2/RA3 and scans addresses 0x08-0x77, reporting responders over USB
- [x] 1.3 Add debug commands to sample candidate GPIO/AN pins while actuating the blade and moving a mark under the sensor; log polarity, signal type and noise behavior
- [ ] 1.4 Run inspection on the machine and write the findings (per detector: pins/address, protocol, signal type, polarity, sampling limits) to a hardware note under `docs/`

## 2. Sensor HAL drivers + simulator models

- [x] 2.1 Create `hal/sensors.h` with the common interface (init/read/selftest, status codes, timeouts) for both detectors
- [ ] 2.2 Implement the mark detector driver backend (I2C or GPIO per inspection record) with timeout-bounded reads and self-test
- [ ] 2.3 Implement the blade detector driver backend with timeout-bounded reads and self-test
- [x] 2.4 Implement `sensors_sim.{h,c}` host-side models with injectable mark position, blade state and fault flags; wire into the Unity host build
- [x] 2.5 Unity tests: driver semantics on simulator (mark present/absent, blade states, timeout and self-test fault paths)

## 3. Blade-axis reference cycle

- [x] 3.1 Implement the blade reference state machine: idle-gated entry, supervised reduced-feed Z motion to a debounced detector transition, second slow pass, set `sys_position[Z_AXIS]`, reset abort to safe state
- [x] 3.2 Add the additive extension command for the reference cycle and wire idle gating in the protocol parser
- [x] 3.3 Integrate detector-based Z reference into `$H` homing and `STATE_WARMUP` with stall-homing fallback and degraded-reference flag
- [x] 3.4 Implement reference integrity monitoring (detector-vs-encoder cross-check, loss-of-reference flag)
- [x] 3.5 Implement blade-state exposure: on-change unsolicited report and query extension command
- [x] 3.6 Unity host tests: reference cycle success, fault abort, busy rejection, stall fallback, integrity contradiction cases

## 4. Verification and docs

- [x] 4.1 Full host-side simulator suite green (drivers + reference cycle + fallbacks) before any machine run
- [ ] 4.2 Build firmware with Docker toolchain, -Werror clean; commit `FIRMWARE_<hash>.uf2` per repo convention
- [ ] 4.3 Verify standard GRBL 1.1 responses unchanged (regression check) and update `docs/` hardware note with final validated detector capabilities
