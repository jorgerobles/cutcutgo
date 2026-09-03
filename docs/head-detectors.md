# Head detectors: mark detector & blade detector — inspection record

Status: **identified and validated on machine** (baremetal bench firmware, 2026-09-03).
Source: `schematics/CricutMaker-schematics.pdf` (rev 2, virtualabs reverse) + on-device probing.

## Identified: I2C light/reflectance sensor @ 0x44 (ISL29125-compatible, partially)

On-machine evidence (bench firmware):
- I2C scan on J500 `SDA_1`/`SCL_1` (MCU RA3/RA2): single responder at **0x44**, ID reg 0x00 = **0x7D** (ISL29125 device ID).
- **NOT bit-exact ISL29125 behavior**: data registers 0x09-0x0E behave as independent 8-bit channels (not 16-bit RGB pairs); CFG2=0x3F NACKs on write (value-validated); CFG1=0x05 (valid ISL29125 mode) breaks reads while 0x0D works. Suspected Cricut-variant silicon/firmware.
- Working config: **CFG1=0x0D, CFG2=0x00**; writes verified with read-back; retry x2 + bus recover (9 SCL pulses) on NACK.
- **Reflectance channel: register 0x0A** responds strongly to surfaces under the sensor (black ≈ 0x1B, colored paper 0x33-0x7F+, flashlight → saturation).
- **Illumination is REQUIRED**: no head illuminator found on RD7/RD8/RD9 (cycled low, no visible light, no reading change). Ambient light under the head is insufficient — an **external fixed light source** is required for reflectance sensing until the original illumination path is found. OPEN ITEM.
- I2C must be **slow (~10 kHz)** and open-drain with clock-stretch wait; at ~100 kHz ACKs become non-deterministic.

## Driver mapping (implemented in `hal/sensors_isl.c`)

- Both detectors share the 0x44 reflectance sensor.
- `mark_detector_read` → channel 0x0A as Q16 (byte << 8).
- `blade_detector_read` → channel 0x0A thresholded (`ISL_BLADE_THRESHOLD`).
- Self-test: ID reg == 0x7D.
- Simulator equivalents in `tools/sim/plant/sensors_sim.c` keep host tests green.

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
