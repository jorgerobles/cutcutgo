## ADDED Requirements

### Requirement: Blade-axis reference cycle
The firmware SHALL provide an additive extension command that establishes the Z (TOOL1) axis reference by supervised motion toward a debounced blade-detector transition at reduced feedrate within clamped travel, then sets `sys_position[Z_AXIS]` to the recorded reference. The command SHALL be accepted only when idle.

#### Scenario: Successful reference
- **WHEN** the host issues the blade reference command while idle with a functional blade detector
- **THEN** the firmware drives Z at reduced feed until the detector transition, sets the Z reference, and reports success with the reference position

#### Scenario: Detector fault during cycle
- **WHEN** the blade detector returns fault or timeout during the cycle
- **THEN** the cycle aborts with an alarm, motors reach a safe state, and no reference is set

#### Scenario: Start while busy
- **WHEN** the host issues the command while a job, homing or calibration is active
- **THEN** the firmware returns an error and machine state is unchanged

### Requirement: Homing and warmup integration with stall fallback
`$H` homing and startup warmup SHALL use the detector-based Z reference when the blade detector passes self-test; otherwise stall-based Z homing SHALL run unchanged and the firmware SHALL set a degraded-reference report flag.

#### Scenario: Homing with functional detector
- **WHEN** `$H` runs and the blade detector self-test passes
- **THEN** the Z axis reference is established from the detector transition

#### Scenario: Homing with faulty detector
- **WHEN** `$H` runs and the blade detector self-test fails
- **THEN** stall-based Z homing runs unchanged and the degraded-reference flag is reported

### Requirement: Blade state exposure
The firmware SHALL expose the current blade detector state (detected / not-detected / fault / unknown) to the system layer and to the host via an on-change unsolicited report and a query extension command.

#### Scenario: On-change report
- **WHEN** the blade detector state changes
- **THEN** the host receives an unsolicited report with the new state

#### Scenario: Host query
- **WHEN** the host sends the blade-state query command
- **THEN** the firmware responds with the current state and detector health

### Requirement: Reference integrity monitoring
After a reference is established, the firmware SHALL cross-check detector state against tracked encoder motion: a transition with no plausible motion, or expected motion completing with no transition, SHALL flag loss of reference instead of silently trusting the sensor.

#### Scenario: Contradiction detected
- **WHEN** detector state contradicts tracked blade position beyond tolerance
- **THEN** the firmware sets a loss-of-reference flag and reports it; stored machine position is not silently overwritten

#### Scenario: Consistent operation
- **WHEN** detector transitions match tracked motion within tolerance
- **THEN** no flag is raised and the reference remains valid

### Requirement: Host-side validation of the reference cycle
The blade reference cycle, its fallback path and integrity checks SHALL be exercisable in the Unity host build using the sensor simulator, and SHALL pass there before any machine run.

#### Scenario: Simulator success and fault paths
- **WHEN** host tests run the reference cycle with simulated detector success, fault, and contradiction cases
- **THEN** all cases produce the behaviors specified above without hardware
