## Purpose

Defines the machine's five-motor axis model and the firmware naming/pin-map that reflects it: X, Y, Z1 (marker Z), Z2 (blade Z), A (blade rotation). Z1 and Z2 are two independent Z-type axes selected by the active tool — never a ganged pair — and the physical pin assignment of Z2 vs A must be verified against the real hardware.

## ADDED Requirements

### Requirement: Five-motor axis naming
The firmware SHALL expose exactly five motor instances named after machine axes — X, Y, Z1 (marker Z), Z2 (blade Z), and A (blade rotation) — replacing the `TOOL1`/`TOOL2`/`ACCESSORY` naming in the HAL and all firmware references. The rename SHALL be a logical rename on the same physical pins, except for the Z2/A assignment governed by the pin-verification requirement below.

#### Scenario: Motor inventory after remap
- **WHEN** the firmware is built and its motor registry/lookup is inspected (simulator or debug build)
- **THEN** the registered motors are exactly X, Y, Z1, Z2, A, and no symbol named `TOOL1`, `TOOL2`, or `ACCESSORY` remains reachable from firmware sources

#### Scenario: Debug console uses axis names
- **WHEN** the debug console motor command is used
- **THEN** motors are addressable as `Z1`, `Z2`, and `A` (legacy names are not required to work)

### Requirement: Z1 and Z2 are independent tool Z axes
Z1 (marker) and Z2 (blade) SHALL remain two separate Z-type axes, each bound to its own motor, encoder, and step geometry; the active tool SHALL select which Z axis receives commanded Z motion. The two motors SHALL NOT be driven as a synced/ganged pair, and switching the active tool SHALL switch the Z axis, its direction handling, and its steps-per-unit source.

#### Scenario: Active tool selects Z axis
- **WHEN** the active tool is the marker, commanded Z motion moves only Z1
- **THEN** Z2 and A do not step

#### Scenario: Active tool selects blade Z
- **WHEN** the active tool is the blade, commanded Z motion moves only Z2
- **THEN** Z1 and A do not step

### Requirement: Verified Z2/A pin assignment
The physical pin sets currently assigned to the blade Z motor and to the blade-rotation wormgear motor SHALL be verified on hardware before the rename is finalized, and the pin map SHALL be corrected (swapped if needed) so that commanding Z2 moves the blade Z carriage and commanding A rotates the wormgear. The verified assignment SHALL be recorded in a hardware note under `docs/`.

#### Scenario: Pin verification on the machine
- **WHEN** each candidate pin set is jogged once in a supervised debug build
- **THEN** exactly one set moves the blade Z carriage and the other spins the blade wormgear, and the pin map matches that observation

#### Scenario: Misassignment corrected
- **WHEN** verification shows the current Z2 and A pin sets are swapped
- **THEN** the firmware pin map is swapped before the axis rename is committed, and a re-jog confirms correct assignment

### Requirement: A motor wired into the motor driver
The A motor SHALL be initialized by the firmware at startup like the other axes (driver mode, encoder lookup with change-notification, default speed, stall watchdog) so it is movable, and SHALL be safely de-energized when idle. A SHALL NOT be driven by ordinary G-code motion in this change.

#### Scenario: A moves from debug console
- **WHEN** a debug build receives a command to rotate A for a short interval
- **THEN** the wormgear motor turns in the requested direction and stops and de-energizes when the command completes

#### Scenario: A is inert to G-code
- **WHEN** a G-code program runs that does not use the debug console
- **THEN** the A motor is never commanded and remains de-energized at idle
