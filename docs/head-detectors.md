# Head detectors: mark detector & blade detector — inspection record

Status: **identified on machine — ISL29125 RGB light sensor** (probing continues).
Source: `schematics/CricutMaker-schematics.pdf` (rev 2, virtualabs reverse) + on-machine probing.

## Identified: ISL29125 RGB light sensor @ I2C 0x44

On-machine evidence (firmware `$DBG` probing, 2026-09-03):
- I2C scan on J500 `SDA_1`/`SCL_1` (MCU RA3/RA2): single responder at **0x44**.
- Register 0x00 (device ID) reads **0x7D** — the ISL29125 default device ID.
- Conclusion: the head PCB carries an **ISL29125 RGB digital light sensor**
  (ADDR low → 0x44). Same reflectivity signal serves both roles:
  **mark detection** (paper Print-Then-Cut marks) and **tool/blade detection**
  (QuickSwap gear flash/notches scanned during homing), per Cricut behavior and
  community reverse-engineering.

ISL29125 register map (for driver implementation):
| Reg | Name        | Notes                                      |
|-----|-------------|--------------------------------------------|
|0x00 | Device ID   | 0x7D                                       |
|0x01 | CFG1        | power-down after reset; write 0x05 = RGB mode, 16-bit, 375 lux (0x0D = 10000 lux range) |
|0x02 | CFG2        | IR compensation / filtering                |
|0x03 | CFG3        | RGB conversion-done flags, IRQ             |
|0x04/05 | G low/high | 16-bit                                  |
|0x06/07 | R low/high | 16-bit                                  |
|0x08/09 | B low/high | 16-bit                                  |

GPIO probing results: RD7/RD8/RD9 (and RA2/RA3 as GPIO) held high, **zero
transitions** during 2 s sampling windows with actuation → no discrete detector
lines on probed pins; all detection goes through the I2C sensor.

## Main-PCB facts (J500 head ribbon)

| J500 pin | Net            | MCU side            | Assigned use                          |
|----------|----------------|---------------------|---------------------------------------|
| 1        | SDA_1          | RA3 (SDA label)     | I2C bus to head PCB — **unassigned in firmware** |
| 2        | SCL_1          | RA2 (SCL label)     | I2C bus to head PCB — **unassigned in firmware** |
| 6        | POWER_STATE_OUT| via Q7 (AO3401A) + R16, gate net POWER_TRIGGER | **unassigned / unknown** |
| 8/9      | UNKNOWN_7/IN_6 | encoder pair        | rotary encoder Tool A (HAL TOOL1/2/ACC mapping TBD) |
| 12/13    | UNKNOWN_9/IN_8 | encoder pair        | rotary encoder Tool B                 |
| 16/17    | UNKNOWN_2/IN_10| encoder pair        | rotary encoder (gear in Tool B)       |
| 20-32    | MOTORx_OUTy    | A4950E/A4954 U1-U4  | head motors (tool drive / clamp)      |

X/Y encoders (RG0/1, RG6/7) go to the X/Y motor JST connectors, not J500.

## Candidate mapping for the detectors

- The main PCB has **no discrete opto components** for mark or blade detection; the
  only unassigned head-side channels are the **I2C bus (RA2/RA3)** and
  **POWER_STATE_OUT**.
- Hypothesis A (primary): both detectors live on the head PCB behind the I2C bus
  (optical front-end / ADC / EEPROM-like devices). Tool identification EEPROMs in
  Cricut tool adapters are known to be I2C, so the bus is expected to have ≥1 responder.
- Hypothesis B: POWER_STATE_OUT is a binary head/blade state line (direction and
  semantics unknown; Q7 can also let the main board gate head power).
- MCU `UNKNOWN_*` pins (RD7/RD8/RD9, RB14) do **not** route to J500 in the schematic;
  treat them as secondary probing candidates only.

## Probing plan (debug firmware, `$DBG` commands)

1. `$DBGI2C` — bit-banged open-drain I2C scan 0x08-0x77 on RA2(SCL)/RA3(SDA); print responders.
2. `$DBGPWR` — sample candidate lines (RA2/RA3, RD7/RD8/RD9): level + drive test.
3. `$DBGSAMP <n>` — sample candidate pins n×1 ms while actuating blade / moving mark.
4. `$DBGI2CR=addr,reg` — read one register (decimal args).
5. `$DBGI2CW=addr,reg,val` — write one register (decimal args).
6. `$DBGRGB[=addr]` — configure ISL29125 (RGB/16-bit/375 lux) and print R/G/B.

## Open items (resolve on machine, then update this note)

- [x] I2C responder address: 0x44, ISL29125 (both detectors behind it).
- [ ] RGB reflectance over white paper vs black mark (which channel + threshold separates marks).
- [ ] Sensor response while head scans blade/gear during homing (tool detection signature).
- [ ] CFG2 IR-compensation value Cricut uses (may matter for mark contrast).
- [ ] Max usable sampling rate (conversion ~100 ms at 16-bit/375 lux; 12-bit/10 kHz modes faster).
- [ ] Board variant (X1 vs X2) of the unit under test.

Driver constants in `hal/sensors*` MUST equal the values recorded here once probing completes.
