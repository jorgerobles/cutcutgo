/**
 * Host simulator test runner.
 *
 * Iterates the automatic test registry (populated by constructors in each
 * test file): adding a suite never requires touching this file.
 */

#include "unity.h"
#include "test_registry.h"

int main(void)
{
    int i;

    UNITY_BEGIN();

    for (i = 0; i < sim_test_count(); i++)
        UnityDefaultTestRun(sim_test_at(i), sim_test_name(i), 0);

    return UNITY_END();
}
