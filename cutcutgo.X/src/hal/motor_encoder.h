/**
 * Motor encoder dispatch and quadrature decoding.
 *
 * Pure, hardware-independent part of the motor HAL: the encoder pin lookup
 * table, the quadrature state machine and the change-notification dispatch.
 * The ISR-side wrapper (hal_motor_update_callback) lives in motor.c and only
 * forwards the port registers here, so this file compiles unmodified on the
 * host simulator (compile-the-real-code boundary).
 */

#ifndef __INC_HAL_MOTOR_ENCODER_H
#define __INC_HAL_MOTOR_ENCODER_H

#include <stdint.h>
#include "hal/motor.h"

/* Clear the encoder lookup table and dispatch list. */
void hal_motor_lookup_clear(void);

/* Register a motor encoder pins into the lookup/dispatch structures. */
void hal_motor_lookup_register(hal_motor_driver_t *p_motor);

/* Quadrature decode for one encoder transition (2-bit port nibble). */
void hal_motor_update_encoder_state(hal_motor_driver_t *motor, uint8_t enc_state);

/* Service all registered motor encoders from the port registers. */
void hal_motor_service_encoders(uint32_t cnstatg, uint32_t portg);

#endif /* __INC_HAL_MOTOR_ENCODER_H */
