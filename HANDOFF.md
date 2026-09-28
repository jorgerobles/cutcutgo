# HANDOFF — CutCutGo / Cricut Maker 1

Fecha: 2026-09-28 (sesión 6 — **causa raíz de la deriva de refl resuelta: CFG2 IR-comp**).
Estado: spike firmware en la máquina, firma óptica recuperada (ranura/chaflán visibles),
diseño de homing adaptativo acordado con el operador. Pendiente: barrido fino del chaflán,
fijar CFG2=0x3F en firmware, portar homing dinámico a `a_home`.

---

## SESIÓN 6 (esta) — resumen ejecutivo

1. **Causa raíz de la deriva de refl (23↔115) = CFG2 en 0.** El ISL29125 tiene
   compensación IR activa (CFG2 0x02: bit7 IRCOM + bits[5:0] ALSCC, 0–63). `isl_setup()`
   escribía CFG2=0x00 (sin compensación) → el IR ambiente se colaba al canal GREEN y la
   base vagaba. **CFG2=0x3F** (ALSCC=63 máx, bit7=0 para evitar el NACK de la variante
   Cricut) restaura la firma: plateau ~55, ranura ~11 (caída 80%), chaflán ~59-75.
   Queda una deriva lenta monotónica (~4 min) probablemente térmica.
2. **steps/rev = 27,428** (76.19 st/°), NO 17,350. 17,350 era un espaciado de features,
   no una revolución. Ancla visual operador-verificada: 13,714 steps = 180.1°.
3. **Velocidad de A continua vía OCxRS**: 2000→8160 st/s, 2100→4985, 2200→2618,
   2300→230, 2400→240. Banda lenta (2300-2400, ~230 st/s) = creep cerca de fricción
   estática del wormgear; da resolución fina (~34 steps/150ms). NO hay "cliff".
4. **Algoritmo de homing adaptativo (split & refine, protocolo del operador):** barrido
   grueso localiza features; en cada transición (variación de refl en un arco pequeño,
   ej. 5°) retrocede y re-barre a resolución fina para fijar flancos; ancho de chaflán =
   distancia entre flancos × ángulo/paso. Discrimina ancho=Este de estrecho=Oeste. Solo
   se refina en flancos, no toda la vuelta. **Robusto a la deriva** (mide flancos
   relativos, no niveles absolutos).
5. **`arm` en el spike es un flag software de confirmación (blade raised), NO el enable
   del motor** (el enable lo hace `jog_start` vía `hal_motor_init`+`set_manual`). El flag
   se limpia en cada reboot → `a cw` da `REFUSED` si no se re-`arm`.
6. **El emisor IR va hardwired al rail** (se enciende al enchufar, sin pasar por el MCU).
   No hay control programático → no hay compensación por apagado de LED.
7. **BUG de dirección del eje A (encoder) — RESUELTO**: `motor_encoder.c` comparaba
   `direction == motor->direction` (el comando del momento, cambia por jog) → `current_steps`
   monotónico (siempre sube o siempre baja). El engranaje holder↔piñón solo volteaba el signo.
   **Fix commiteado en `feat/spike-firmware`**: comparar contra referencia fija `M_CW` →
   contador de posición bidireccional (`cw`→holder CW→piñón CCW→decrementa, `ccw`→incrementa).
   Verificado on-machine. (Hipótesis anterior de swap IN1/IN2 era incorrecta, revertida.)
8. **Workaround para barrido fino sin dirección CCW** (obsoleto tras el fix): el eje es
   rotativo (27,428 pasos = 1 vuelta), así que se daba la vuelta solo en `cw`; ya no hace
   falta porque el contador es bidireccional.

## Descubrimientos clave (corrigen el HANDOFF de sesión 5)

- **CFG1=0x05** = MODE RGB(101) + RNG=0 (375 lux) + 16-bit + SYNC=0. El commit "correct
  mode for blade IR detection" solo cambió RNG 10k→375 lux (0x0D→0x05). CFG1 se mantiene
  en 0x05.
