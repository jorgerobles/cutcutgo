## Context

La máquina expone consola USB CDC (GRBL 1.1 + extensiones $DBG/$HB/$BQ + stream del bench `[B]`). El agente de desarrollo corre en la misma máquina Linux donde está conectada la Cricut por USB (`/dev/ttyACM*`), con pyserial 3.5 disponible. Hoy toda interacción pasa por el operador (pegar/comandos/copy-paste), lo que ralentiza cualquier ciclo de debug. Restricción: el modo bootloader y el flasheo siguen siendo físicos (montar unidad y copiar UF2) — no se automatizan aquí.

## Goals / Non-Goals

**Goals:**
- Una sola utilidad `tools/tty.py` que el agente llame desde bash: send/monitor/waitfor.
- Respuestas delimitadas y parseables (`ok`, `error:N`, timeout) para scripting fiable.
- Autodetección del puerto y bloqueo exclusivo (flock) para no pisar la terminal del operador.
- Cero dependencias nuevas (pyserial ya presente).

**Non-Goals:**
- Flasheo automático / modo BL (requiere intervención física).
- Emular el protocolo GRBL completo (solo transporte serie).
- Wrapper del boot EFI: ningún cambio de firmware.

## Decisions

1. **Python + pyserial en un único fichero** (`tools/tty.py`) — subcomandos estilo CLI: `send`, `monitor`, `waitfor`.
   - Alternativa: stty+cat/echo en bash → frágil (race al leer respuestas, sin timeout fino). pyserial da timeouts y drain correcto.
2. **Autodetección**: primero `/dev/serial/by-id/` (coincidencia insensible mayúsculas de "cutcutgo"/"pic32"/"cdc"), si no el único `/dev/ttyACM*|ttyUSB*`; si hay varios → error pidiendo `--dev`. El CDC puede no existir si la máquina está en modo BL → error 2 con mensaje claro.
3. **Delimitación de respuesta**: en `send`, capturar líneas hasta ver `ok` o `error:` (respuestas GRBL), con timeout global (`--timeout`, defecto 10 s). Eco opcional del comando enviado. Devuelve exit 3 en timeout imprimiendo lo capturado (parcial útil).
4. **Exclusión**: flock no-bloqueante sobre un lockfile por dispositivo (`/tmp/cutcutgo-tty-<name>.lock`) en TODOS los subcomandos; si está tomado → exit 2 "consola en uso por otro proceso (¿terminal del operador?)".
5. **waitfor** para flujos largos: imprime cada línea recibida (passthrough) y termina al casar el patrón (regex Python) o timeout; pensado para `[B]`, `[BLADE:...]`, `[MSG:blade ref set]`.

## Risks / Trade-offs

- [El operador deja su terminal abierta y el CDC queda ocupado] → flock solo protege entre procesos tty.py; el CDC en sí no es multiplexable. Mitigación: mensaje de error claro + documentar cerrar terminal. Linux permite abrir el CDC dos veces (sin aviso) — el flock propio es la única barrera de nuestro lado.
- [Respuestas no delimitadas (bench streams continuos)] → `send` no es útil ahí; `waitfor`/`monitor` cubren el streaming.
- [Comando enviado mientras la máquina está busy] → respuestas `error:1`/`error:3`; el script las propaga tal cual (exit 0 con contenido, decisión del llamador).

## Open Questions

- Ninguno bloqueante. (Verificar device path real cuando la máquina esté conectada en modo app.)
