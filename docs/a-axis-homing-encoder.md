# Homing del eje A — Encoder angular óptico del portahojas

Fecha: 2026-09-14 (sesión 4). Estado: **validado en hardware** con spike firmware.
Pendiente: portar a GRBL (`blade_home.c`) y endurecer umbrales con una rev limpia densa.

## Descubrimiento

El portahojas de la Cricut Maker 1 tiene, a la altura del sensor óptico (ISL29125,
I2C 0x44), una geometría NO circular que actúa como **encoder absoluto de un elemento**:

- **Chaflanes pulidos** (planos, E/W): reflejan especularmente → picos de refl.
  Hay **dos**, de anchos distintos (uno ancho = índice, uno estrecho).
- **Ranuras** (N/S): líneas oscuras → dips de refl. Dos, enfrentadas.

Cadena óptica: emisor IR (siempre ON) → superficie del holder → tubo de luz (knob
inferior) → ISL29125 → canal GREEN (reg 0x0A, byte alto = valor `refl` de telemetría).

## Firma medida (motor A parado en cada muestra, jogs de 150-300ms a PWM 2000)

```
refl
63 |      ★  ← PICO chaflán ANCHO (retro-reflexión) = ÍNDICE
56 |
47 |                                                    · ← bump estrecho
39 |  ······        ····································    (base 35-43)
35 | ·····
31 |                                                    ····
23 |       ·    ·                                        ····
19 |       ·   · ← RANURA 2
 7 |       · ← RANURA 1
   +------------------------------------------------------------→ a (steps)
        -62252   -79609                    (distancia ranura→ranura ≈ 17350)
```

| Feature | refl | Ancho aprox | Función |
|---|---|---|---|
| Base (curva holder) | 31-43 | resto de la rev | línea base |
| Ranura ×2 | **7-23** | estrecha (1-2 muestras de 1200 steps) | marcador |
| Chaflán ancho | **>55 (63 medido)** | estrecho | **ÍNDICE 0°** |
| Chaflán estrecho | ~39-47 (bump) | estrecho | verificación de sentido |

- Periodo: **≈ 17350 encoder steps / revolución de hoja**.
- Secuencia índice: ranura → ~3500 steps → pico del chaflán ancho.
- Span total de señal: 56 counts (7-63) — SNR cómodo para umbrales.

## Algoritmo de homing (validado)

```
1. PRECONDICIÓN: Z2 en tope stall (hoja arriba, nunca rotar con hoja bajada).
   Verificar con jog corto: dz ≤ 150 = parado.
2. Rotar A en PULSOS: 150-600ms a PWM 2000, pausa ≥1s entre pulsos
   (evita latch térmico del A4950; con motor parado refl es muestra puntual
   sin smear de integración).
3. TRIGGER 1: refl < 15  → ranura detectada. Anotar posición a_slot.
4. Seguir pulsando: TRIGGER 2: refl > 55 → chaflán ancho. Anotar a_index.
   (Si entre ambos aparece refl>50 antes de 2500 steps → falso índice,
    descartar; el chaflán ancho va SIEMPRE ~3500 steps tras la ranura.)
5. HOME = centro del pico del chaflán ancho (bisecar con pulsos finos de 100ms,
   opcional para precisión).
6. VERIFICACIÓN: girando CCW desde home debe aparecer bump estrecho (refl 39-47)
   y después ranura; girando CW, ranura a ~3500 steps.
```

### Umbrales iniciales (a re-calibrar con rev densa)
- RANURA: refl < 15  (medido 7-23)
- INDICE: refl > 55  (medido 63; base 35-43)
- Muerta: 25 < refl < 50 = fuera de feature

### Constantes del eje (esta unidad)
| Magnitud | Valor |
|---|---|
| Steps encoder / rev hoja | ~17350 |
| Ranura → chaflán ancho | ~3500 steps |
| PWM giro lento (jogs) | 2000 (≈ 6000-7000 steps/s) |
| Pwm pulse duration | 150-600ms, cooldown ≥1s |
| Z2 travel total | ~6900-7000 steps (tope↔tope) |
| Ventana refl Z2 (hoja visible) | 0-2500 steps desde tope |

## Restricciones de seguridad (duras)

1. **NUNCA rotar A con la hoja bajada** — confirmado: a ~2900 steps del tope el
   motor apenas gira (la hoja roza el collar). Rotación solo en tope stall.
2. **Pulsos, no PWM sostenido** — el A4950 entra en **latch térmico** con stalls
   sostenidos (>1-2s) y no recupera hasta power-cycle. Con pulsos + cooldown no
   se disparó en ninguna prueba.
3. El homing debe abortar si: refl no varía tras 1.5 rev (óptica tapada), dz de
   Z2 cambia durante la rotación (la hoja se mueve), o el driver deja de contar
   encoder (latch térmico).

## Por qué este diseño funciona como homing absoluto

Los dos chaflanes de ancho distinto + las dos ranuras dan una secuencia
asimétrica única por revolución: `ranura → (3500) → PICO-ancho → ... → bump →
ranura2 → ...`. No hay otra ventana angular con refl>55: **el pico es índice
absoluto** (no hace falta contar revoluciones ni referencia previa).

## Notas de implementación GRBL (pendiente)

- `blade_home.c` ya tiene la estructura de máquina de estados; añadir estados:
  `AHOME_SCAN` (pulsos + muestreo), `AHOME_SLOT` (esperar ranura),
  `AHOME_INDEX` (esperar pico), `AHOME_VERIFY` (bump + ranura CCW).
- Muestreo refl: `blade_detector_read()` tras el priming 0x09→0x0A (~36ms/muestra;
  sobra: los features duran ≥1 muestra de 1200 steps).
- Z2 homing por stall ya funciona con el patrón jog+verify-dz.
- El tool-check (presencia de hoja) = refl en fondo-stall (43-51) vs tope (35-43):
  débil — mantener también la detección por umbral 50 si la alineación óptica
  mejora, o combinar ambas.
