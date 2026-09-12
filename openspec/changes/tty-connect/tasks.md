## 1. Utilidad consola

- [ ] 1.1 Crear `tools/tty.py`: argumentos (--dev, --timeout), autodetección (serial-by-id → único ttyACM/USB → error listando candidatos) y lock flock no bloqueante por dispositivo con códigos 2
- [ ] 1.2 Implementar `send "<cmd>"`: abrir, drenar entrada, enviar línea, capturar hasta `ok`/`error:`/timeout (exit 3), imprimir respuesta
- [ ] 1.3 Implementar `monitor [--secs N]` y `waitfor PATRÓN [--timeout S]` (regex, passthrough de líneas, exit 3 en timeout)
- [ ] 1.4 Prueba rápida real con la máquina en modo app: `send "$BQ"`, `send "X"` (error esperado), `waitfor` sobre stream del bench

## 2. Integración en flujo de depuración

- [ ] 2.1 Documentar uso en `docs/head-detectors.md` (o HANDOFF): ejemplos send/waitfor contra $DBG/$HB/[B]
- [ ] 2.2 Repetir el ciclo de debug del sensor (scan + lectura canal) usando solo tty.py desde el agente, sin copy-paste del operador
