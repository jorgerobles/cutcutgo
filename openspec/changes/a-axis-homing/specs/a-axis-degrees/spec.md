# a-axis-degrees Specification

## Purpose
Promote A from a debug-driven motor to a real rotary axis in degrees: G-code `A` words move the blade rotation, the motion planner and tracer carry A alongside X/Y/Z, position is reported in degrees, and A homing establishes the degree origin at the optical index.

## ADDED Requirements

### Requirement: G-code A words move the axis in degrees
The g-code parser SHALL accept `A<value>` words and convert them to A-axis steps using `steps_per_degree` (≈76.19, derived from the measured ≈27,428 steps/rev). `A` SHALL be treated as a rotary axis (no soft-limit clamping in degrees unless enabled).

#### Scenario: A word commands rotation
- **WHEN** a job contains `A90` while homed
- **THEN** A rotates the equivalent of 90° (≈4338 steps) and the reported A position is 90.0°

### Requirement: Planner and tracer carry A
The motion planner and the custom Bresenham tracer SHALL include A as a fourth axis, steered to `HAL_MOTOR_A`, so A moves coordinated (or independently) with X/Y/Z. The existing X/Y/Z cutting path SHALL be unchanged when A is not present in a block.

#### Scenario: Coordinated and independent A motion
- **WHEN** a block contains only `A` motion, or A together with X/Y/Z
- **THEN** A follows the block correctly and existing X/Y/Z-only jobs behave identically to before

### Requirement: A position in status reports
The status report (`?`, `<…>`, and `$BQ`/new A report) SHALL include the A position in degrees when A is a live axis.

#### Scenario: Status shows A degrees
- **WHEN** A is homed and moved to 45°
- **THEN** the status report reflects an A position of 45.0° (within encoder resolution)

### Requirement: A homing integrated with the homing cycle
A homing SHALL be reachable both standalone (`$HA`) and as part of establishing a full machine reference, so a homed A has a defined degree origin (index = 0°) before A words are executed.

#### Scenario: Homing gates A words
- **WHEN** A has no home reference
- **THEN** A-word motion is refused or a soft-limit alarm is raised until A is homed
