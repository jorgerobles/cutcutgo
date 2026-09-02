# HANDOFF — CutCutGo / Cricut Maker 1

Fecha: 2026-09-02
Estado: Fork upstream v1.0 compilable. Handoff limpio para desarrollo.

## Qué es este proyecto

Fork de **cutcutgo** de virtualabs: puerto de **GRBL 1.1** al **PIC32MX470F512L** de la Cricut Maker 1.
Máquina de corte/trazado por GCODE, controlada por USB con senders GRBL estándar.

**Repositorio upstream**: https://github.com/virtualabs/cutcutgo
**Bootloader**: https://github.com/virtualabs/cutcutgo-bl

## Estado actual

- **Compilación verificada**: upstream v1.0 + Docker toolchain (XC32 v4.35) = firmware funcional
- **Firmware original**: `FIRMWARE_ORIGINAL.uf2` (SHA256: `772b3367...`)
- **Firmware reconstruido**: `FIRMWARE_FORK.uf2` (SHA256: `54ca9923...`) — idéntico a upstream CI latest
- **Conclusión**: cualquier código que no compile fue roto por código añadido después del fork, no por el toolchain

## Hardware

- **MCU**: PIC32MX470F512L @96 MHz, 512 KB flash, 128 KB RAM, sin FPU
- **Motores**: 5 canales DC con encoder cuadratura óptico (A4950E/A4954)
- **Conector cabezal J500**: I²C (SDA/SCL), encoders adicionales, motores pinza
- **Sin endstops**: homing por stall (detecta pérdida de pulsos encoder)
- **BLE**: módulo RN4678 (sin usar en firmware actual)

## Objetivo del proyecto

Máquina de corte **repetible y segura**:
- Homing fiable en X/Y/Z
- Sin stalls falsos ni motores energizados bloqueados
- Posición/calibración persistida con desvío mínimo medible
- Detección de herramienta/hoja por I²C
- Estimación de presión por proxy encoder/PWM

## Restricciones

- **Seguridad física**: ningún cambio de control se prueba en máquina sin pasar antes por simulador host-side
- **NVM**: flash con erase-before-write (nunca borrar durante movimiento)
- **Protocolo**: mantener GRBL 1.1 compatibility
- **Toolchain**: todo por Docker (XC32 v4.35 + DFP 1.5.259)

## Fases planificadas (resumen)

1. **Foundation** — Docker tooling, simulador host-side, NVM
2. **HAL v2** — Motion en lazo cerrado coordinado (PID 1 kHz por motor)
3. **Homing & Stall** — Homing robusto X/Y/Z, stall detection corregido
4. **Position Truth** — Sync sys_position con encoders
5. **Calibración** — Estadística robusta (mediana+MAD), `$C` asistido
6. **Tool & Pressure** — I²C cabezal, proxy de carga
7. **Usabilidad** — Botones, UF2 slim, corte E2E

## Preguntas abiertas (validar en máquina)

- Mapeo exacto encoders J500 ↔ RG6-9/RG12-15
- Función real motor ACCESSORY y POWER_STATE_OUT
- Lectura I²C EEPROM de herramienta (dirección, formato)
- Variante de placa instalada (X2 vs X1)
- Thresholds de stall por eje/fase

## Referencias

- Docs: https://virtualabs.github.io/cutcutgo/
- Esquemáticos: tech.html → PDF/SVG
- Host: inkcut-cutcutgo (fork de InkCut) o cualquier sender GRBL 1.1
