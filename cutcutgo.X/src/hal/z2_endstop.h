/**
 * Z2 endstop cycle (blade Z plunger, CW = up).
 *
 * Raises Z2 to its top stall and verifies it is parked there by measuring the
 * encoder delta over a short jog (stalled = dz <= tol). Used as the
 * raised-blade precondition for A rotation. See docs/a-axis-homing-encoder.md.
 */

#ifndef __INC_HAL_Z2_ENDSTOP_H
#define __INC_HAL_Z2_ENDSTOP_H

#include <stdint.h>

/* Raise Z2 to top stall in bounded chunks; 1 = parked at top, 0 = not parked. */
uint8_t z2_endstop_raise(void);

/* Lower Z2 to bottom stall (blade just above the mat, collar on the sensor
 * piece) in bounded chunks; 1 = parked at bottom, 0 = not parked. */
uint8_t z2_endstop_lower(void);

#endif /* __INC_HAL_Z2_ENDSTOP_H */
