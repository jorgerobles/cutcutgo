/**
 * Automatic test registry (see test_registry.h).
 */

#include "test_registry.h"

#include "unity.h"

#define SIM_TEST_MAX 64

typedef struct {
    const char *name;
    sim_test_fn_t fn;
} sim_test_entry_t;

static sim_test_entry_t registry[SIM_TEST_MAX];
static int registered;

/*
 * Runner-level Unity hooks. UnityDefaultTestRun() references global
 * setUp/tearDown, and all suites link into ONE binary — so the pair is
 * defined exactly once here as no-ops. Each suite does its own fixture
 * setup explicitly at the top of its test bodies (static helpers).
 */
void setUp(void)
{
}

void tearDown(void)
{
}

void sim_register_test(const char *name, sim_test_fn_t fn)
{
    if (registered >= SIM_TEST_MAX)
        return;

    registry[registered].name = name;
    registry[registered].fn = fn;
    registered++;
}

int sim_test_count(void)
{
    return registered;
}

const char *sim_test_name(int index)
{
    if (index < 0 || index >= registered)
        return 0;

    return registry[index].name;
}

sim_test_fn_t sim_test_at(int index)
{
    if (index < 0 || index >= registered)
        return 0;

    return registry[index].fn;
}
