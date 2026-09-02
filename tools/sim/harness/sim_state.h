/**
 * Host-side machine-state harness.
 *
 * Implements the nvm_machine_idle() guard seam declared in nvm.h for the
 * simulator: tests drive machine activity with sim_set_machine_busy() and
 * the NVM commit guard reacts. The firmware build provides its own
 * implementation over sys.state + motor states (Plan 02).
 */

#ifndef __INC_SIM_SIM_STATE_H
#define __INC_SIM_SIM_STATE_H

#include <stdbool.h>

/* Set the simulated machine activity: busy = motors moving / work in progress. */
void sim_set_machine_busy(bool busy);

#endif /* __INC_SIM_SIM_STATE_H */
