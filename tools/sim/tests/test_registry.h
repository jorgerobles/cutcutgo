/**
 * Automatic test registry for the host simulator.
 *
 * Test files register themselves via __attribute__((constructor)) calls to
 * sim_register_test(), so adding a suite never requires touching main.c.
 */

#ifndef __INC_SIM_TEST_REGISTRY_H
#define __INC_SIM_TEST_REGISTRY_H

/* Test function prototype (Unity test body). */
typedef void (*sim_test_fn_t)(void);

/* Register a test under `name`. Called from constructors, before main(). */
void sim_register_test(const char *name, sim_test_fn_t fn);

/* Registry accessors for the runner. */
int sim_test_count(void);
const char *sim_test_name(int index);
sim_test_fn_t sim_test_at(int index);

#endif /* __INC_SIM_TEST_REGISTRY_H */
