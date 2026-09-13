# Spike: motor A (hoja) vs. Z2 — verificación de pines y detección de hoja

Status: **pendiente de ejecución** (nota esqueleto, sin datos de máquina).
Propósito: verificar en hardware la asignación de pines de Z2 / A y comprobar que el
eje A (wormgear de rotación de hoja, sobre el eje Z2) puede moverse sin interrumpir la
detección de hoja.

> Docs-only: esta nota es el esqueleto de un spike de hardware. Los valores marcados
> con `______` se rellenan **durante** la sesión supervisada sobre la máquina.

## 1. Objetivo

- Verificar la asignación de pines **Z2 / A** en hardware (bare metal, firmware de
  depuración con `$DBG` + jogging manual).
- Mover el **A** (rotación de hoja, deuter metal) mientras la detección de hoja sigue
  emitiendo reportes `[BLADE:*]`.
- Recomendar si el **A** debe promoverse a un **eje completo de G-code** (gated sobre
  la evidencia obtenida en el spike).

## 2. Seguridad

- **Sesión supervisada únicamente** (no correr sin operador presente).
- Mover **solo A** (rotación de hoja) y **milisegundos de Z2** (jog cortísimo).
- **Nunca X / Y** durante este spike (masa de corte no probada).
- **Abort = soft reset** (`ctrl-x`) o **apagado** de la unidad.
- El **A queda desenergizado en idle** (A4950 en brake/hold controlado); no queda
  energizado entre comandos.

## 3. Verificación de pines Z2 / A

Comparar los dos conjuntos de pines candidatos contra el comportamiento real de la
máquina. Rellenar los resultados observados durante la sesión.

| Etapa | Acción | Resultado observado |
|-------|--------|---------------------|
| Baseline | Detección de hoja en idle (reportes `[BLADE:*]` streaming) | `______` |
| Jog candidato Z2 | Jog del conjunto `MOTOR_Z2_*` | `______` (mueve carriage / solo hoja) |
| Jog candidato A | Jog del conjunto `MOTOR_A_*` | `______` (hoja / carriage) |
| Conclusión | ¿asignación correcta o intercambiada? | `______` |

Pines candidatos (de `hal/config.h`):

| Motriz | IN1 | IN2 | ENC_A | ENC_B | OCM |
|--------|-----|-----|-------|-------|-----|
| Candidato Z2 (`MOTOR_Z2_*`) | RB6 | RD5 | RG14 | RG15 | 2 |
| Candidato A (`MOTOR_A_*`) | RD3 | RD11 | RG8 | RG9 | 4 |

## 4. Prueba A + detección de hoja

Con la detección de hoja emitiendo reportes (`[BLADE:*]`), mover A en ambas
direcciones y comprobar que el stream del detector no se interrumpe.

| Dirección | Comando | Resultado stream `[BLADE:*]` | `BLADE:LOSS` | Z counters |
|-----------|---------|------------------------------|--------------|------------|
| A CW | `$DBGMOTOR=A,CW,<ms>` | `______` | no / sí | `______` |
| A CCW | `$DBGMOTOR=A,CCW,<ms>` | `______` | no / sí | `______` |

Criterios de paso:
- Stream del detector **sin interrupciones** durante todo el jog.
- **Sin** `BLADE:LOSS`.
- **Contadores Z intactos** (sin avance por el jog de A).

## Hallazgos (sesión en curso — firmware spike `hal/spike.c`)

- **A (RD3/RD11, OCM4) = rotación de hoja (wormgear) CONFIRMADO** — sesión 1: -43.675 enc steps, hoja rotando.
- **Z2 (RB6/RD5, OCM2) = émbolo/portaherramientas (subir/bajar) CONFIRMADO** — CW = SUBIR.
- **Calibración Z2**: sonda 150 ms ≈ **2 mm** (1.715 steps) + lote 3×500 ms ≈ **5 mm** (3.816 steps) = **7 mm totales** (~5.531 steps @1800) → **~790 steps/mm** (aprox, ~1.200 steps/mm nominal en dosis).
- **Fin de carrera superior de Z2**: total recorrido hasta tope ≈ **7 mm** desde la posición inicial — stall mecánico contra tope (worm autofrenante).
- **Velocidades**: duty 2300 (SPEED_MIN) NO mueve eje cargado (causa del silencio de `$DBGMOTOR`); 1800-2000 mueve.
- **Z1 (RF12/RF8)**: encoder cuenta (1.942) pero sin efecto visible reportado (pendiente confirmar).
- **X**: control positivo OK (jog directo @2000).
- **Detector de hoja**: `blade=0` durante toda la sesión, sin fallos.

## 5. Hallazgos

- **Asignación de pines final**: `______`
- **Dirección / velocidad / encoder de A**: `______`
- **Comportamiento de detección de hoja** al mover A: `______`

## Abiertos (nuevos, sesión 2)

- **Home de A / ambigüedad de fase**: un triángulo da oclusión máxima a 0/120/240° — ¿cómo desambigua la máquina? Hipótesis: (1) amplitud distinta por cara (recubrimiento vs acero), (2) home por flanco de transición (vértice→plano) y no por máximo, (3) referencia de fase del portaherramientas. Experimento previsto: rotar A 360° a Z fija muestreando canales → forma de onda de oclusión real.
- **LED IR no encontrado**: 33 GPIO + CFG1/CFG2 completos sin enable; soporte Cricut confirma "LED IR propio del detector lateral". Pistas: Q7/POWER_STATE_OUT (pin MCU POWER_TRIGGER no identificado en el esquemático), LED muerto en esta unidad, o pulsado solo en rutinas stock. Ch10 responde a luz ambiente: claro=7, ocluido=3.
- **Geometría confirmada con fotos**: sensor mira lateral al hueco del portaherramientas B, reflector enfrente, punta de hoja cruza el hueco; engranaje latón superior = eje A.

## 6. Recomendación

- **¿Promover A a un eje completo de G-code?** `______` (gated sobre la evidencia de
  la sección 4: sin pérdidas de detección y sin interferencia en Z).

## 7. Nota sobre inconsistencia preexistente

Preservada sin cambios en el rename de símbolos (task 3.x):

- `st_select_tool()` → tool 1 (`mark`) asocia `DEFAULT_Z1_STEPS_PER_MM`.
- `st_prep_buffer()` → TOOL2 (blade) asocia `DEFAULT_Z2_STEPS_PER_MM` para `step_dtz`.

Es decir, los **rates se asocian de forma cruzada** respecto a la nueva
vocabularía Z1/Z2/A. **A confirmar en el spike** si esta asociación cruzada es la
comportamiento esperado o introduce desviaciones medibles en `step_dtz`.
