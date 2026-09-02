# Head detectors: mark detector & blade detector — inspection record

Status: **schematic analysis done; on-device probing pending** (see open items).
Source: `schematics/CricutMaker-schematics.pdf` (rev 2, virtualabs reverse).

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
2. `$DBGPWR` — sample POWER_STATE_OUT line (both directions/pull configs); print level.
3. `$DBGSAMP <n>` — sample candidate pins (RA2/RA3 as GPIO, POWER_STATE_OUT, RD7/RD8/RD9)
   n times at 1 ms while the operator actuates blade / moves a mark; print min/max/transitions
   to derive polarity, signal type (digital/analog), debounce needs.

## Open items (resolve on machine, then update this note)

- [ ] I2C responder addresses and which is mark vs blade detector (if I2C at all).
- [ ] Per detector: protocol, signal type, polarity, max sampling rate.
- [ ] POWER_STATE_OUT direction/semantics; does it participate in blade detection?
- [ ] Board variant (X1 vs X2) of the unit under test.

Driver constants in `hal/sensors*` MUST equal the values recorded here once probing completes.
