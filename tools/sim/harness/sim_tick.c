#include "sim_tick.h"

/* Fixed hook pool, same shape as the firmware soft-timer pool
 * (cutcutgo.X/src/hal/timer.c TIMER_POOL_SIZE). */
#define SIM_TICK_MAX_HOOKS 8

typedef struct
{
    void (*fn)(void *ctx);
    void *ctx;
} sim_hook_t;

static sim_hook_t hooks[SIM_TICK_MAX_HOOKS];
static int hook_count;
static uint32_t now_ms;

void sim_reset_clock(void)
{
    hook_count = 0;
    now_ms = 0u;
}

void sim_register_1ms_hook(void (*fn)(void *ctx), void *ctx)
{
    if ((fn == 0) || (hook_count >= SIM_TICK_MAX_HOOKS))
        return;

    hooks[hook_count].fn = fn;
    hooks[hook_count].ctx = ctx;
    hook_count++;
}

void sim_run_ticks(uint32_t n)
{
    uint32_t t;
    int i;

    for (t = 0u; t < n; t++)
    {
        /* Complete the millisecond first, then dispatch: hooks observe a
         * coherent sim_now_ms(), one invocation per hook per tick. */
        now_ms++;

        for (i = 0; i < hook_count; i++)
            hooks[i].fn(hooks[i].ctx);
    }
}

uint32_t sim_now_ms(void)
{
    return now_ms;
}
