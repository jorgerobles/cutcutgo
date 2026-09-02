## ADDED Requirements

### Requirement: Hardware inspection record for head detectors
Before any driver implementation, the project SHALL record the discovered capabilities of the mark detector and the blade detector (wiring/pins, protocol and address if I2C, signal type, polarity, and usable sampling behavior) in a hardware note document under `docs/`, derived from schematic analysis and on-device probing with a debug firmware build.

#### Scenario: Inspection findings recorded
- **WHEN** the inspection task group completes schematic analysis and on-device probing (I2C address scan, GPIO sampling while actuating blade and marks)
- **THEN** a hardware note document exists under `docs/` listing, per detector: pin or bus address, protocol, signal type, polarity, and sampling limits

#### Scenario: Driver constants match inspection record
- **WHEN** the sensor drivers are implemented
- **THEN** every pin, address and protocol constant in the drivers SHALL equal the value recorded in the hardware note document

### Requirement: Mark detector HAL driver
The HAL SHALL expose a mark detector driver with init, raw read and self-test functions. The read SHALL return a fixed-point reflectance or boolean mark-present value with a status code, and SHALL time out instead of blocking forever when the device does not respond.

#### Scenario: Read over a mark
- **WHEN** the detector is positioned over a fiducial mark and read
- **THEN** the read returns success and a value in the mark-present range

#### Scenario: Read over blank surface
- **WHEN** the detector is positioned away from any mark and read
- **THEN** the read returns success and a value in the no-mark range

#### Scenario: Bus or pin fault
- **WHEN** the detector does not respond within the configured timeout
- **THEN** the read returns a timeout error status and no value is consumed

#### Scenario: Self-test detects fault
- **WHEN** self-test is executed with the detector disconnected or shorted
- **THEN** self-test returns a fault status identifying the detector

### Requirement: Blade detector HAL driver
The HAL SHALL expose a blade detector driver with init, read and self-test functions. The read SHALL report blade state (present/contact or position, per the inspection record) with a status code and bounded timeout.

#### Scenario: Blade state read
- **WHEN** the blade is actuated to a detectable state and read
- **THEN** the read returns success and the detected state

#### Scenario: No blade detected
- **WHEN** the blade is absent or out of detection range and read
- **THEN** the read returns success and the not-detected state

#### Scenario: Self-test detects fault
- **WHEN** self-test is executed with the detector line forced to a stuck state
- **THEN** self-test returns a fault status identifying the detector

### Requirement: Simulator equivalence for head sensors
The host-side (Unity) build SHALL provide simulator implementations of both detector drivers behind the same interface, with injectable state (mark position under the sensor, blade state) and fault flags so consumer logic can be tested without hardware.

#### Scenario: Simulator injects a mark
- **WHEN** a host test sets the simulator mark position under the sensor and calls the mark detector read
- **THEN** the read returns a mark-present value identical in semantics to the hardware driver

#### Scenario: Simulator injects blade contact
- **WHEN** a host test sets the simulator blade state to contact and calls the blade detector read
- **THEN** the read returns the contact state

#### Scenario: Simulator fault injection
- **WHEN** a host test sets a fault flag on a simulated detector
- **THEN** reads and self-test return the same error statuses as the hardware driver would
