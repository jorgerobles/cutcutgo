## Why

Cada ciclo de depuración con la máquina exige intervención manual: el operador pega comandos en su terminal, copia la respuesta y la pega al agente. Con la iteración de head-sensors esto resultó ser el cuello de botella principal (decenas de ciclos flash→probar→reportar). El agente debe poder enviar comandos GRBL/debug y leer las respuestas del hardware directamente desde scripts, dejando al humano solo las acciones físicas (flashear en modo BL, manipular material/herramienta).

## What Changes

- Añadir `tools/tty.py` (pyserial, ya disponible 3.5): utilidad de consola serie para la máquina.
  - `send "<cmd>"`: envía un comando y captura la respuesta hasta `ok`/`error:N` o timeout.
  - `monitor [--secs N]`: stream crudo de la consola.
  - `waitfor PATRÓN [--timeout S]`: bloquea hasta ver un patrón (para bench, homing, reportes `[B]`/`[BLADE:...]`).
  - `--dev` opcional con autodetección (`/dev/serial/by-id`, luego único ttyACM/ttyUSB).
- Exclusión de acceso con flock (evita colisión con la terminal del operador).
- Códigos de salida máquina-parseables (0 ok, 2 dispositivo no disponible/ocupado, 3 timeout).
- Fuera de alcance: flasheado (sigue siendo manual en modo BL) y cualquier wrapper de flasheo.

## Capabilities

### New Capabilities
- `tty-console`: acceso scripted a la consola USB de la máquina (enviar, monitorizar, esperar patrones) con exclusión y autodetección.

### Modified Capabilities
<!-- Ninguna: herramientas nuevas, sin cambios de requisitos existentes. -->

## Impact

- `tools/tty.py` nuevo (host-side, sin tocar firmware); uso inmediato por el agente en depuración.
- Cambios futuros de debug (bench, $HB, $DBG*) dejan de requerir cortar/pegar.
- Riesgo de ocupar el CDC mientras el operador usa su terminal → flock + mensaje claro.
