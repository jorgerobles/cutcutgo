/**
 * Plant physics tests: DC motor coherence + quadrature/Gray compatibility.
 *
 * The Gray matrix below is a LOCAL COPY of the firmware decoder
 * (cutcutgo.X/src/hal/motor.c:694-760, with M_CW=1 / M_CCW=-1 / M_NULL=0 /
 * M_ERR=2 from motor.c:18-21). The plant generates, the test decodes:
 * the plant must never contain the matrix itself (compile-the-real-code
 * boundary).
 *
 * Determinism: every case is seedless and wall-clock free; running any case
 * twice produces bit-identical numbers (no rand() anywhere in the plant).
 */

#include <string.h>

#include "unity.h"

#include "dc_motor.h"
#include "quadrature.h"
#include "test_registry.h"

/* Firmware transition codes (motor.c:18-21). */
#define M_NULL  0
#define M_CW    1
#define M_CCW   (-1)
#define M_ERR   2

/* Local copy of the firmware Gray state_matrix (motor.c:700-705). */
static const int8_t fw_state_matrix[4][4] = {
    {M_NULL, M_CW,   M_CCW,  M_ERR},
    {M_CCW,  M_NULL, M_ERR,  M_CW},
    {M_CW,   M_ERR,  M_NULL, M_CCW},
    {M_ERR,  M_CCW,  M_CW,   M_NULL},
};

/* Decode one encoder transition exactly like hal_motor_update_encoder_state. */
static int fw_decode(uint8_t prev, uint8_t cur)
{
    return fw_state_matrix[prev & 3u][cur & 3u];
}

/* 1) Fixed voltage -> omega converges to V/ke within 5 % in finite time
 *    (coherent with V = R*i + ke*omega, J*domega/dt = kt*i - B*omega). */
static void test_dc_reaches_steady_state(void)
{
    dc_motor_t m;
    dc_params_t p = dc_params_default();
    const float v = 12.0f;
    const float w_target = v / p.ke;
    const int ticks = 1000;          /* 1 s: ~60 mechanical time constants */
    int64_t counts_mid;
    float omega_mid;
    int t;

    /* Determinism probe: bit-identical replay of the same trajectory. */
    dc_init(&m, &p);
    dc_set_voltage(&m, v);
    for (t = 0; t < 500; t++)
        dc_tick(&m);
    counts_mid = m.counts;
    omega_mid = m.omega;
    for (t = 500; t < ticks; t++)
        dc_tick(&m);

    TEST_ASSERT_FLOAT_WITHIN(0.05f * w_target, w_target, m.omega);

    dc_init(&m, &p);
    dc_set_voltage(&m, v);
    for (t = 0; t < 500; t++)
        dc_tick(&m);
    TEST_ASSERT_EQUAL_INT64(counts_mid, m.counts);
    TEST_ASSERT_EQUAL_MEMORY(&omega_mid, &m.omega, sizeof(float));
    for (t = 500; t < ticks; t++)
        dc_tick(&m);
    TEST_ASSERT_FLOAT_WITHIN(0.05f * w_target, w_target, m.omega);
}

/* 2) Same voltage with a bigger load torque -> lower steady-state speed
 *    (J*domega/dt = kt*i - B*omega - Tload: physics, not tuning). */
static void test_dc_load_reduces_speed(void)
{
    dc_motor_t unloaded;
    dc_motor_t loaded;
    dc_params_t p = dc_params_default();
    const int ticks = 1000;
    int t;

    dc_init(&unloaded, &p);
    dc_set_voltage(&unloaded, 12.0f);

    dc_init(&loaded, &p);
    dc_set_voltage(&loaded, 12.0f);
    dc_set_load(&loaded, 0.02f);

    for (t = 0; t < ticks; t++)
    {
        dc_tick(&unloaded);
        dc_tick(&loaded);
    }

    TEST_ASSERT_TRUE(unloaded.omega > 0.0f);
    TEST_ASSERT_TRUE(loaded.omega > 0.0f);
    TEST_ASSERT_TRUE(loaded.omega < unloaded.omega);
}

/* 3) N CW steps decoded with the firmware matrix -> exactly +N counts;
 *    N CCW steps -> back to 0. Zero M_ERR transitions on valid sequences,
 *    in both directions (must-have: generated counts == decoded counts). */
static void test_quadrature_gray_roundtrip(void)
{
    const int n = 1000;              /* 250 full Gray cycles, wraps fine */
    uint8_t prev;
    uint8_t cur;
    int counts = 0;
    int i;
    int dir;

    quad_reset();
    prev = 0u;                       /* nibble after reset */

    for (i = 0; i < n; i++)
    {
        cur = quad_step(+1);
        dir = fw_decode(prev, cur);
        TEST_ASSERT_EQUAL_INT(M_CW, dir);
        counts++;
        prev = cur;
    }
    TEST_ASSERT_EQUAL_INT(n, counts);

    for (i = 0; i < n; i++)
    {
        cur = quad_step(-1);
        dir = fw_decode(prev, cur);
        TEST_ASSERT_EQUAL_INT(M_CCW, dir);
        counts--;
        prev = cur;
    }
    TEST_ASSERT_EQUAL_INT(0, counts);
}

/* 4) quad_inject_error followed by a step -> the single transition decodes
 *    as M_ERR (the firmware would count one error_step, motor.c:721-729),
 *    and the sequence is valid again afterwards. */
static void test_quadrature_error_injection(void)
{
    uint8_t prev;
    uint8_t cur;

    quad_reset();
    prev = quad_step(+1);
    TEST_ASSERT_EQUAL_INT(M_CW, fw_decode(0u, prev));

    quad_inject_error();
    cur = quad_step(+1);
    TEST_ASSERT_EQUAL_INT(M_ERR, fw_decode(prev, cur));

    /* Nibble is a valid Gray state again: next step decodes CW. */
    prev = cur;
    cur = quad_step(+1);
    TEST_ASSERT_EQUAL_INT(M_CW, fw_decode(prev, cur));

    /* Same behaviour in the CCW direction. */
    quad_reset();
    prev = quad_step(-1);
    TEST_ASSERT_EQUAL_INT(M_CCW, fw_decode(0u, prev));
    quad_inject_error();
    cur = quad_step(-1);
    TEST_ASSERT_EQUAL_INT(M_ERR, fw_decode(prev, cur));
}

__attribute__((constructor)) static void register_test_plant(void)
{
    sim_register_test("test_dc_reaches_steady_state",
                      test_dc_reaches_steady_state);
    sim_register_test("test_dc_load_reduces_speed",
                      test_dc_load_reduces_speed);
    sim_register_test("test_quadrature_gray_roundtrip",
                      test_quadrature_gray_roundtrip);
    sim_register_test("test_quadrature_error_injection",
                      test_quadrature_error_injection);
}
