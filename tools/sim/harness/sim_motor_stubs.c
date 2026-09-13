/**
 * Host stub for the motor direction/braking hook (see sim_motor_stubs.h).
 */

#include <string.h>

#include "sim_motor_stubs.h"

static hal_motor_driver_t *log_target[SIM_MOTOR_LOG_MAX];
static hal_motor_direction_t log_dir[SIM_MOTOR_LOG_MAX];
static int log_count;

int hal_motor_set_direction(hal_motor_driver_t *motor, hal_motor_direction_t direction)
{
    if (log_count < SIM_MOTOR_LOG_MAX)
    {
        log_target[log_count] = motor;
        log_dir[log_count] = direction;
        log_count++;
    }

    return 0;
}

void sim_motor_reset(void)
{
    log_count = 0;
}

int sim_motor_dir_calls(void)
{
    return log_count;
}

hal_motor_driver_t *sim_motor_dir_target(int i)
{
    return (i >= 0 && i < log_count) ? log_target[i] : (hal_motor_driver_t *)0;
}

hal_motor_direction_t sim_motor_dir_value(int i)
{
    return (i >= 0 && i < log_count) ? log_dir[i] : HAL_MOTOR_STOP;
}
