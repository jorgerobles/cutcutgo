/**
 * Host stub for the motor direction/braking hook (boundary seam).
 *
 * motor_encoder.c (real firmware code) calls hal_motor_set_direction()
 * when a driven motor reaches its commanded steps; the real HAL writes
 * PWM/OCM registers and is not compiled on the host. This stub records
 * every call so tests can assert braking behavior and cross-motor
 * isolation of the braking path.
 */

#ifndef __INC_SIM_HARNESS_MOTOR_STUBS_H
#define __INC_SIM_HARNESS_MOTOR_STUBS_H

#include <stdint.h>
#include <stdbool.h>
#include "hal/motor.h"

#define SIM_MOTOR_LOG_MAX 64

void sim_motor_reset(void);
int sim_motor_dir_calls(void);
hal_motor_driver_t *sim_motor_dir_target(int i);
hal_motor_direction_t sim_motor_dir_value(int i);

#endif /* __INC_SIM_HARNESS_MOTOR_STUBS_H */
