/**
 * Encoder dispatch isolation tests (compile-the-real-code).
 *
 * Exercises the REAL firmware dispatch (hal/motor_encoder.c) with the
 * quadrature plant generating the Gray sequence: motion on one motor's
 * encoder pins must touch only that motor's counters (and only that
 * motor's axis in the limits log) — the guarantee blade detection relies
 * on while the A axis rotates.
 *
 * Pin layout mirrors the firmware map (port G bits):
 *   Z1 (marker) = RG12/13, Z2 (blade) = RG14/15, A (wormgear) = RG8/9.
 * Registration order mirrors hal_motor_lookup_init().
 */

#include <string.h>

#include "unity.h"

#include "hal/motor_encoder.h"
#include "quadrature.h"
#include "sim_limits_stub.h"
#include "sim_motor_stubs.h"
#include "test_registry.h"

#define ENC_Z1 12
#define ENC_Z2 14
#define ENC_A  8

static hal_motor_driver_t m_z1;
static hal_motor_driver_t m_z2;
static hal_motor_driver_t m_a;

static void fixture(void)
{
    hal_motor_lookup_clear();
    sim_limits_reset();
    sim_motor_reset();
    quad_reset();

    memset(&m_z1, 0, sizeof(m_z1));
    memset(&m_z2, 0, sizeof(m_z2));
    memset(&m_a, 0, sizeof(m_a));

    m_z1.encA = ENC_Z1; m_z1.encB = ENC_Z1 + 1;
    m_z2.encA = ENC_Z2; m_z2.encB = ENC_Z2 + 1;
    m_a.encA  = ENC_A;  m_a.encB  = ENC_A + 1;

    /* All motors "driving" CW so decoded CW edges count up. */
    m_z1.direction = HAL_MOTOR_DIR_CW;
    m_z2.direction = HAL_MOTOR_DIR_CW;
    m_a.direction  = HAL_MOTOR_DIR_CW;

    hal_motor_lookup_register(&m_z1);
    hal_motor_lookup_register(&m_z2);
    hal_motor_lookup_register(&m_a);
}

/* Feed n encoder transitions on one motor's pin pair through the dispatch. */
static void drive(int enc_a_bit, int dir, int n)
{
    int i;

    for (i = 0; i < n; i++)
    {
        uint32_t portg = (uint32_t)quad_step(dir) << enc_a_bit;
        hal_motor_service_encoders(0x3u << enc_a_bit, portg);
    }
}

/* Isolation: A motion touches only A counters (spec a-axis-control). */
static void test_a_motion_isolation(void)
{
    fixture();
    drive(ENC_A, 1, 10);

    TEST_ASSERT_EQUAL_INT32(10, m_a.current_steps);
    TEST_ASSERT_EQUAL_INT32(10, m_a.rel_pos);
    TEST_ASSERT_EQUAL_INT32(0, m_a.error_steps);

    TEST_ASSERT_EQUAL_INT32(0, m_z1.current_steps);
    TEST_ASSERT_EQUAL_INT32(0, m_z1.rel_pos);
    TEST_ASSERT_EQUAL_INT32(0, m_z2.current_steps);
    TEST_ASSERT_EQUAL_INT32(0, m_z2.rel_pos);
    TEST_ASSERT_EQUAL_INT32(0, m_z2.error_steps);

    /* Limits log: only A's axis may have been reported. */
    TEST_ASSERT_TRUE(sim_limits_count() >= 1);
    for (int i = 0; i < sim_limits_count(); i++)
        TEST_ASSERT_EQUAL_UINT8(m_a.grbl_axis, sim_limits_axis(i));
}

/* CCW motion on Z1 counts down; A/Z2 untouched. */
static void test_z1_ccw_isolation(void)
{
    fixture();
    drive(ENC_Z1, -1, 5);

    TEST_ASSERT_EQUAL_INT32(-5, m_z1.current_steps);
    TEST_ASSERT_EQUAL_INT32(-5, m_z1.rel_pos);
    TEST_ASSERT_EQUAL_INT32(0, m_a.current_steps);
    TEST_ASSERT_EQUAL_INT32(0, m_z2.current_steps);
}

/* Invalid Gray transition counts as error step on the driven motor only. */
static void test_error_injection_isolation(void)
{
    fixture();
    quad_inject_error();
    drive(ENC_A, 1, 1);

    TEST_ASSERT_EQUAL_INT32(1, m_a.error_steps);
    TEST_ASSERT_EQUAL_INT32(0, m_a.current_steps);
    TEST_ASSERT_EQUAL_INT32(0, m_z1.error_steps);
    TEST_ASSERT_EQUAL_INT32(0, m_z2.error_steps);
}

