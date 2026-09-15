/**
 * A-homing firmware binding.
 *
 * Wires the pure a_home state machine (hal/a_home.c) to the real HAL: A pulse
 * drive (manual mode + thermal cooldown), ISL29125 GREEN reflectance via
 * mark_detector_read, A/Z2 encoder steps, the Z2 endstop raised-blade
 * precondition, and sys.abort. Exposes a_home_hal_run()/a_home_hal_report()
 * for the $HA/$AQ system commands.
 */

#ifndef __INC_HAL_A_HOME_HAL_H
#define __INC_HAL_A_HOME_HAL_H

#include <stdint.h>

/* Run A homing; returns a GRBL STATUS_* code (prints the result). */
uint8_t a_home_hal_run(void);

/* Print the last A homing result (steps/degrees). */
void a_home_hal_report(void);

/* Print the raw GREEN reflectance byte (non-motion, for calibration). */
void a_home_hal_refl(void);

#endif /* __INC_HAL_A_HOME_HAL_H */
