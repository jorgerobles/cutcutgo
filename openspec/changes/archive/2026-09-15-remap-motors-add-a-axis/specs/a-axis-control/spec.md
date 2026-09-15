## Purpose

Makes the blade-rotation axis (A, wormgear on top of the blade Z head) a first-class motor driver in the firmware — movable from the debug console with encoder feedback and stall protection — while guaranteeing that blade detection keeps working while A moves. Establishes, via a bare-metal spike, the evidence needed to later promote A to a full G-code axis.

## ADDED Requirements

### Requirement: A movement with live blade detection
While the A motor is moving, blade detection SHALL continue to operate: detector polling keeps reporting blade state transitions, and the blade-reference integrity monitor SHALL NOT report loss (`BLADE:LOSS`) or fault as a consequence of A motion alone.

#### Scenario: Spike on bare metal
- **WHEN** the spike debug build rotates A back and forth while blade detection is live
- **THEN** detector state messages continue to stream unchanged and no integrity `BLADE:LOSS` is emitted during or after the A motion

#### Scenario: Encoder isolation
- **WHEN** A rotates at its commanded speed
- **THEN** only the A motor's encoder counters change; Z1/Z2 encoder counts and system position are unaffected

### Requirement: Blade raised before A rotation (HARD SAFETY)
The firmware/sequence SHALL never rotate A while the blade is down/engaged: every A motion path SHALL be gated on a verified raised-blade state (raised via Z2 before rotation, confirmed by operator or sensor). Bare-metal spike sequences SHALL raise the blade before any A jog.

#### Scenario: A jog with blade down is rejected
- **WHEN** an A rotation is requested while the blade is in the down/engaged state
- **THEN** the motion is refused and the operator is prompted to raise the blade first

### Requirement: Operator beep + confirm before any machine motion (HARD SAFETY)
Any spike/debug motion sequence SHALL signal the operator audibly (terminal bell) and obtain an explicit GO confirmation immediately before issuing machine motion. Auto-running jog sequences after flash without operator confirmation are forbidden.

#### Scenario: Motion requires beep + explicit GO
- **WHEN** a debug build is about to move any motor
- **THEN** the operator hears a beep and must answer an explicit confirmation prompt before the motion starts

### Requirement: A axis safety envelope
The A motor SHALL run with a stall watchdog like the other axes, SHALL stop and de-energize on stall or fault, and SHALL honor the global stop/reset paths (feed hold, reset, alarm) available to the rest of the machine.

#### Scenario: Stall on blocked wormgear
- **WHEN** the wormgear is blocked so A loses steps beyond its watchdog threshold
- **THEN** the firmware stops A, de-energizes it, and reports the stall through the same channel used for other motor stalls

#### Scenario: Reset during A motion
- **WHEN** a soft reset is issued while A is rotating from the debug console
- **THEN** A stops immediately and is de-energized

### Requirement: Spike evidence recorded
The bare-metal spike SHALL produce a findings note under `docs/` recording: the confirmed Z2/A pin assignment, observed A motion behavior (direction, speed, encoder counts), and blade-detection behavior during A motion, plus a recommendation for (or against) promoting A to a full G-code axis in a follow-up change.

#### Scenario: Findings note exists
- **WHEN** the spike completes
- **THEN** `docs/` contains the spike note with pin-assignment evidence, A motion observations, blade-detection results, and the follow-up recommendation