/* No change-notification bits set -> no motor is serviced at all. */
static void test_cnstatg_gating(void)
{
    fixture();
    drive(ENC_A, 1, 3);
    TEST_ASSERT_EQUAL_INT32(3, m_a.current_steps);

    hal_motor_service_encoders(0, 0xFFFFFFFFu);
    TEST_ASSERT_EQUAL_INT32(3, m_a.current_steps);
    TEST_ASSERT_EQUAL_INT32(0, m_z1.current_steps);
    TEST_ASSERT_EQUAL_INT32(0, m_z2.current_steps);
}

/* Braking: a driven motor reaching command_steps is stopped exactly once
 * (the real HAL cuts PWM there, so no further encoder edges would arrive;
 * with synthetic edges the counters keep rising — that is not a firmware
 * concern). */
static void test_brake_on_command_steps(void)
{
    fixture();
    m_z2.state = HAL_MOTOR_DRIVEN;
    m_z2.command_steps = 4;

    drive(ENC_Z2, 1, 10);

    TEST_ASSERT_TRUE(m_z2.current_steps >= 4);
    TEST_ASSERT_EQUAL_INT(HAL_MOTOR_IDLE, m_z2.state);
    TEST_ASSERT_EQUAL_INT(1, sim_motor_dir_calls());
    TEST_ASSERT_EQUAL_PTR(&m_z2, sim_motor_dir_target(0));
    TEST_ASSERT_EQUAL_INT(HAL_MOTOR_STOP, sim_motor_dir_value(0));
    /* Z1/A were idle: no braking calls for them. */
    TEST_ASSERT_EQUAL_INT(HAL_MOTOR_IDLE, m_z1.state);
    TEST_ASSERT_EQUAL_INT(HAL_MOTOR_IDLE, m_a.state);
}

/* Stall watchdog: frozen encoder steps on a driven, armed motor stop it. */
static void test_stall_watchdog_stops_blocked_a(void)
{
    fixture();
    /* A driven, armed, blocked: encoder steps frozen at commanded position. */
    m_a.state = HAL_MOTOR_DRIVEN;
    m_a.wd_armed = true;
    m_a.wd_prev_steps = 5;
    m_a.current_steps = 5;
    m_a.command_steps = 10;

    hal_motor_stall_detection(&m_a);

    TEST_ASSERT_EQUAL_INT(HAL_MOTOR_IDLE, m_a.state);
    TEST_ASSERT_FALSE(m_a.wd_armed);
    TEST_ASSERT_EQUAL_INT(1, sim_motor_dir_calls());
    TEST_ASSERT_EQUAL_PTR(&m_a, sim_motor_dir_target(0));
    TEST_ASSERT_EQUAL_INT(HAL_MOTOR_STOP, sim_motor_dir_value(0));
    /* Stall reported to GRBL on A's axis only. */
    TEST_ASSERT_EQUAL_INT(1, sim_limits_count());
    TEST_ASSERT_EQUAL_UINT8(m_a.grbl_axis, sim_limits_axis(0));
    TEST_ASSERT_TRUE(sim_limits_triggered(0));
}

/* Progressing motor: watchdog re-arms baseline, no stop, no report. */
static void test_stall_watchdog_progressing_motor(void)
{
    fixture();
    m_z1.state = HAL_MOTOR_DRIVEN;
    m_z1.wd_armed = true;
    m_z1.wd_prev_steps = 5;
    m_z1.current_steps = 7;

    hal_motor_stall_detection(&m_z1);

    TEST_ASSERT_EQUAL_INT(HAL_MOTOR_DRIVEN, m_z1.state);
    TEST_ASSERT_TRUE(m_z1.wd_armed);
    TEST_ASSERT_EQUAL_INT(7, m_z1.wd_prev_steps);
    TEST_ASSERT_EQUAL_INT(0, sim_motor_dir_calls());
    TEST_ASSERT_EQUAL_INT(0, sim_limits_count());
}

__attribute__((constructor)) static void register_test_encoder_dispatch(void)
{
    sim_register_test("test_a_motion_isolation", test_a_motion_isolation);
    sim_register_test("test_z1_ccw_isolation", test_z1_ccw_isolation);
    sim_register_test("test_error_injection_isolation", test_error_injection_isolation);
    sim_register_test("test_cnstatg_gating", test_cnstatg_gating);
    sim_register_test("test_brake_on_command_steps", test_brake_on_command_steps);
    sim_register_test("test_stall_watchdog_stops_blocked_a", test_stall_watchdog_stops_blocked_a);
    sim_register_test("test_stall_watchdog_progressing_motor", test_stall_watchdog_progressing_motor);
}
