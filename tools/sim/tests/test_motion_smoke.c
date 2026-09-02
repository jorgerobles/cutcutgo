/**
 * Open-loop motion smoke tests over the plant + 1 ms tick harness.
 *
 * These cases validate the BANCADA (deterministic plant + tick harness),
 * NOT control algorithms: the hook below is a trivial open-loop voltage
 * source. The REAL motion/stall/homing algorithms plug into exactly this
 * harness in phases 2-3 via sim_register_1ms_hook() (BUILD-03 phase-1
 * scope, RESEARCH.md:147) — nothing here is a substitute for them.
 *
 * No randomness anywhere: bit-for-bit repeatability is a hard requirement
 * so phases 2-3 can calibrate thresholds in sim (T-1-12).
 */

#include <stdint.h>

#include "unity.h"

#include "dc_motor.h"
#include "quadrature.h"
#include "sim_tick.h"
#include "test_registry.h"

#define DC_TWO_PI 6.2831853f

typedef struct
{
    dc_motor_t m;
    int64_t prev_counts;
    int64_t min_delta;
    int64_t max_delta;
    uint32_t calls;          /* hook invocations (must equal ticks) */
} open_loop_ctx_t;

static open_loop_ctx_t open_loop_ctx;
static open_loop_ctx_t repeat_ctx_a;
static open_loop_ctx_t repeat_ctx_b;

/* Open-loop hook: advance the plant one tick and record the per-tick
 * counts delta (firmware-like ordering: plant tick first, encoder read
 * right after). */
static void open_loop_hook(void *arg)
{
    open_loop_ctx_t *c = (open_loop_ctx_t *)arg;
    int64_t delta;

    dc_tick(&c->m);
    delta = c->m.counts - c->prev_counts;
    c->prev_counts = c->m.counts;

    if (c->calls == 0u)
    {
        c->min_delta = delta;
        c->max_delta = delta;
    }
    else
    {
        if (delta < c->min_delta)
            c->min_delta = delta;
        if (delta > c->max_delta)
            c->max_delta = delta;
    }

    c->calls++;
}

/* 1) M ms of fixed voltage: counts accumulate positive and monotonic,
 *    the hook fires exactly once per ms, and the instantaneous rate never
 *    exceeds the physical maximum derived from omega_max = V/ke
 *    (integration sanity: omega_inf <= V/ke always). */
static void test_open_loop_reaches_target(void)
{
    dc_params_t p = dc_params_default();
    const float v = 12.0f;
    const uint32_t ticks = 1000;     /* 1 s open loop */
    /* Physical per-tick bound: w_max*dt*counts_per_rev*reduction/(2pi),
     * plus one count of truncation slack. */
    const int64_t delta_bound =
        (int64_t)((v / p.ke) * 1.0e-3f * (p.counts_per_rev * p.reduction)
                  / DC_TWO_PI) + 1;

    sim_reset_clock();
    dc_init(&open_loop_ctx.m, &p);
    dc_set_voltage(&open_loop_ctx.m, v);
    open_loop_ctx.prev_counts = 0;
    open_loop_ctx.min_delta = 0;
    open_loop_ctx.max_delta = 0;
    open_loop_ctx.calls = 0u;

    sim_register_1ms_hook(open_loop_hook, &open_loop_ctx);
    sim_run_ticks(ticks);

    /* 1 ms semantics: hook invoked exactly n times, clock advanced n ms. */
    TEST_ASSERT_EQUAL_UINT32(ticks, open_loop_ctx.calls);
    TEST_ASSERT_EQUAL_UINT32(ticks, sim_now_ms());

    /* Reached target direction: positive accumulated counts. */
    TEST_ASSERT_TRUE(open_loop_ctx.m.counts > 0);

    /* Monotonic growth in the positive direction. */
    TEST_ASSERT_TRUE(open_loop_ctx.min_delta >= 0);

    /* Instantaneous rate below the omega_max = V/ke physical bound. */
    TEST_ASSERT_TRUE(open_loop_ctx.max_delta <= delta_bound);
}

/* Trajectory of run_sequence(): counts sampled every 100 ticks. */
#define REPEAT_TICKS   800u
#define REPEAT_SAMPLES 8

static void run_sequence(open_loop_ctx_t *c, int64_t *trajectory)
{
    dc_params_t p = dc_params_default();
    uint32_t half;
    int i;

    sim_reset_clock();
    dc_init(&c->m, &p);
    dc_set_voltage(&c->m, 6.0f);
    dc_set_load(&c->m, 0.005f);
    c->prev_counts = 0;
    c->min_delta = 0;
    c->max_delta = 0;
    c->calls = 0u;

    sim_register_1ms_hook(open_loop_hook, c);

    for (i = 0; i < REPEAT_SAMPLES; i++)
    {
        half = REPEAT_TICKS / (uint32_t)REPEAT_SAMPLES;
        sim_run_ticks(half);
        trajectory[i] = c->m.counts;
    }
}

/* 2) The SAME sequence twice (same voltage, same ticks, same load) gives
 *    bit-identical counts — precondition for phases 2-3 to calibrate
 *    thresholds in sim. */
static void test_deterministic_repeat(void)
{
    int64_t run_a[REPEAT_SAMPLES];
    int64_t run_b[REPEAT_SAMPLES];

    run_sequence(&repeat_ctx_a, run_a);
    run_sequence(&repeat_ctx_b, run_b);

    /* Something actually moved. */
    TEST_ASSERT_TRUE(run_a[REPEAT_SAMPLES - 1] > 0);

    /* Bit-for-bit identical trajectories. */
    TEST_ASSERT_EQUAL_INT64_ARRAY(run_a, run_b, REPEAT_SAMPLES);
}

__attribute__((constructor)) static void register_test_motion_smoke(void)
{
    sim_register_test("test_open_loop_reaches_target",
                      test_open_loop_reaches_target);
    sim_register_test("test_deterministic_repeat",
                      test_deterministic_repeat);
}