- **CFG2=0x3F** es la pieza que faltaba (sesión 5 lo dejaba en 0x00). El datasheet
  recomienda 0xBF (máx IR comp + rangos estándar), pero la variante Cricut NACKea bit7
  (IRCOM), así que se usa 0x3F (ALSCC máx sin IRCOM).
- **Posición de lectura (SET IN STONE)**: plunger Z2 en **stall inferior**. Con el holder
  arriba hay material absorbedor → lecturas en blanco. Tras plegar/desplegar la máquina
  el Z2 NO queda en stall (lectura plana sin features); hay que bajar Z2 antes de medir.
- **Geometría**: ranuras ×2 alineadas N/S; chaflanes ×2 planos E/O (ancho=Este=índice,
  estrecho=Oeste). Motor CW = hoja CCW (inversión tractor→holder). Filo triangular define
  "hacia dónde mira". Home preferente = filo al NORTE (ranura norte bajo el sensor).
- **I2C del sensor**: lectura 0x09 ANTES de 0x0A es OBLIGATORIA (latch). Bus a ~100kHz
  (dly 100 iters). Lectura ~15ms (límite = clock-stretching del sensor). Muestreo en giro
  continuo es inútil; solo stop-go con lectura asentada por parada.
- **La máquina cae a BL / reboota intermitentemente** en corridas largas o entre comandos.
  En BL aparece como `04d8:0009` (MSD) en `lsusb`, NO como CDC (`04d8:000a`).

## Estado físico de la máquina

- Spike firmware flasheado (worktree `feat/spike-firmware` HEAD `305a794` + variante
  EMPTY). Funcionando: CDC vivo, firma visible.
- CFG2 se dejó en 0x3F en vivo (comando `w 2 63`) — se pierde en el próximo reboot
  (hay que re-escribirlo o, mejor, fijarlo en `isl_setup()`).

## Qué está pendiente (orden sugerido)

1. **Barrido fino del chaflán** (speed 2300, ~34 steps/pulso) sobre el arco del chaflán
   (~a=-66655..-75824, ~9200 steps) para medir su ancho exacto por flancos.
2. **Fijar CFG2=0x3F en firmware** (`isl_setup()` en `sensors_isl.c`, spike y develop).
3. **Portar el homing dinámico/adaptativo a `a_home`**: umbral relativo (base medida en
   runtime), detección por flancos, disambiguación ancho/estrecho por secante. Reemplaza
   los `A_HOME_SLOT_REFL=30`/`INDEX_REFL=70` fijos.
4. **Resolver la deriva térmica residual** (opcional): calibrar ALSCC (no "máx") o aceptar
   el enfoque por flancos que ya la tolera.
5. **`$HA` end-to-end** con las constantes nuevas (27428) y home = filo norte.

## Comandos rápidos (spike)

```
tools/flash.sh <uf2>              # máquina en BL (PAUSE+encendido)
# consola spike (tty.py o python serial):
#   arm / disarm / t / r <reg> / w <reg> <val> / cap <ms> <speed>
#   <x|y|z1|z2|a> <cw|ccw> <ms> [speed]   # speed=OCxRS (más alto = más lento)
#   ch (latch test), i, c, f, m, adc
```

## Referencias

- `openspec/changes/a-axis-homing/` — proposal/specs/design/tasks (actualizados sesión 6:
  27428 st/rev, 76.19 st/°, CFG2=0x3F, umbrales dinámicos, algoritmo adaptativo).
- `docs/a-axis-calibration.md` — calibración 2026-09-28 (anclas, geometría, I2C, emisor).
- `docs/a-axis-homing-encoder.md` — algoritmo sesión 4 (umbrales ANTIGUOS 15/55).
- `docs/spike-a-axis-blade-detection.md` (rama spike) — puzzle del detector de hoja.
