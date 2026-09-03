# HANDOFF — CutCutGo / Cricut Maker 1

Fecha: 2026-09-03
Estado: Workflow OpenSpec activo. Cambio `head-sensors` en 17/18 tareas (falta regresión en máquina).

## Qué es este proyecto

Fork de **cutcutgo** de virtualabs: puerto de **GRBL 1.1** al **PIC32MX470F512L** de la Cricut Maker 1.
Máquina de corte/trazado por GCODE, controlada por USB con senders GRBL estándar.

- **Upstream**: https://github.com/virtualabs/cutcutgo
- **Bootloader**: https://github.com/virtualabs/cutcutgo-bl
- **Workflow**: OpenSpec (`openspec/changes/`) — cambios: `head-sensors` (activo, 17/18) y `calibration-cycle` (deferido, depende de head-sensors)

## Estado actual (rama main, ~40 commits locales SIN push)

### Hecho en esta sesión (head-sensors)
- **Workflow de planning**: `openspec/changes/head-sensors/` (proposal/design/specs/tasks) y `calibration-cycle` reescrito para consumir head-sensors.
- **Sim host-side portado a main** desde develop (SIN el rewrite de nvm/eeprom de develop): `tools/sim/` (Unity 2.7.2, plant, harness). Suite: `make -f tools/sim/Makefile.sim test` → **22/22 verde**.
- **Debug probe**: comandos serie `$DBGI2C` (scan), `$DBGPWR` (pines), `$DBGSAMP=n` (muestreo), `$DBGI2CR=a,r` / `$DBGI2CW=a,r,v` (regs), `$DBGRGB` (lectura sensor). En `hal/debug_probe.c`.
- **Baremetal bench** (sin GRBL, motores muertos): `make -f tools/Makefile.firmware BENCH=1 CONF=cutcutgo_bl clean build-one uf2` → stream continuo de bus/config/reflectancia. Último: `bin/BENCH_80469ec.uf2`.
- **Drivers reales**: `hal/sensors.h` + `hal/sensors_isl.c` (ambos detectores sobre el sensor de reflectancia I2C 0x44). Sim: `tools/sim/plant/sensors_sim.c`. Stub eliminado.
- **Blade reference**: `grbl/grbl/blade_ref.{h,c}` (core puro, testeado en sim) + `blade_home.{h,c}` (glue): comando `$HB`, query `$BQ`, hook tras `$H` con fallback stall + flag degradado, monitor de integridad y reportes `[BLADE:...]`.
- Docs: `docs/head-detectors.md` (registro de inspección completo).

### Hallazgos de hardware (detalle en docs/head-detectors.md)
- **Sensor**: esclavo I2C en **0x44** (ID reg0 = 0x7D), variante NO estándar de ISL29125: registros 0x09-0x0E son **canales de 8 bits independientes** (no pares RGB 16-bit); CFG2=0x3F NACKea; CFG1=0x05 rompe lecturas.
- **Config que funciona**: CFG1=0x0D, CFG2=0x00. Canal de reflectancia: **reg 0x0A** (negro ≈ 0x13, rojo ≈ 0x2F, verde ≈ 0x57-0x67, linterna → satura).
- **I2C crítico**: bit-bang **open-drain con espera de clock-stretch y a ~10 kHz** (dly 2400). A 100 kHz los ACKs son no deterministas. Retry x2 + bus recover (9 pulsos SCL) en cada op.
- **Iluminación REQUERIDA**: no se encontró iluminador en el head (RD7/8/9 ciclados LOW sin efecto). Luz ambiente bajo el cabezal insuficiente → para calibrar/detectar hace falta **fuente de luz fija externa**. OPEN.
- Z axis = TOOL1 (motor cuchilla); homing actual: stall-only en `limits.c`.
- develop = fase 1 GSD (NVM+sim) SIN merges de main — no tocar sin plan (decisión del usuario).

## Pendiente

1. **Task 4.3 de head-sensors**: regresión GRBL 1.1 (respuestas estándar sin cambios) + verificación en máquina del firmware real `bin/FIRMWARE_d7a63f5.uf2` (con luz fija: `$HB` debe buscar transición del reflector de cuchilla; sin luz → `[BLADE:NOTRANS]` + degradado, seguro).
2. **Iluminación**: decidir LED externo fijo vs localizar el iluminador original (abierto en docs).
3. **Change `calibration-cycle`** (deferido): al completar head-sensors, revisar sus artifacts — asume hallazgos ya presentes; añadir decisión de iluminación/umbral blade al planning.
4. **Push a origin**: ~40 commits locales en main.

## Comandos

```bash
# Firmware (Docker, -Werror limpio)
docker run --rm -v "$PWD":/work -w /work cutcutgo-builder make -f tools/Makefile.firmware all
# -> dist/cutcutgo_bl/production/Cutcutgo_maker1_bootloader_app.uf2

# Bench baremetal (sin GRBL/motores)
docker run --rm -v "$PWD":/work -w /work cutcutgo-builder \
  bash -c "make -f tools/Makefile.firmware clean CONF=cutcutgo_bl && make -f tools/Makefile.firmware BENCH=1 CONF=cutcutgo_bl build-one uf2"

# Sim host (22 tests)
make -f tools/Makefile.sim test

# Flash (BL mode: PAUSE al encender; montar y copiar)
udisksctl mount -b /dev/sdd1 && cp bin/FIRMWARE_<hash>.uf2 /run/media/r2d2/Cutcutgo/

# Convención de artefactos: bin/FIRMWARE_<hash>.uf2 (commit del código fuente),
# bench: bin/BENCH_<hash>.uf2. Commits atómicos + "chore: firmware build <hash>".
```

## Comandos serie nuevos (firmware normal)

- `$HB` — blade reference (solo IDLE; luz necesaria)
- `$BQ` — estado blade (state/ref/valid/degraded)
- `$DBGI2C|$DBGPWR|$DBGSAMP=n|$DBGI2CR=a,r|$DBGI2CW=a,r,v|$DBGRGB` — debug hardware

## Referencias

- Docs: https://virtualabs.github.io/cutcutgo/ · Esquemáticos: `schematics/CricutMaker-schematics.pdf`
- Nota de hardware: `docs/head-detectors.md` (fuente de verdad de constantes de driver)
- Openspec: `openspec status --change head-sensors`
