## ADDED Requirements

### Requirement: Calibration cycle command interface
The firmware SHALL provide additive GRBL-compatible extension commands to run the calibration cycle (full cycle and per-axis sub-cycles: blade-tip Z, X/Y offsets). Commands SHALL be accepted only when the machine is idle; otherwise the firmware SHALL return an error and start nothing.

#### Scenario: Start while idle
- **WHEN** the host sends the calibration command while the machine is idle
- **THEN** the firmware acknowledges, enters the calibration state and begins the requested cycle

#### Scenario: Start while busy
- **WHEN** the host sends the calibration command while a job, homing or jogging is active
- **THEN** the firmware returns an error response and machine state is unchanged

#### Scenario: Unknown to legacy senders is harmless
- **WHEN** a standard GRBL 1.1 sender never sends the extension commands
- **THEN** all standard GRBL behavior and responses are unchanged

### Requirement: Blade-tip Z calibration via blade detector
The Z sub-cycle SHALL determine the blade-tip reference by supervised motion sampling the blade detector, taking multiple samples with retracts between them, and computing the offset as the median of outlier-rejected samples. Motion SHALL use reduced feedrate and remain within clamped travel limits.

#### Scenario: Successful Z calibration
- **WHEN** the Z sub-cycle runs with a functional blade detector
- **THEN** the firmware samples the detector at N positions with retracts, stores the median of outlier-rejected samples as the Z offset and reports the result with its spread

#### Scenario: Detector fault during Z calibration
- **WHEN** the blade detector reports a fault or timeout during the sub-cycle
- **THEN** the cycle aborts with an alarm, motors reach a safe state, and no partial Z value is stored

#### Scenario: Detector unsupported
- **WHEN** the inspection record marks the blade detector as unusable
- **THEN** the Z sub-cycle reports unsupported and leaves the stored Z offset unchanged

#### Scenario: Degraded blade-axis reference
- **WHEN** the Z sub-cycle runs while the blade-axis reference is flagged degraded (stall fallback)
- **THEN** the sub-cycle completes measurements but refuses to store a Z offset and reports the degraded condition

### Requirement: X/Y offset calibration via mark detector
The X/Y sub-cycle SHALL scan fiducial marks with the mark detector, locate each mark center from detector transitions during supervised bidirectional passes, and compute X and Y sensing offsets from the deviation against the expected mark positions, using median + MAD over multiple passes.

#### Scenario: Successful X/Y calibration
- **WHEN** the X/Y sub-cycle runs over a fiducial with a functional mark detector
- **THEN** the firmware computes X and Y offsets from mark-center deviations and stores them with their spread

#### Scenario: Mark not found
- **WHEN** no detector transition is observed within the search window
- **THEN** the cycle aborts with a specific alarm, motors reach a safe state, and stored offsets are unchanged

### Requirement: Robust fixed-point statistics
Calibration math SHALL use fixed-point (Q16) integer arithmetic only (no FPU). Each quantity SHALL be estimated as the median of its samples, with MAD-based outlier rejection before the final value and spread are computed.

#### Scenario: Outlier rejection
- **WHEN** one of N samples deviates beyond the MAD threshold
- **THEN** the final value excludes the outlier and the spread reflects the retained samples

#### Scenario: Fixed-point determinism
- **WHEN** the same sample set is processed twice on the host simulator
- **THEN** both runs produce bit-identical results

### Requirement: Calibration persistence in NVM
Calibration results (Z/X/Y offsets, spread, sample count, schema version, CRC) SHALL be stored as one NVM record written once at cycle completion while idle. At startup the firmware SHALL validate the record CRC and schema version and apply the offsets to the position system; a missing or corrupt record SHALL yield zero offsets and a report flag.

#### Scenario: Round-trip persistence
- **WHEN** a calibration cycle completes and the firmware restarts
- **THEN** the stored offsets are loaded, CRC validates, and position output reflects the offsets

#### Scenario: Corrupt record
- **WHEN** the NVM record CRC fails at startup
- **THEN** the firmware uses zero offsets, sets the calibration-invalid report flag, and does not alarm

#### Scenario: No write during motion
- **WHEN** a calibration cycle is aborted mid-run
- **THEN** no NVM erase or write occurs until the machine is idle and the cycle is complete

### Requirement: Calibration reporting and abort
The firmware SHALL emit unsolicited `[CAL:...]` reports for cycle progress (sub-cycle start, per-sample values, final result) and SHALL honor feed-hold/reset as a clean abort: motion stops controlled, motors reach a safe state, and no partial values are stored.

#### Scenario: Progress reporting
- **WHEN** a full calibration cycle runs
- **THEN** the host receives `[CAL:...]` reports for each sub-cycle start, each accepted sample, and the final stored values with spread

#### Scenario: Abort mid-cycle
- **WHEN** the host sends reset during a calibration cycle
- **THEN** motion stops controlled, the machine returns to a safe idle state, stored calibration values are unchanged, and an abort report is emitted
