#include <stdint.h>

#include "unity.h"

#include "hal/a_home.h"
#include "test_registry.h"

/* Plant: A position advances a fixed step per pulse; reflectance is a linear
 * signature (slot at [4000,5000), index at [8000,9000), base elsewhere). */

#define PULSE_STEP 900
#define SLOT_LO    4000
#define SLOT_HI    5000
#define INDEX_LO   8000
#define INDEX_HI   9000

static int32_t sim_a;
static int32_t sim_z2;
static int32_t sim_start_a;
static int32_t sim_start_z2;
static uint8_t sim_abort;
static uint8_t sim_fault;
static uint8_t sim_frozen;
static uint8_t sim_no_index;
static int32_t sim_drift_after;
static uint32_t sim_pulses;

static int32_t refl_at(int32_t pos)
{
    if (pos >= SLOT_LO && pos < SLOT_HI)
        return 10;
    if (pos >= INDEX_LO && pos < INDEX_HI)
        return sim_no_index ? 40 : 63;
    return 40;
}

static int32_t c_read_refl(void)
{
    return sim_fault ? -1 : refl_at(sim_a);
}

static int32_t c_read_a(void)
{
    return sim_a;
}

static int32_t c_read_z2(void)
{
    return sim_z2;
}

static void c_pulse(int8_t dir, uint32_t ms)
{
    (void)dir;
    (void)ms;

    sim_pulses++;
    if (!sim_frozen)
        sim_a += PULSE_STEP;
    if (sim_drift_after && sim_pulses >= (uint32_t)sim_drift_after)
        sim_z2 = sim_start_z2 + 1000;
}

static uint8_t c_abort(void)
{
    return sim_abort;
}

static const a_home_ctx_t ctx = {
    c_read_refl, c_read_a, c_read_z2, c_pulse, c_abort
};

static void reset_fixture(void)
{
    sim_a = 0;
    sim_z2 = 0;
    sim_start_a = 0;
    sim_start_z2 = 0;
    sim_abort = 0;
    sim_fault = 0;
    sim_frozen = 0;
    sim_no_index = 0;
    sim_drift_after = 0;
    sim_pulses = 0;
}

static void test_a_home_clean_rev(void)
{
    int32_t home = -1;

    reset_fixture();
    TEST_ASSERT_EQUAL_INT(A_HOME_OK, a_home_run(&ctx, &home));
    TEST_ASSERT_TRUE(home >= INDEX_LO && home < INDEX_HI);
}

static void test_a_home_notrans(void)
{
    int32_t home = -1;

    reset_fixture();
    sim_a = 100000; /* far from any slot/index in this linear plant */
    /* Scan window is 1.5 rev (26025 steps); from 100000 it exhausts. */
    TEST_ASSERT_EQUAL_INT(A_HOME_ERR_NOTRANS, a_home_run(&ctx, &home));
}

static void test_a_home_seek_timeout(void)
{
    int32_t home = -1;

    reset_fixture();
    sim_no_index = 1; /* slot present, index never appears */
    TEST_ASSERT_EQUAL_INT(A_HOME_ERR_NOTRANS, a_home_run(&ctx, &home));
}

static void test_a_home_latch(void)
{
    int32_t home = -1;

    reset_fixture();
    sim_frozen = 1;
    TEST_ASSERT_EQUAL_INT(A_HOME_ERR_LATCH, a_home_run(&ctx, &home));
}

static void test_a_home_drift(void)
{
    int32_t home = -1;

    reset_fixture();
    sim_drift_after = 1;
    TEST_ASSERT_EQUAL_INT(A_HOME_ERR_DRIFT, a_home_run(&ctx, &home));
}

static void test_a_home_fault(void)
{
    int32_t home = -1;

    reset_fixture();
    sim_fault = 1;
    TEST_ASSERT_EQUAL_INT(A_HOME_ERR_FAULT, a_home_run(&ctx, &home));
}

static void test_a_home_abort(void)
{
    int32_t home = -1;

    reset_fixture();
    sim_abort = 1;
    TEST_ASSERT_EQUAL_INT(A_HOME_ERR_ABORT, a_home_run(&ctx, &home));
}

__attribute__((constructor))
static void register_test_a_home(void)
{
    sim_register_test("test_a_home_clean_rev", test_a_home_clean_rev);
    sim_register_test("test_a_home_notrans", test_a_home_notrans);
    sim_register_test("test_a_home_seek_timeout", test_a_home_seek_timeout);
    sim_register_test("test_a_home_latch", test_a_home_latch);
    sim_register_test("test_a_home_drift", test_a_home_drift);
    sim_register_test("test_a_home_fault", test_a_home_fault);
    sim_register_test("test_a_home_abort", test_a_home_abort);
}
