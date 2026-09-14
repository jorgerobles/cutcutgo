# HANDOFF — CutCutGo / Cricut Maker 1

Fecha: 2026-09-14 (sesión 3 — remap motores + A axis + spike detección de hoja)
Estado: firmware **spike** en máquina (no el app normal). `openspec/changes/remap-motors-add-a-axis` en curso (10/17 tareas).

## Qué es este proyecto

Fork de **cutcutgo** (virtualabs): puerto de **GRBL 1.1** al **PIC32MX470F512L** de la Cricut Maker 1. Máquina de corte por GCODE vía USB.
- Upstream: https://github.com/virtualabs/cutcutgo · Bootloader: https://github.com/virtualabs/cutcutgo-bl
- Workflow: OpenSpec (`openspec/changes/`). Cambio activo: `remap-motors-add-a-axis`.

## Qué se entregó esta sesión (todo commiteado, ~102 commits locales SIN push)

### Rename de motores + A axis (cambio `remap-motors-add-a-axis`)
- **Rename lógico** (mismos pines): `TOOL1→Z1` (marker), `TOOL2→Z2` (blade), `ACCESSORY→A` (rotación de hoja). `DEFAULT_A/B_STEPS_PER_MM → DEFAULT_Z2/Z1_STEPS_PER_MM` (libera el nombre "A"). `$DBGMOTOR` ahora usa `Z1/Z2/A`. `PL_COND_ACCESSORY_MASK` (upstream) intacto.
- **Dispatch de encoders extraído** a `hal/motor_encoder.{h,c}` (puro, compila en host): tabla de lookup + máquina de estados de cuadratura + `hal_motor_service_encoders()`. El CN ISR (`hal_motor_update_callback`) ahora es wrapper. `motor.h` desacoplado de `config.h` (host-safe). `hal_motor_stall_detection()` movido también.
- **A traído a la vida**: init al boot (PWM, CN, velocidad 2300, watchdog), en `hal_motor_safety_checks()`. Fix de bug latente: `HAL_MOTOR_A` tenía 16/17 inicializadores posicionales (faltaba `hard_limit_threshold`, desplazaba mode/direction).
- **Sim host**: `tools/sim/tests/test_encoder_dispatch.c` (5 casos) sobre el código REAL + stubs de costura (`sim_limits_stub`, `sim_motor_stubs`). Suite: `make -f tools/Makefile.sim test` → **29/29**.

### Firmware spike bare-metal (`hal/spike.c`, build `SPIKE=1`)
Consola interactiva por USB (no GRBL, no warmup/homing automático):
- `<x|y|z1|z2|a> <cw|ccw> <ms> [speed]` — jog directo (speed 800-2300, default 2000).
- `t` telemetría 10 Hz (steps por motor + reflectancia + blade) · `c` dump canales RGB · `d` dump regs 0x00-0x3F · `i` niveles digitales 14 pines · `p` barrido GPIO (IR LED hunt) · `s` scan CFG1/CFG2 · `w <reg> <val>` · `G` bench Gemini · `S` bench transiciones · `arm`/`disarm` (interlock A).
- **Interlock de seguridad**: `a` jogs requieren `arm` previo. `G`/`S` son no-bloqueantes (state machine) — un print bloqueante dentro de `spike_task` DEADLOCKEA el anillo TX del USB (lección dura, dejó A girando).
- Build: `docker run --rm -v "$PWD":/work -w /work cutcutgo-builder make -f tools/Makefile.firmware SPIKE=1 CONF=cutcutgo_bl clean build-one uf2`.

### Tooling nuevo
- `tools/beep.sh [motion|ok]` — **bip de audio real** (PulseAudio) antes de CUALQUIER movimiento. Directiva dura.
- `tools/flash.sh <uf2>` — flasheo determinista en BL (PAUSE+encendido). `tools/tty.py send|monitor|waitfor`.

### Directivas de seguridad (duras, en specs + docs)
1. **NUNCA rotar A con la hoja bajada/engranada** — subir con Z2 antes de rotar.
2. **BIP + confirmación GO explícita antes de CUALQUIER movimiento** — nunca auto-ejecutar tras flash.

## Mapa de motores CONFIRMADO (esquemático + máquina)

| Motor HAL | Pines MCU | Motor PCB | Función |
|---|---|---|---|
| X | RD0/RD1 | MOTOR2 (A4950E) | carro X |
| Y | RD2/RF1 | MOTOR1 (A4950E) | rodillos Y |
| **Z1** | RF12/RF8 | MOTOR5 (A4954) | plunger marker |
| **Z2** | RB6/RD5 | MOTOR4 (A4954) | plunger hoja (CW=subir, ~790 steps/mm, ~7mm tope) |
| **A** | RD3/RD11 | MOTOR3 (A4950E) | rotación hoja (engranaje latón) |

- Encoders: X=RG0/1, Y=RG6/7, A=RG8/9, Z1=RG12/13, Z2=RG14/15 (las líneas "encoder" de J500 = encoders de los 3 motores de herramienta, **sin ADC**).
- `MOTOR_X_OCM=2` = `MOTOR_Z2_OCM=2` → colisión de PWM preexistente (mover X y Z2 a la vez se pisan).
- Duty: 2300 = NO mueve eje cargado; 1800-2000 mueve (valores menores = más fuerza).

