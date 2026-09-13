/**
 * Host stub for the GRBL limit-state hook (boundary seam).
 *
 * motor_encoder.c (real firmware code) calls limits_set_state(); limits.c
 * itself is hardware-coupled and not compiled on the host. This stub records
 * every call so tests can assert which axes were touched — the encoder
 * isolation guarantee is exactly that motion on one motor never reports
 * limits for another axis.
 */

#ifndef __INC_SIM_HARNESS_LIMITS_STUB_H
#define __INC_SIM_HARNESS_LIMITS_STUB_H

#include <stdint.h>
#include <stdbool.h>

#define SIM_LIMITS_LOG_MAX 64

void sim_limits_reset(void);
int sim_limits_count(void);
uint8_t sim_limits_axis(int i);
bool sim_limits_triggered(int i);

#endif /* __INC_SIM_HARNESS_LIMITS_STUB_H */
