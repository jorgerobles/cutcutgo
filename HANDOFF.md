# HANDOFF — CutCutGo / Cricut Maker 1

Fecha: 2026-09-15 (sesión 5 — **homing de A en GRBL, casi**)
Estado: homing óptico de A implementado en `develop` (cambio OpenSpec `a-axis-homing`),
host-validado (sim 37/37) y `-Werror` limpio. Pendiente: validar on-machine — la máquina
quedó **bloqueada** (pulso de 50 ms demasiado corto) y hay que power-cyclear.

---

## SESIÓN 5 (esta) — resumen ejecutivo

1. **Reorganización de ramas**: spike → `feat/spike-firmware` (3b582db, conserva todo).
   `develop` = producción (84 commits, hasta 99399fa beep.sh). `main` NO se tocó.
2. **Archivado `remap-motors-add-a-axis`** (rename Z1/Z2/A + A bring-up); specs
   `a-axis-control` + `motor-axis-map` sincronizadas a `openspec/specs/`.
3. **Nuevo cambio `a-axis-homing`** (proposal/specs/design/tasks): homing óptico absoluto
   de A + eje A real en grados (`N_AXIS=4`). 9/17 tareas.
4. **Sensor**: portado el priming 0x09→0x0A y **CFG1=0x05** (el spike terminó en 0x05, no
   0x0D como decía el HANDOFF viejo). `ISL_BLADE_THRESHOLD` 100→50.
5. **Homing de A en GRBL** (`hal/a_home.c` puro + `hal/a_home_hal.c` + `hal/z2_endstop.c`).
   Comandos: `$HA` (homing), `$AQ` (reporte), `$ARQ` (refl crudo), `$AS` (barrido),
   `$ZL`/`$ZR`/`$ZU=<steps>` (bajar/subir/afinar Z2).
6. **Bug de tooling arreglado**: `tools/tty.py send` usaba `args.cmd` (no existía) →
   AttributeError. Corregido a `args.command`.

## Descubrimientos clave (corrigen el HANDOFF de sesión 4)

- **El chaflán se lee con Z2 BAJADO, no en tope.** En tope refl=35-43 (solo base, sin
  pico). La firma angular (slot<30, pico>70) se lee con Z2 en el stall inferior + rebote
  del muelle + **retract de 400 steps (0.5 mm)**. Sin bajar, el `$HA` da NOTRANS.
- **Geometría del chaflán (calibre D=11, ancho=5.14)**: chaflán ancho = **55.7° (~2685
  steps)**, chaflán estrecho ≤4mm = **≤42.7° (~2055 steps)**. `steps/grado = 48.19`
  (17350/rev). 0° = centro del chaflán ancho.
- **No es trivial distinguir ancho/estrecho**: N/S (ranuras) y E/W (chaflanes) están a
  180°, así que desde cualquier ranura hay un chaflán a ~3500 steps. El discriminador es
  la **anchura del pico** (umbral ~2370 steps ≈ 49°), no el espaciado.
- **Umbrales actuales (calibración actual, desplazados de sesión 4)**: ranura <30
  (medido 19), índice >70 (medido 83-95). La sesión 4 decía 15/55 — no valen aquí.
- **Barrido**: 50 ms NO mueve A (mínimo fiable ~150 ms). 150 ms@2000 = ~900 steps (muy
  grueso para discriminar anchura). Solución: **velocidad 2200 (más lenta)** a 150 ms.

## Estado físico de la máquina (IMPORTANTE)

- **BLOQUEADA**: el último `$HA` (firmware 5fdb3d5 con pulso 50 ms) dejó el main loop
  colgado (A apenas gira en 50 ms). **No responde a serial** → power-cycle
  (desconectar/reconectar USB).
- Último firmware flasheado: `5fdb3d5` (malo). **Corregido pero SIN flashear**:
  `FIRMWARE_3e571fe.uf2` (150 ms + velocidad 2200 + settle-antes-retract).

## Qué está hecho (todo en `develop`, commiteado)

| Commit | Qué |
|---|---|
| `d9e5156` | archiva remap-motors-add-a-axis |
| `d63cec4` | crea cambio a-axis-homing |
| `0476975` | priming 0x09→0x0A |
| `d4d4cba` | CFG1 0x05 + SEEK scan-limit (fix loop infinito) |
| `babe9f3` | wiring $HA/$AQ + z2_endstop + a_home_hal |
| `e9e1b10` | baja Z2 a stall antes de rotar A |
| `1a09139` | discriminación ancho/estrecho por anchura de pico + retract 400 |
| `5fdb3d5` | barrido 50ms (DESCARTADO) + settle-antes-retract + diag NOTRANS |
| `3e571fe` | 150 ms + velocidad 2200 (resolución fina) |
| `8df1dde` | fix tty.py |

## Próxima sesión (orden sugerido)

1. **Power-cycle** la máquina → BL (PAUSE+encendido) → `tools/flash.sh bin/FIRMWARE_3e571fe.uf2`.
2. **Verificar posición de lectura**: `$ZL` → esperar rebote → `$ZU=400` → `$AS` (debe
   dar span~76: min~19 ranura, max~95 pico).
3. **`$HA`** (beep + GO): debe devolver `[AHOME:ok steps=...]`. Si NOTRANS, el diag
   ahora imprime `min/max` de refl.
4. **Medir steps/pulso a velocidad 2200**: `$AS` y comparar samples vs 1.5 rev.
5. **Verificar umbral de anchura (2370) on-machine**: el pico de refl puede no abarcar
   los 55.7° geométricos (reflexión especular). Alternativa: discriminar por **altura**
   del pico (ancho ~95 vs estrecho ~60?) si la anchura no resuelve.
6. **Fase grados** (`a-axis-degrees`): reportar home en grados (steps/48.19) + `N_AXIS=4`
   (G-code `A`, planner, status). El homing ya fija 0°=chaflán ancho.

## Comandos rápidos

```bash
make -f tools/sim/Makefile.sim test   # 37 tests host
docker run --rm -v "$PWD":/work -w /work cutcutgo-builder make -f tools/Makefile.firmware all
tools/flash.sh bin/FIRMWARE_3e571fe.uf2   # máquina en BL
tools/tty.py send '$ZL' ; tools/tty.py send '$ZU=400' ; tools/tty.py send '$AS'   # barrido
tools/tty.py send '$HA'    # homing (beep + GO antes)
tools/tty.py send '$ARQ'   # refl crudo
```

## Referencias

- `openspec/changes/a-axis-homing/` — proposal/specs/design/tasks.
- `docs/a-axis-homing-encoder.md` — algoritmo sesión 4 (umbrales ANTIGUOS 15/55; los
  actuales son 30/70).
- `docs/spike-a-axis-blade-detection.md` (rama spike) — puzzle del detector de hoja.
