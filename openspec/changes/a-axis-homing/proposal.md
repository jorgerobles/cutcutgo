## Why

`remap-motors-add-a-axis` (now archived) brought the A motor to life — renamed
it `HAL_MOTOR_A`, initialized it at boot, serviced its encoder through the
generalized CN dispatch, and validated it with a bare-metal spike. But A is
still a *fake* axis: it lives in the motor driver only, is commanded manually
via the debug console, and has no position reference and no G-code surface.

Session 4 solved the missing piece — the blade holder carries an **absolute
optical encoder** (two polished chamfers + two slots read by the ISL29125 on
I2C 0x44, GREEN channel). A validated homing algorithm recovers absolute 0° in
one revolution from the reflectance signature alone. That makes it possible to
promote A to a *real* rotary axis in degrees with a reliable homing reference.

## What Changes

- **A-axis homing** (`a-axis-homing`): port the validated pulsed-rotate +
  reflectance-threshold algorithm into GRBL as a state machine
  (`SCAN → SLOT → INDEX → VERIFY`) that drives A in short PWM pulses (thermal
  safety), samples the GREEN reflectance with the motor stopped, detects the
  slot (refl<15) then the wide-chamfer index peak (refl>55), and sets A home to
  the index. Triggered by a new `$HA` command (custom optical homing, not the
  limit-switch `$H` cycle). Hard preconditions: Z2 raised (stall-verified,
  dz≤150) before any A rotation; abort on thermal latch (frozen encoder), on no
  reflectance variation within 1.5 rev, and on Z2 drift during rotation.
- **Real A axis in degrees** (`a-axis-degrees`): promote A from a motor driver
  to a full rotary axis — `N_AXIS=4`, G-code `A` words in degrees, planner and
  custom Bresenham tracer extended to carry A, `steps_per_degree` derived from
  the measured ≈27,428 steps/rev, status report includes A position, and A
  homing integrated so `$H` (or `$HA`) establishes A=0 at the optical index.

## Capabilities

### New Capabilities
- `a-axis-homing`: absolute A-axis homing via the optical encoder signature,
  with the thermal-safety pulse drive and the raised-blade precondition.
- `a-axis-degrees`: A promoted to a real rotary axis in degrees (G-code `A`
  words, planner/tracer support, `steps_per_degree`, position reporting).

### Modified Capabilities
- `a-axis-control` (from `remap-motors-add-a-axis`): its "blade raised before A
  rotation" and stall-envelope requirements are now exercised by the homing
  sequence and by normal A motion, not just the spike.
- `motor-axis-map`: A gains a `steps_per_degree` calibration alongside its
  pin-map semantics.

## Impact

- **Firmware**: new `hal/a_home.{c,h}` (state machine + pulse driver); `hal/sensors_isl.c`
  (GREEN reflectance read with 0x09→0x0A priming); `grbl/grbl/system.c` (`$HA`
  command); for degrees: `grbl/grbl/config.h` (`N_AXIS=4`), `defaults.h`,
  `settings.c`, `stepper.c` (tracer), `planner.c`, `gcode.c` (A words),
  `motion_control.c` (homing), `report.c`/`protocol.c` (status).
- **Safety**: A homing and A motion remain gated on raised blade + stall
  watchdog; pulse drive avoids the A4950 thermal latch. Every motion path is
  host-side validated before machine exposure.
- **Tests**: host sim (`tools/sim`) gains an A-homing state-machine suite over
  the compile-the-real-code seam (`hal/a_home.c` with a reflectance+encoder
  plant model).
- **Docs**: `docs/a-axis-homing-encoder.md` lands in-tree (ported from the
  spike branch).
- **Build**: Docker XC32 `-Werror` clean; firmware committed as
  `FIRMWARE_<hash>.uf2` per repo convention.
