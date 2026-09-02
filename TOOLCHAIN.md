# TOOLCHAIN — CutCutGo

## Docker Image

```bash
# Build image (solo una vez)
docker build -f tools/docker/Dockerfile -t cutcutgo-builder tools/docker
```

**Contenido de la imagen:**
- Ubuntu 22.04
- XC32 v4.35 (compilador Microchip para PIC32)
- PIC32MX_DFP 1.5.259 (device family pack)

## Compilación

```bash
# Build completo (ambas configs + UF2)
./tools/build.sh

# Build solo una config
./tools/build.sh cutcutgo        # standalone
./tools/build.sh cutcutgo_bl     # bootloader

# Clean
./tools/build.sh clean
./tools/build.sh clean-all
```

## Makefile alternativo (desde el repo)

```bash
# Requiere stubs de MPLAB X (ver HOW_TO_BUILD.md §3-4)
make CONF=cutcutgo_bl TYPE_IMAGE=production build \
  "PATH_TO_IDE_BIN=/home/virtualabs/opt/mplab/v6.05/mplab_platform/bin/" \
  "MP_CC=/opt/microchip/xc32/v4.35/bin/xc32-gcc" \
  "MP_CC_DIR=/opt/microchip/xc32/v4.35/bin" \
  "DFP_DIR=/opt/packs/Microchip/PIC32MX_DFP/1.5.259"
```

## Conversión a UF2

```bash
# hex → bin
xc32-objcopy -Iihex -Obinary \
  dist/cutcutgo_bl/production/cutcutgo.X.production.hex \
  dist/cutcutgo_bl/production/cutcutgo-app.bin

# bin → UF2
python3 utils/uf2conv.py -c -f 0x4d414b52 -b 0x1d010000 \
  -o FIRMWARE.uf2 \
  dist/cutcutgo_bl/production/cutcutgo-app.bin
```

**Family ID**: `0x4d414b52` ("MAKR"), app base: `0x1d010000`

## Flasheo

1. Modo bootloader: mantener PAUSE pulsado al encender
2. Montar: `udisksctl mount -b /dev/sdd1`
3. Copiar UF2: `cp FIRMWARE.uf2 /run/media/r2d2/Cutcutgo/ && sync`
4. La unidad desaparece = bootloader consumió el UF2

## Verificación

```bash
ls /dev/ttyACM*                     # USB CDC activo
python3 -c "import serial; s=serial.Serial('/dev/ttyACM0',115200); s.write(b'\r\n$I\r\n'); print(s.read(256))"
# -> "Grbl 1.1h ['$' for help]"
```

## Flags de compilación (producción)

```
-g -mprocessor=32MX470F512L -ffunction-sections -fno-common -Werror -Wall
-mdfp="/opt/packs/Microchip/PIC32MX_DFP/1.5.259"
```

Link:
```
--script=<ld> --defsym=_min_heap_size=512 --gc-sections --no-code-in-dinit
```

## Memoria del PIC (build verificado)

| Métrica | Standalone | App bootloader |
|---|---|---|
| Flash usada | 129,940 B (24.6%) | 129,128 B (28.7%) |
| Flash libre | ~402 KB | ~319 KB |
| RAM usada | 57,931 B (44%) | ídem |
| RAM libre | ~73 KB | ídem |
