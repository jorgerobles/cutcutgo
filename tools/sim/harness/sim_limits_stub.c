/**
 * Host stub for the GRBL limit-state hook (see sim_limits_stub.h).
 */

#include <string.h>

#include "sim_limits_stub.h"

static uint8_t log_axis[SIM_LIMITS_LOG_MAX];
static bool log_triggered[SIM_LIMITS_LOG_MAX];
static int log_count;

void limits_set_state(uint8_t axis, bool triggered)
{
    if (log_count >= SIM_LIMITS_LOG_MAX)
        return;

    log_axis[log_count] = axis;
    log_triggered[log_count] = triggered;
    log_count++;
}

void sim_limits_reset(void)
{
    log_count = 0;
}

int sim_limits_count(void)
{
    return log_count;
}

uint8_t sim_limits_axis(int i)
{
    return (i >= 0 && i < log_count) ? log_axis[i] : 0xFF;
}

bool sim_limits_triggered(int i)
{
    return (i >= 0 && i < log_count) ? log_triggered[i] : false;
}
