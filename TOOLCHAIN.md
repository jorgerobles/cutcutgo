# TOOLCHAIN — CutCutGo

## Imagen Docker

`cutcutgo-builder`: Ubuntu 22.04 + XC32 v4.35 + PIC32MX_DFP 1.5.259 (sin MPLAB X).
La imagen se auto-construye en el primer `./tools/build.sh` si no existe localmente.
Reconstruirla a mano:

```bash
docker build -f tools/docker/Dockerfile -t cutcutgo-builder tools/docker
# versiones con override: --build-arg XC32_VERSION=v4.35 --build-arg DFP_VERSION=1.5.259
```

## Build (recomendado)

```bash
./tools/build.sh              # ambas configs + UF2
./tools/build.sh cutcutgo     # solo standalone
./tools/build.sh cutcutgo_bl  # solo app de bootloader
./tools/build.sh uf2          # regenerar solo el UF2
./tools/build.sh clean        # limpia la config actual
./tools/build.sh clean-all    # limpia build/ y dist/
```

El wrapper monta el repo en `/work`, corre make con el uid del host (artifacts
no-root) y auto-construye la imagen si falta.

Salida:

- `dist/cutcutgo/production/cutcutgo.X.production.hex` (standalone)
- `dist/cutcutgo_bl/production/Cutcutgo_maker1_bootloader_app.uf2` (app BL)

### Equivalente docker directo

```bash
docker run --rm -v "$PWD":/work -w /work cutcutgo-builder \
  make -f tools/Makefile.firmware all
```

### Flags del wrapper make (`tools/Makefile.firmware`)

- `TOLERANT=1` — elimina `-Werror` (debug)
- `BENCH=1` — define `SENSOR_BENCH`, artefactos en `dist/*-bench/`

## Simulador host (gate previo a flashear)

```bash
make -f tools/sim/Makefile.sim test    # Unity, gcc de host — 37 tests, sin Docker
```

Equivalente: `./tools/build.sh test`.

## Convención de artefactos (AGENTS.md)

Todo firmware compilado se commitea con el hash del commit:

```bash
cp dist/cutcutgo_bl/production/Cutcutgo_maker1_bootloader_app.uf2 \
   bin/FIRMWARE_$(git rev-parse --short HEAD).uf2
```

También `.hex`/`.bin`/`.elf` si hacen falta para debug.

## Conversión a UF2

**Ya está automatizada** — el target `uf2` (incluido en `all`) invoca
`tools/uf2_slim.py`, que trunca el bin al rango real de la app (fin de código,
4 KB-alineado, por debajo de la página NVM). No convertir a mano con
`objcopy + uf2conv.py` sobre el bin crudo del config `_bl`: ese bin abarca
hasta los config words (~92 MB de padding; el BL solo acepta
`0x1D010000..0x1D080000`, pero el artefacto queda gigante).

- **Family ID**: `0x4d414b52` ("MAKER"), app base: `0x1d010000`
- `utils/uf2conv.py` es lo que usa CI (`.github/workflows/build.yml`) sobre el
  hex standalone — ruta alternativa válida, ya con truncado implícito por
  rango del hex.

## Flasheo

```bash
tools/flash.sh bin/FIRMWARE_<hash>.uf2 [timeout_s]
```

- Espera la unidad `Cutcutgo` (modo BL: PAUSE mantenido al encender)
- Monta, copia, trata la desaparición del dispositivo = éxito
- Espera la re-enum del CDC serial **por id USB** (`/dev/serial/by-id/`), no
  por nombre de puerto

Manual equivalente: PAUSE+encendido → `udisksctl mount -b <dev>` → `cp` → `sync`
→ unidad que desaparece = bootloader consumió el UF2.

## Verificación

```bash
ls /dev/serial/by-id/ | grep -i cdc   # USB CDC activo (no asumir ttyACM0)
python3 - <<'PY'
import serial, glob
s = serial.Serial(glob.glob('/dev/serial/by-id/*CDC*')[0], 115200, timeout=1)
s.write(b'\r\n$I\r\n'); print(s.read(256))
PY
# -> "Grbl 1.1h ['$' for help]"
```

## Flags (producción)

Compilación (lista completa en `tools/Makefile.firmware`):

```
-g -x c -c -mprocessor=32MX470F512L -ffunction-sections -fno-common
-Icutcutgo.X/src -Icutcutgo.X/src/config/cutcutgo -DXPRJ_cutcutgo=<CONF>
-Werror -Wall -Wundef -Wshadow -Wpointer-arith -Wbad-function-cast
-Wwrite-strings -Waggregate-return -Wstrict-prototypes
-Wno-deprecated-declarations -Wredundant-decls -Wnested-externs
-Wlong-long -Wunreachable-code -Wmissing-noreturn
-mdfp="/opt/packs/Microchip/PIC32MX_DFP/1.5.259"
```

Link:

```
-Wl,--script=<ld>,--defsym=_min_heap_size=512,--gc-sections,
--no-code-in-dinit,--no-dinit-in-serial-mem,-Map=<map>,--memorysummary=<xml>
```

## Memoria del PIC (último build: 2026-09-24, `xc32-size`)

| Métrica | Standalone | App bootloader |
|---|---|---|
| Flash usada (text+data) | 153,236 B (29.2% de 512 KB) | 153,192 B (34.0% de 440 KB) |
| Flash libre | ~362 KB | ~290 KB |
| RAM usada (data+bss) | 82,027 B (62.6% de 128 KB) | ídem |
| RAM libre | ~48 KB | ídem |

La RAM incluye el ring buffer `$AT` (`AT_BUF_SIZE=2000` samples ≈ 16 KB).
Regenerar: `./tools/build.sh all` → `dist/*/production/memoryfile.xml`.

## CI

`.github/workflows/build.yml` (solo rama `main`) usa la action
`Rockman18/ghactions-mplabx` (MPLAB X en CI) — no el Docker local. Conviene
alinearla con `tools/build.sh` cuando se mergee a main.
