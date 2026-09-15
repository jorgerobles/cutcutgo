# a-axis-homing Specification

## Purpose
Give the A axis (blade rotation, wormgear) an absolute home reference using the blade holder's optical encoder signature — two polished chamfers (one wide = index) and two slots read as GREEN reflectance by the ISL29125 — recovered by a pulsed-rotation state machine that is safe against the A4950 thermal latch and gated on a raised blade.

## ADDED Requirements

### Requirement: Absolute A homing from the optical signature
The homing sequence SHALL drive A in short PWM pulses and, with the motor stopped, sample the GREEN reflectance to detect the slot (refl < 15) followed by the wide-chamfer index peak (refl > 55), and SHALL set A home to the index peak. The sequence SHALL recover 0° in at most one revolution plus the slot→index spacing (≈17350 + 3500 steps).

#### Scenario: Clean revolution homes at the index
- **WHEN** the blade is raised and the holder presents a clean signature within 1.5 revolutions
- **THEN** A home is set at the wide-chamfer peak and reported, with no more than one full revolution of scan

#### Scenario: Signature absent (optics blocked)
- **WHEN** reflectance never leaves the base band (35–43) after 1.5 revolutions of pulsing
- **THEN** homing aborts with an explicit error and A is de-energized

### Requirement: Thermal-safe pulse drive
The homing sequence SHALL drive A only in discrete pulses (150–600 ms at speed 2000) separated by a cooldown of at least 1 s, so the A4950 never sustains PWM long enough to enter its thermal latch. The sequence SHALL abort if the A encoder stops advancing while the driver is commanded (frozen encoder = latch or jam).

#### Scenario: Pulse width and cooldown enforced
- **WHEN** homing drives A
- **THEN** no single energization exceeds 600 ms and each is followed by ≥1 s of stopped sampling before the next pulse

#### Scenario: Frozen encoder aborts
- **WHEN** A is commanded to move but its encoder counter does not advance across consecutive pulses
- **THEN** homing aborts, A is de-energized, and a stall/latch error is reported

### Requirement: Raised-blade precondition (HARD SAFETY)
The homing sequence SHALL refuse to rotate A unless the blade is verified raised: Z2 is driven to its top stall and a short verification jog confirms dz ≤ 150 encoder steps. If the blade is not raised, the sequence SHALL abort without rotating A.

#### Scenario: Blade down rejects homing
- **WHEN** A homing is requested while the blade is not verified raised
- **THEN** the request is refused with an explicit prompt to raise the blade, and A does not rotate

### Requirement: No Z2 drift during rotation
During A rotation the sequence SHALL monitor Z2 position; if Z2 moves beyond the stall-verify tolerance (dz > 150) the sequence SHALL abort, because blade engagement has changed mid-homing.

#### Scenario: Z2 drift aborts
- **WHEN** Z2 encoder position changes beyond tolerance while A is rotating
- **THEN** homing aborts and both motors are de-energized

### Requirement: Homing triggered and reported over the protocol
A homing SHALL be triggerable from a dedicated command (`$HA`) and its result (home found / error) SHALL be reported with the resolved A position in steps and degrees.

#### Scenario: Command triggers homing and reports
- **WHEN** the operator issues `$HA` with the machine idle and blade raised
- **THEN** the homing sequence runs and reports `[AHOME:ok steps=… deg=…]` or a specific error code