## Estado de la investigación: detección de hoja (el puzzle)

### Cadena confirmada
- Sensor I2C @**0x44** = **ISL29125 genuino** (datasheet Renesas, registro a registro 1:1): ID 0x00=0x7D, CFG1=0x01 (escribimos 0x0D), CFG2=0x02 (IRCOMP bit7 **NACK** en variante Cricut — compensación IR retirada), umbrales 0x04-0x07 (THL/THH default 0x00/0xFFFF = nuestro "255,255"), STATUS 0x08 (RGBTHF), GREEN 0x09/0x0A, RED 0x0B/0x0C, BLUE 0x0D/0x0E (16-bit). Espacio real 32 regs, alias módulo 32.
- **Emisor IR confirmado vivo y siempre encendido** (foto con cámara de móvil: brillo rosa en knob superior). El knob inferior = tubo de luz al ISL29125.
- Cadena: emisor IR → reflector/hoja → knob inferior → ISL29125 → I2C.
- Config que funciona: CFG1=0x0D, CFG2=0x00. Canal que responde al hueco: **GREEN** (llave ancha: dentro 769 ↔ fuera 1792).

### Impasse (lo que NO cuadra aún)
- Hoja real (fina, pulida) en la posición correcta (collar arriba, punta cruza knob inferior) **NO mueve GREEN** (plano 1793). Con A girando tampoco (sin modulación).
- El bench `S` descartó cualquier lector oculto en el ribbon: A girando solo pulsa rg8/9 (cuadratura del propio A); las otras 12 líneas a cero.
- La respuesta de la llave (769↔1792) probablemente era sombra sobre la **ruta inferior** (mark), no el haz lateral.
- No probado: **registros 0x40+** (el dump `d` para en 0x3F; si el canal IR de hoja vive arriba, se escapó).

### Hipótesis vivas para la próxima sesión
1. **Registros altos 0x40-0xFF** no aliased (build de una línea en `d`): barrer con llave dentro/fuera.
2. **Mecanismo de umbral+interrupción**: stock fija THL/THH (0x04-0x07) + INTSEL (CFG3) y lee STATUS RGBTHF en vez de datos crudos.
3. **Receptor no es el ISL29125** → analizador lógico/scope en la PCB de cabeza (SDA/SCL + líneas encoder) con firmware stock = respuesta definitiva. O preguntar a virtualabs.
4. La detección stock es el "spin test" (rotar + leer) — pero requiere que el haz cruce algo asimétrico; la hoja fina quizá no lo cruza a esta altura.

## Estado físico de la máquina (IMPORTANTE)
- Firmware **spike** flasheado (no el app). Para uso normal reflashear `bin/FIRMWARE_4f82d49.uf2`.
- Portaherramientas en **TOP** (collar por encima de los knobs, punta cruza knob inferior) — la posición de detección.
- Hoja **puesta**. A sin rotar. Z2 en ~154 steps del tope (cuidado al subir: sin watchdog en spike, mirar el freeze del encoder).
- Para reflashear: **PAUSE+encendido** → unidad "Cutcutgo" → `tools/flash.sh`.

## Pendiente (cambio remap-motors-add-a-axis)

Host-side: DONE (rename, encoder dispatch, A bring-up, tests, builds, UF2). Tareas hardware restantes del OpenSpec: 2.4 sanity, 3.2/3.3 pin-verify Z2/A (hecho implícito por mapa), 4.3 bench A, 5.1/5.3 spike run + findings, 6.1 closure.

Próxima sesión (orden sugerido):
1. Decidir el impasse del detector (registros altos vs scope vs virtualabs).
2. Completar tareas OpenSpec restantes + `openspec validate --strict` + archive si procede.
3. Push a origin (~102 commits locales).

## Comandos rápidos

```bash
# Sim host (29 tests)
make -f tools/Makefile.sim test

# Firmware app normal (Docker -Werror)
docker run --rm -v "$PWD":/work -w /work cutcutgo-builder make -f tools/Makefile.firmware all

# Firmware spike
docker run --rm -v "$PWD":/work -w /work cutcutgo-builder make -f tools/Makefile.firmware SPIKE=1 CONF=cutcutgo_bl clean build-one uf2

# Flash (máquina en BL: PAUSE+encendido)
tools/flash.sh <uf2>

# Consola spike: arm → <motor> cw/ccw <ms> [speed] · c · d · i · p · s · w · G · S · t
tools/tty.py send "z2 cw 200 1800"
```

## Referencias
- Datasheet ISL29125 (Renesas) — confirma el mapa 1:1. `docs/spike-a-axis-blade-detection.md` = fuente de verdad del puzzle.
- Esquemáticos: `schematics/CricutMaker-schematics.pdf` · Nota de detectores: `docs/head-detectors.md`.
- OpenSpec: `openspec status --change remap-motors-add-a-axis`.
