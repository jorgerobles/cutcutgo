# HANDOFF — CutCutGo / Cricut Maker 1

Fecha: 2026-09-03 (sesión 2)
Estado: head-sensors 18/18 + tty-connect 6/6. Firmware en máquina: `bin/FIRMWARE_c53ed76.uf2` (fix de stall NO verificado en máquina — ver bugs residuales).

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

1. **BUGS RESIDUALES de firmware (prioridad, requiere sesión con bancada)**:
   - a) Fix de stall alarm (c53ed76: ABORT_CYCLE no-bloqueante) NO verificado en máquina. Test: con herramienta FUERA, `T1`+`G1 Z2 F150` → esperado `ALARM:9` + firmware vivo + `$X` desbloquea.
   - b) Unlock ($X) durante warmup interrumpido → reanuda en bucle `<Run>` congelado (MPos 0).
   - c) ctrl-X (0x18) en ese estado → firmware no-responsivo total (solo power cycle).
   - d) Boot con herramienta FUERA → warmup homing en bucle infinito (el encoder de TOOL2 engrana con la herramienta). Restricción operativa: nunca reiniciar sin herramienta cepada.
2. **Mapeo de canales de motor** (`$DBGMOTOR=ch,dir,ms` ya implementado en firmware `fad0d2d+`): probar `ACC,CW,500` → ¿gira el engranaje del cuchillo? HAL tiene 5 canales: X, Y, TOOL1(=T0, sin efecto visible), TOOL2(=T1, **worm del plunger de cuchilla**, OCM2 ¡conflicto con X!), ACCESSORY (sin ruta G-code).
3. **Iluminación**: sensor 0x44 requiere luz externa fija (no se encontró iluminador en head; RD7/8/9 descartados). LED-hunt por registros del esclavo 0x44 a medias (bench `BENCH_33566cd`).
4. **Change `calibration-cycle`** (deferido): artifacts listos; añadir decisión de iluminación/umbral blade.
5. **Push a origin**: ~55 commits locales en main.

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

- `$HB` — blade reference (solo IDLE; luz necesaria) · `$BQ` — estado blade
- `$DBGI2C|$DBGPWR|$DBGSAMP=n|$DBGI2CR=a,r|$DBGI2CW=a,r,v|$DBGRGB` — debug hardware (requieren IDLE; error:8 si no)
- `$DBGMOTOR=<ch>,<dir>,<ms>` — drive directo de un canal HAL (ch: T1/T2/ACC/X/Y; dir: CW/CCW; ms<=2000, velocidad suave)

## Tooling de sesión (nuevo)

- `tools/tty.py` — consola serie scripted: `send` (delimita ok/error, exit 3 timeout), `monitor --secs`, `waitfor regex`; flock por device; exit 2 sin dispositivo. EL agente habla con la máquina sin copy-paste.
- `tools/flash.sh <uf2> [tmo]` — flasheo determinista en BL: espera unidad "Cutcutgo", monta, copia, detecta re-enumeración CDC **por hardware-id** (no por puerto). La carrera del BL está resuelta.
- `tools/llm.sh` — delegación a modelo local ornith (puerto 8090, OpenAI-compatible).
- opencode: agente `delegate` (subagent → llama.cpp/local-llama-256) + plugin `.opencode/plugin/local-delegate.ts` (pinea explore/general al modelo local; escape `[main-model]`). Reiniciar opencode para cargar.
- NOTA de delegación: el 9B NO para scripts/flash — eso es tooling determinista. Delegar solo cognición (resúmenes, análisis).

## Hallazgos hardware/máquina (sesión 2)

- `T1`+Z mueve el **worm del plunger de cuchilla** (TOOL2 motor) — el worm es autofrenante (no retrocedible a mano). `T0`/TOOL1 sin efecto visible.
- Con herramienta puesta, `T1`+`G1 Z±2 F150` mueve suave y sin alarma (F1500 alarma — feed del worm limitado).
- Stall-watchdog del stepper ES el endstop del homing (máquina sin endstops): `limits_set_state` durante homing solo marca el bit (`limits_disable` evita la alarma); fuera de homing lanzaba HARD_LIMIT → **bucle bloqueante = firmware muerto**. Fix c53ed76: ABORT_CYCLE no-bloqueante (ALARM viva, `$X` recupera). VERIFICACIÓN EN MÁQUINA PENDIENTE (test a).
- Colisión HAL: `MOTOR_X_OCM=2` = `MOTOR_TOOL2_OCM=2` (mismo OC2) — mover X y TOOL2 a la vez se pisa el PWM. Pendiente reasignar pines (decisión del usuario).
- Sensor 0x44: lecturas fluctúan con luz ambiente; umbrales requieren iluminación controlada.

## Referencias

- Docs: https://virtualabs.github.io/cutcutgo/ · Esquemáticos: `schematics/CricutMaker-schematics.pdf`
- Nota de hardware: `docs/head-detectors.md` (fuente de verdad de constantes de driver)
- Openspec: `openspec status --change head-sensors`
