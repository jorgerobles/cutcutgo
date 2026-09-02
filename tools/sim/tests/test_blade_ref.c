#include <stdint.h>

#include "unity.h"

#include "grbl/grbl/blade_ref.h"
#include "sensors_sim.h"
#include "test_registry.h"

static int32_t sim_z;
static int32_t plane;
static uint8_t fault_on_move;
static uint8_t abort_at;
static uint8_t moves;

static int32_t fget(void)
{
    return sim_z;
}

static void fmove(int32_t target, int32_t rate)
{
    (void)rate;

    sim_z = target;
    moves++;
    if (fault_on_move && moves >= 2)
        sim_blade_fault(1);
    if (plane != 0)
        sim_blade_set(target <= plane ? 1 : 0);
}

static void fstop(void)
{
}

static uint8_t fabort(void)
{
    return (abort_at && moves >= abort_at) ? 1 : 0;
}

static const blade_ref_motion_t motion = {
    fget, fmove, fstop, fabort
};

static void reset_fixture(void)
{
    sim_z = 0;
    plane = 0;
    fault_on_move = 0;
    abort_at = 0;
    moves = 0;
    sim_blade_set(0);
    sim_blade_fault(0);
    sim_blade_timeout(0);
    sim_blade_unsupported(0);
}

static void test_ref_success(void)
{
    int32_t ref = 1;

    reset_fixture();
    plane = -1000;
    TEST_ASSERT_EQUAL_UINT8(BLADE_REF_OK,
        blade_ref_run(&motion, -1, 2000, 200, 100, 100, 10, 3, &ref));
    TEST_ASSERT_EQUAL_INT32(-1000, ref);
}

static void test_ref_fault_selftest(void)
{
    int32_t ref;

    reset_fixture();
    sim_blade_fault(1);
    TEST_ASSERT_EQUAL_UINT8(BLADE_REF_ERR_FAULT,
        blade_ref_run(&motion, -1, 2000, 200, 100, 100, 10, 3, &ref));
}

static void test_ref_unsupported(void)
{
    int32_t ref;

    reset_fixture();
    sim_blade_unsupported(1);
    TEST_ASSERT_EQUAL_UINT8(BLADE_REF_ERR_UNSUPPORTED,
        blade_ref_run(&motion, -1, 2000, 200, 100, 100, 10, 3, &ref));
}

static void test_ref_fault_midseek(void)
{
    int32_t ref;

    reset_fixture();
    plane = -10000;
    fault_on_move = 1;
    TEST_ASSERT_EQUAL_UINT8(BLADE_REF_ERR_FAULT,
        blade_ref_run(&motion, -1, 2000, 200, 100, 100, 10, 3, &ref));
}

static void test_ref_notrans(void)
{
    int32_t ref;

    reset_fixture();
    plane = -10000;
    TEST_ASSERT_EQUAL_UINT8(BLADE_REF_ERR_NOTRANS,
        blade_ref_run(&motion, -1, 2000, 200, 100, 100, 10, 3, &ref));
}

static void test_ref_abort(void)
{
    int32_t ref;

    reset_fixture();
    plane = -10000;
    abort_at = 2;
    TEST_ASSERT_EQUAL_UINT8(BLADE_REF_ERR_ABORT,
        blade_ref_run(&motion, -1, 2000, 200, 100, 100, 10, 3, &ref));
}

static void test_integrity_consistent(void)
{
    blade_ref_integrity_t st;

    blade_ref_integrity_init(&st, 3);
    TEST_ASSERT_EQUAL_UINT8(0, blade_ref_integrity_update(&st, 0, 0, -1000, 10, -1));
    TEST_ASSERT_EQUAL_UINT8(0, blade_ref_integrity_update(&st, -1000, 1, -1000, 10, -1));
    TEST_ASSERT_EQUAL_UINT8(0, blade_ref_integrity_update(&st, -995, 1, -1000, 10, -1));
}

static void test_integrity_loss(void)
{
    blade_ref_integrity_t st;

    blade_ref_integrity_init(&st, 3);
    TEST_ASSERT_EQUAL_UINT8(0, blade_ref_integrity_update(&st, 0, 1, -1000, 10, -1));
    TEST_ASSERT_EQUAL_UINT8(0, blade_ref_integrity_update(&st, 0, 1, -1000, 10, -1));
    TEST_ASSERT_EQUAL_UINT8(1, blade_ref_integrity_update(&st, 0, 1, -1000, 10, -1));
    TEST_ASSERT_EQUAL_UINT8(0, blade_ref_integrity_update(&st, 0, 1, -1000, 10, -1));
}

static void test_integrity_reset(void)
{
    blade_ref_integrity_t st;

    blade_ref_integrity_init(&st, 3);
    blade_ref_integrity_update(&st, 0, 1, -1000, 10, -1);
    blade_ref_integrity_update(&st, 0, 1, -1000, 10, -1);
    TEST_ASSERT_EQUAL_UINT8(0, blade_ref_integrity_update(&st, 0, 0, -1000, 10, -1));
    TEST_ASSERT_EQUAL_UINT8(0, blade_ref_integrity_update(&st, 0, 1, -1000, 10, -1));
    TEST_ASSERT_EQUAL_UINT8(0, blade_ref_integrity_update(&st, 0, 1, -1000, 10, -1));
}

__attribute__((constructor))
static void register_test_blade_ref(void)
{
    sim_register_test("test_ref_success", test_ref_success);
    sim_register_test("test_ref_fault_selftest", test_ref_fault_selftest);
    sim_register_test("test_ref_unsupported", test_ref_unsupported);
    sim_register_test("test_ref_fault_midseek", test_ref_fault_midseek);
    sim_register_test("test_ref_notrans", test_ref_notrans);
    sim_register_test("test_ref_abort", test_ref_abort);
    sim_register_test("test_integrity_consistent", test_integrity_consistent);
    sim_register_test("test_integrity_loss", test_integrity_loss);
    sim_register_test("test_integrity_reset", test_integrity_reset);
}
