/**
 * Host-side machine-state harness (see sim_state.h).
 */

#include "sim_state.h"

static bool sim_busy;

void sim_set_machine_busy(bool busy)
{
    sim_busy = busy;
}

/* nvm.h seam: the machine may only commit flash while idle. */
bool nvm_machine_idle(void)
{
    return !sim_busy;
}
