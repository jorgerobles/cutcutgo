<!-- GSD:project-start source:PROJECT.md -->

## Project

**CutCutGo — Firmware libre y fiable para Cricut Maker 1**

Fork de cutcutgo (virtualabs): puerto de GRBL 1.1 al PIC32MX470F512L de la Cricut Maker 1.
Máquina de corte/trazado por GCODE, controlada por USB con senders GRBL estándar.

**Core Value:** Que la máquina corte de forma **repetible y segura**: homing fiable, sin stalls falsos ni
motores energizados bloqueados, y posición/calibración persistida con desvío mínimo medible.

### Constraints

- **Hardware**: PIC32MX470F512L @96 MHz, sin FPU (fixpoint/Q15-Q16 en PID), sin EEPROM
  interna (NVM = flash con erase-before-write; nunca borrar durante movimiento)
- **Seguridad física**: los motores mueven masa real — ningún cambio de control se prueba en
  máquina sin pasar antes por simulador host-side; stall en trabajo = parada controlada
- **Compatibilidad**: mantener protocolo GRBL 1.1 (senders estándar); extensiones vía $ /M codes
- **Toolchain**: Docker (XC32 v4.35 + DFP 1.5.259); compilación -Werror limpia
- **Repositorio**: fork de virtualabs — commits atómicos por fase

### Key Decisions

- Quedarse en cutcutgo (no portar grblHAL) — cherry-pick de conceptos (NVM, plugins)
- Simulador host-side antes de tocar la máquina (restricción de seguridad física)
- Presión por proxy encoder/PWM (A4950 no expone corriente al MCU)
- NVM en última página flash de app + CRC + versión de esquema
- Homing por stall con thresholds por fase/eje

### Build Status

- **Verificado**: upstream v1.0 + Docker toolchain = firmware funcional en máquina
- **Firmware original**: `FIRMWARE_ORIGINAL.uf2`
- **Firmware reconstruido**: `FIRMWARE_FORK.uf2` (idéntico a upstream CI latest)

<!-- GSD:project-end -->

<!-- GSD:stack-start source:STACK.md -->

## Technology Stack

- **MCU**: PIC32MX470F512L @96 MHz (MIPS, sin FPU)
- **Firmware**: GRBL 1.1 modificado (C, MPLAB X project)
- **Toolchain**: XC32 v4.35 + PIC32MX_DFP 1.5.259 (Docker)
- **Build**: Makefiles MPLAB X + tools/Makefile.firmware (sin MPLAB)
- **Testing**: Unity 2.7.2 (host-side simulator)
- **UF2**: utils/uf2conv.py (family 0x4d414b52, base 0x1d010000)
- **Host**: inkcut-cutcutgo o cualquier sender GRBL 1.1 (USB CDC 115200)

<!-- GSD:stack-end -->

<!-- GSD:conventions-start source:CONVENTIONS.md -->

## Conventions

Conventions not yet established. Will populate as patterns emerge during development.
<!-- GSD:conventions-end -->

<!-- GSD:architecture-start source:ARCHITECTURE.md -->

## Architecture

Architecture not yet mapped. Follow existing patterns found in the codebase.
<!-- GSD:architecture-end -->

<!-- GSD:skills-start source:skills/ -->

## Project Skills

No project skills found. Add skills to any of: `.claude/skills/`, `.agents/skills/`, `.cursor/skills/`, `.github/skills/`, or `.codex/skills/` with a `SKILL.md` index file.
<!-- GSD:skills-end -->

<!-- GSD:workflow-start source:GSD defaults -->

## GSD Workflow Enforcement

Before using Edit, Write, or other file-changing tools, start work through a GSD command so planning artifacts and execution context stay in sync.

Use these entry points:

- `/gsd-quick` for small fixes, doc updates, and ad-hoc tasks
- `/gsd-debug` for investigation and bug fixing
- `/gsd-execute-phase` for planned phase work

Do not make direct repo edits outside a GSD workflow unless the user explicitly asks to bypass it.
<!-- GSD:workflow-end -->

<!-- GSD:profile-start -->

## Developer Profile

> Profile not yet configured. Run `/gsd-profile-user` to generate your developer profile.
> This section is managed by `generate-claude-profile` -- do not edit manually.
<!-- GSD:profile-end -->
