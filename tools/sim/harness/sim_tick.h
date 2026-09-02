/**
 * Deterministic 1 ms tick harness for the host simulator.
 *
 * Host equivalent of the firmware's Timer1 tick + soft-timer dispatch
 * (cutcutgo.X/src/hal/timer.c:107-110 timer_get_ms over the tick counter)
 * with the watchdog callback as the model 1 ms consumer
 * (cutcutgo.X/src/hal/watchdog.c:9-13). One call to sim_run_ticks(n)
 * invokes every registered hook EXACTLY n times, once per simulated
 * millisecond, and advances the sim clock — identical scheduling semantics,
 * zero wall-clock dependence.
 *
 * This is the future connection point for real control algorithms: phases
 * 2-3 register their motion/stall/homing loops here (the way the firmware
 * registers watchdog_callback) without touching simulator infrastructure.
 * The plant does NOT advance on its own — each tick, the registered hook
 * decides the order of dc_tick() + encoder reads, mirroring the firmware's
 * ISR-decoder -> control-tick ordering.
 */

#ifndef __INC_SIM_HARNESS_SIM_TICK_H
#define __INC_SIM_HARNESS_SIM_TICK_H

#include <stdint.h>

/**
 * @brief   Reset the simulated clock to 0 and unregister every hook
 *          (clean fixture for each test).
 */
void sim_reset_clock(void);

/**
 * @brief   Register a 1 ms hook (called once per simulated millisecond,
 *          in registration order). Fails silently if the hook pool is
 *          full (fixed pool, like the firmware's timer pool).
 *
 * @param   fn   Hook function; must not be NULL.
 * @param   ctx  Opaque context passed back to the hook.
 */
void sim_register_1ms_hook(void (*fn)(void *ctx), void *ctx);

/**
 * @brief   Advance the simulation by n milliseconds: n dispatch rounds,
 *          each invoking every registered hook exactly once. The clock
 *          advances before the hooks of that tick run, so hooks observe
 *          the completed millisecond through sim_now_ms().
 *
 * @param   n  Number of 1 ms ticks to simulate.
 */
void sim_run_ticks(uint32_t n);

/**
 * @brief   Current simulated time in milliseconds since sim_reset_clock().
 */
uint32_t sim_now_ms(void);

#endif /* __INC_SIM_HARNESS_SIM_TICK_H */
