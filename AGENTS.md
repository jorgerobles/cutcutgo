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
- **Firmware original**: `FIRMWARE_972f570.uf2` (upstream v1.0 release)
- **Firmware reconstruido**: `FIRMWARE_f0faf2f.uf2` (v1.0 sources + XC32 v4.35)

### Build Artifacts Convention

- **Todos los firmware compilados se commitean** con el hash del commit en el nombre
- Formato: `FIRMWARE_<commit-hash>.uf2` (ej: `FIRMWARE_f0faf2f.uf2`)
- No importa el peso del repositorio — trazabilidad completa
- También: `.hex`, `.bin`, `.elf` si son necesarios para debug

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

### Delegation policy (models & agents)

- **Route by judgment**: mechanical jobs (greps, inventories, rename sweeps, log digestion,
  drafting) go to delegated models; judgment work (ISRs, safety logic, pin decisions, commits)
  stays with the main model.
- **ornith (local 9B, `delegate` agent)**: ONE job at a time — memory-bound, never launch
  parallel ornith agents. Cognition only: never scripts, flashing, or direct writes to the
  live tree. Parallelize independent verification jobs with other free models (mimo 2.5 free,
  `general`/`explore` agents).
- **Prompt shape**: fully mechanical spec — scope dirs, exact patterns, output format, "do not
  modify files". Demand self-verification in the output (raw vs unique counts reconciled,
  collision flags). Explicitly list out-of-scope lookalikes (e.g. `PL_COND_ACCESSORY_MASK`,
  `DC_REDUCTION_*`); renames are whole-identifier, never blind sed.
- **Verify before acting**: spot-check edit-critical claims with cheap reads. Delegated intel
  expires when the tree moves — re-sync (codegraph/grep) before each edit batch; never cache
  line numbers across edits.
- **codegraph before delegating**: one `codegraph_explore` call returns verbatim source to
  cross-check delegate claims cheaply.
- **Tree safety**: delegated file-modifying jobs go in a `git worktree`, never the live tree;
  the main model applies + verifies + commits. Commits are atomic, gated on Docker `-Werror`
  build / sim tests, and never delegated. Commit promptly — uncommitted work is invisible to
  other sessions (check `git status` BEFORE delegating: parallel sessions may have moved the
  tree ahead of the plan).
- **Plan drift**: if delegate output changes the plan, update OpenSpec tasks/design
  immediately — don't let stale tasks persist.
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
