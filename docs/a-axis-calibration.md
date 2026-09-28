# Calibración del eje A — resultados on-machine 2026-09-28

Sesión de calibración con spike firmware (`feat/spike-firmware`), método
stop-go con lectura asentada por parada y verificación visual por el
operador (mancha negra marcada en el portahojas). Supercede los valores
de `a-axis-homing-encoder.md` (17350) — ver sección "Correcciones".

## Constante principal

| Magnitud | Valor | Método |
|---|---|---|
| **Steps encoder / revolución del holder** | **27,428** (76.19 st/°) | **Visual, operador-verificada**: filo-sur → filo-norte con mancha marcada, 13,714 steps = 180.1° |
| Media vuelta visual (180°) | 13,714 steps | ídem |
| Valor anterior (morning, feature-spacing) | 27,600 | ~0.6% arriba — feature-based, menos fiable |
| Valor doc sesión 4 | 17,350 | **ENTERRADO** — era una distancia entre features, no la rev |

## Geometría de la firma (SET IN STONE, operador)

- **Ranuras** ×2, enfrentadas, alineadas **N/S** — al girar, cuando el filo
  de la hoja apunta al **norte**, la ranura norte queda bajo el sensor
  (refl colapse a 0 medido en el ancla).
- **Chaflanes** ×2, planos E/O, anchos distintos: el **ancho = índice**,
  el **estrecho = verificación de sentido**. Desambiguan norte/sur.
- **Inversión de engranajes**: motor `CW` = hoja gira `CCW` (tractor
  pinion → holder pinion, dos engranajes).
- **Posición de lectura (SET IN STONE)**: plunger Z2 en el **stall
  inferior**. Con el holder arriba hay material absorbedor enfrente del
  sensor → lecturas en blanco (0s medidos en el TEST del tope superior).
- El **filo** de la hoja (triangular) define "hacia dónde mira".

## Anclas calibradas (operador-verificadas)

| Referencia | a (encoder) | refl |
|---|---|---|
| Filo SUR | -273,645 | 66↔0 (borde de ranura) |
| **Filo NORTE = home preferente** | -287,359 | **0** (ranura norte bajo el sensor) |

## Sensor ISL (I2C bit-bang RA2/RA3): hallazgos del día

1. **La lectura 0x09 antes de 0x0A es OBLIGATORIA** — el sensor lanza el
   dato en el par: sin 0x09 previo, 0x0A devuelve valor rancio
   (`pure0A=3` vs `after09: 09=60 0A=167`, comando `ch` del spike).
2. **Bus a ~100 kHz**: `dly()` 2400 → 100 iteraciones. La lectura completa
   baja de ~20-37 ms a ~15 ms — el límite restante lo impone el sensor
   (clock-stretching de SCL), no el bus.
3. **Umbrales ABSOLUTOS frágiles**: la meseta del holder deriva entre
   momentos de la sesión (69-87 → 95-147); los dips profundos (0) de una
   captura no aparecieron necesariamente en la siguiente. Acción en
   `a_home`: umbrales **dinámicos** (medir base en el seek y disparar por
   caída relativa), no fijos (A_HOME_SLOT_REFL=30 / INDEX_REFL=70).

## Protocolo de medición que funciona (validado hoy)

- STOP-GO: pulso `a cw 60 2000` (~550 st ≈ 7°) + pausa ≥0.5s (asentar)
  + 1 lectura. ~52 paradas = 1 rev (~1 min).
- Lecturas estáticas nítidas; muestreo en GIRO CONTINUO es inútil
  (el sensor integrate a su ritmo y cada muestra promedia ~12-25 st).
- `arm` ANTES de cada lote de A (un reboot lo limpia: `a_armed` es un
  flag estático).
- cooldown ≥1s entre pulsos largos; tandas cortas toleran pausas menores.

## Pendiente (próxima sesión, máquina fría)

1. Verificar estabilidad de la firma en frío vs caliente (dips presentes
   o no según warmup del LED/sensor).
2. Umbrales dinámicos en `a_home` (seek → medir base → dips relativos).
3. `$HA` end-to-end con las constantes nuevas (27428) y home = filo
   norte; comparar contra esta ancla visual.
4. build develop con el fix `dly()` (solo cambió el .c, -Werror).
