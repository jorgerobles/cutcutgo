#include <stdint.h>

#include "unity.h"

#include "hal/sensors.h"
#include "sensors_sim.h"
#include "test_registry.h"

static void test_mark_present(void)
{
    int32_t v = -1;

    sim_mark_fault(0);
    sim_mark_timeout(0);
    sim_mark_set(0x10000);
    TEST_ASSERT_EQUAL_INT(SENSOR_OK, mark_detector_read(&v));
    TEST_ASSERT_EQUAL_INT32(0x10000, v);
}

static void test_mark_absent(void)
{
    int32_t v = -1;

    sim_mark_set(0);
    TEST_ASSERT_EQUAL_INT(SENSOR_OK, mark_detector_read(&v));
    TEST_ASSERT_EQUAL_INT32(0, v);
}

static void test_mark_timeout(void)
{
    int32_t v = -1;

    sim_mark_timeout(1);
    TEST_ASSERT_EQUAL_INT(SENSOR_TIMEOUT, mark_detector_read(&v));
    sim_mark_timeout(0);
}

static void test_mark_selftest_fault(void)
{
    int32_t v;

    sim_mark_fault(1);
    TEST_ASSERT_EQUAL_INT(SENSOR_FAULT, mark_detector_selftest());
    TEST_ASSERT_EQUAL_INT(SENSOR_FAULT, mark_detector_read(&v));
    sim_mark_fault(0);
    TEST_ASSERT_EQUAL_INT(SENSOR_OK, mark_detector_selftest());
}

static void test_blade_states(void)
{
    uint8_t d = 9;

    sim_blade_fault(0);
    sim_blade_timeout(0);
    sim_blade_unsupported(0);
    sim_blade_set(1);
    TEST_ASSERT_EQUAL_INT(SENSOR_OK, blade_detector_read(&d));
    TEST_ASSERT_EQUAL_UINT8(1, d);
    sim_blade_set(0);
    TEST_ASSERT_EQUAL_INT(SENSOR_OK, blade_detector_read(&d));
    TEST_ASSERT_EQUAL_UINT8(0, d);
}

static void test_blade_timeout(void)
{
    uint8_t d;

    sim_blade_timeout(1);
    TEST_ASSERT_EQUAL_INT(SENSOR_TIMEOUT, blade_detector_read(&d));
    sim_blade_timeout(0);
}

static void test_blade_selftest_fault(void)
{
    sim_blade_fault(1);
    TEST_ASSERT_EQUAL_INT(SENSOR_FAULT, blade_detector_selftest());
    sim_blade_fault(0);
    TEST_ASSERT_EQUAL_INT(SENSOR_OK, blade_detector_selftest());
}

__attribute__((constructor))
static void reg(void)
{
    sim_register_test("test_mark_present", test_mark_present);
    sim_register_test("test_mark_absent", test_mark_absent);
    sim_register_test("test_mark_timeout", test_mark_timeout);
    sim_register_test("test_mark_selftest_fault", test_mark_selftest_fault);
    sim_register_test("test_blade_states", test_blade_states);
    sim_register_test("test_blade_timeout", test_blade_timeout);
    sim_register_test("test_blade_selftest_fault", test_blade_selftest_fault);
}
