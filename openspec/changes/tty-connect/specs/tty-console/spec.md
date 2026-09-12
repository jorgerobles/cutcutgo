## ADDED Requirements

### Requirement: Envío de comandos con respuesta delimitada
La utilidad `tools/tty.py send "<cmd>"` SHALL enviar un comando de texto a la consola USB de la máquina y capturar las líneas de respuesta hasta recibir `ok`, `error:` o agotar el timeout, imprimiendo la respuesta capturada. En timeout SHALL terminar con código 3 mostrando lo capturado parcialmente.

#### Scenario: Comando aceptado
- **WHEN** se envía `send "$BQ"` con la máquina en IDLE
- **THEN** se imprime la respuesta del comando y una línea `ok`, y el código de salida es 0

#### Scenario: Comando rechazado
- **WHEN** se envía un comando inválido
- **THEN** se imprime la línea `error:N` correspondiente y el código de salida es 0 (la decisión es del llamador)

#### Scenario: Sin respuesta
- **WHEN** la máquina no responde antes del timeout
- **THEN** el código de salida es 3 y se imprime lo capturado hasta ese momento

### Requirement: Monitorización y espera por patrón
La utilidad SHALL ofrecer `monitor [--secs N]` (stream crudo de líneas durante N segundos) y `waitfor PATRÓN [--timeout S]` (imprime cada línea recibida y termina cuando una línea coincide con la expresión regular dada). `waitfor` SHALL terminar con código 3 si expira el timeout sin coincidencia.

#### Scenario: Espera de reporte del bench
- **WHEN** el bench emite líneas `[B] d: ...` y se ejecuta `waitfor "\[B\] d:"`
- **THEN** la utilidad imprime la línea coincidente y termina con código 0

#### Scenario: Patrón que nunca llega
- **WHEN** el patrón no aparece antes del timeout
- **THEN** el código de salida es 3 tras imprimir las líneas recibidas

### Requirement: Autodetección y acceso exclusivo
La utilidad SHALL autodetectar el puerto (entradas de `/dev/serial/by-id/` coincidentes con cutcutgo/pic32/cdc, y en su defecto el único `/dev/ttyACM*` o `/dev/ttyUSB*`), aceptando `--dev` para forzar. Toda operación SHALL tomar un lock exclusivo no bloqueante por dispositivo; si el lock está tomado SHALL terminar con código 2 y mensaje indicando que la consola está en uso. Si no hay dispositivo disponible SHALL terminar con código 2.

#### Scenario: Dispositivo en modo bootloader
- **WHEN** la máquina está en modo BL y no existe puerto CDC
- **THEN** la utilidad termina con código 2 indicando que no hay dispositivo (el flasheo sigue siendo manual)

#### Scenario: Consola ocupada
- **WHEN** otra instancia de tty.py mantiene el lock
- **THEN** la nueva instancia termina con código 2 sin abrir el puerto

#### Scenario: Varios candidatos
- **WHEN** hay más de un puerto serie compatible y no se pasa `--dev`
- **THEN** la utilidad lista los candidatos y termina con código 2
