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
- **LED IR no encontrado** (cerrado por ahora): 33 GPIO (todas lasbandas RE/RC/RA/RB/RD/RF/RG candidatas) + CFG1/CFG2 completos sin enable; soporte Cricut confirma "LED IR propio del detector lateral". Sin él, el detector lateral NO detecta: en la inmersión con el collar ocluyendo visiblemente el hueco, ch10 NO cambió (7→7). Pista restante: traza física de continuidad desde puerta de Q7 (POWER_TRIGGER) hasta el MCU, o consultar a virtualabs.
- **Geometría de detección real**: al sumergir el portaherramientas, es el COLLAR de agarre (no la punta) lo que cruza la ventana lateral → la firma de detección stock probablemente sea la oclusión del collar con LED IR propio.
- **Geometría confirmada con fotos**: sensor mira lateral al hueco del portaherramientas B, reflector enfrente, punta de hoja cruza el hueco; engranaje latón superior = eje A.

- **Mapa de registros del chip de cabeza (0x44)**: espacio real de 32 registros (0x00-0x1F), alias módulo 32 por encima; reg1=CFG1, reg2=CFG2 (bits bajos leen como 3), reg6/7=0xFFFF fijo (saturación o constante 16-bit), reg8=7, reg30=0x7D (espejo ID). El receptor IR no aparece en ninguno.
- **Receptor IR no localizado** (siguiente sesión): no está en niveles estáticos de 14 líneas del ribbon ni en registros I2C. Hipótesis fuerte: salida modulada/frecuencia (fotodiodo inmune a luz ambiente) → hace falta muestreo de transiciones por línea, scope/multímetro en la PCB de cabeza, o las notas de virtualabs.
- **Emisor IR confirmado vivo y siempre encendido** (foto con cámara de móvil: brillo rosa en el knob superior).

- **BREAKING: el chip 0x44 es un ISL29125 real** (ID 0x7D = firma ISL29125; CFG1=0x0D = "RGB,16-bit,375lux,start"). Regs 0x08-0x0D = pares 16-bit: verde(8,9), rojo(10,11), azul(12,13). Relectura de los diffs llave-in/out con esa estructura: verde 512→257, rojo 263→259 con llave puesta → respuesta óptica REAL del hueco lateral por los canales RGB. No hay "línea receptora" separada: el ISL29125 es el receptor de ambas rutas ópticas (tubos de luz).
- **Teoría de detección stock (a verificar)**: "spin test" = muestrear RGB mientras A rota la hoja → modulación por el triángulo (hipótesis original del operador). Siguiente sesión: burst-sample de los 3 canales durante rotación lenta de A con hoja real → forma de onda → umbrales → integración en blade_home. (Descartadas: TCS3472/0x29/EEPROM 0x50 de la conversación con otra IA — contradicen nuestras mediciones: único respondedor 0x44, ID 0x7D.)

- **CONFIRMADO CON DATASHEET (Renesas ISL29125)**: mapa oficial calza 1:1 con nuestras lecturas — ID 0x00=0x7D; CFG1 0x01 (SYNC/BITS/RNG/MODE, escribimos 0x0D); CFG2 0x02 (IRCOMP bit7 + ALSCC[5:0] — la variante Cricut NACKA el bit7: IRComp retirado); umbral-alto 0x06/07 default 0xFF/0xFF (nuestro "255,255"); STATUS 0x08 (RGBTHF/BOUTF/CONVENF — nuestro "8=7"); GREEN 0x09/0x0A, RED 0x0B/0x0C, BLUE 0x0D/0x0E (16-bit cada uno).
- **CANAL VERDE = ruta lateral de hoja**: llave dentro verde=0x0301 (769) vs fuera 0x0700 (1792) — 2.3x. El "canal 0x0A" histórico del mark detector = byte alto GREEN. El firmware stock probablemente: fija umbrales (0x04-0x07) + INTSEL y sondea STATUS/RGBTHF durante el giro de A.
- **Diseño de detección propio (próxima sesión)**: leer GREEN 16-bit (regs 9,10) como canal de hoja; calibrar umbrales con hoja arriba/abajo y rotación de A; evaluar interrupción por umbral (CFG3 INTSEL) en vez de polling.

- **¿Promover A a un eje completo de G-code?** `______` (gated sobre la evidencia de
  la sección 4: sin pérdidas de detección y sin interferencia en Z).

## PRIMERA TAREA PRÓXIMA SESIÓN: bench 'S' (histograma de transiciones)

Contexto: emisor IR siempre encendido (foto), ISL29125 ve el hueco por GREEN (769 vs 1792 con llave), pero 14 líneas del ribbon leídas como niveles estáticos no se movieron. Lo que puede estar colándose:
1. **Niveles analógicos** — un fototransistor a media tensión leído por GPIO digital colapsa a un 0/1 constante.
2. **Pulsos/frecuencia** — los pares "encoder" de J500 son lecturas ópticas del portaherramientas; la señal son transiciones, invisible a un dump de niveles.
3. **Interacción dinámica** — nunca giramos A mientras leíamos esas líneas.

**Ajuste del operador (más probable: analógico)**: el ADC ya está en el juego — el propio ISL29125 es el lector analógico (GREEN respondió a la llave). Las líneas RG del ribbon son digital-only (diseño = señal digital), así que el test de atribución va ANTES del bench 'S':

TEST DE ATRIBUCIÓN (sin movimiento, sin firmware nuevo): con `c` en marcha — (1) cartulina tapando SOLO la ventana lateral → si GREEN cae, la ruta lateral es el canal verde, confirmado; (2) tapando SOLO la ventana inferior (mark) → si GREEN no cambia, separación limpia de rutas. Con eso: umbrales GREEN para hoja arriba/abajo y rotación de A.

Bench 'S' en spike.c (solo si la atribución falla): sondeo continuo de las 14 líneas candidatas contando CAMBIOS de nivel por pin, en 3 ventanas de 1 s: (a) baseline sin girar, (b) A girando lento (cw @1900), (c) parado. Histograma final. Toda línea con transiciones correlacionadas con el giro = lector IR. Requisito: hoja/key EN el hueco (bajar Z2 a la ventana). Beep + GO, sin pasarse del fin de carrera.

## 7. Nota sobre inconsistencia preexistente

Preservada sin cambios en el rename de símbolos (task 3.x):

- `st_select_tool()` → tool 1 (`mark`) asocia `DEFAULT_Z1_STEPS_PER_MM`.
- `st_prep_buffer()` → TOOL2 (blade) asocia `DEFAULT_Z2_STEPS_PER_MM` para `step_dtz`.

Es decir, los **rates se asocian de forma cruzada** respecto a la nueva
vocabularía Z1/Z2/A. **A confirmar en el spike** si esta asociación cruzada es la
comportamiento esperado o introduce desviaciones medibles en `step_dtz`.
