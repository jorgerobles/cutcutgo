/**
 * Hardware Abstraction Layer configuration
 *
 * Axis map: X, Y, Z1 (marker Z), Z2 (blade Z), A (blade-rotation wormgear
 * mounted on top of the Z2 head). Z1/Z2 are two independent Z-type axes;
 * the active tool selects which one receives commanded Z motion.
 *
 * Z1 (marker Z, tool #1):
 *
 *  - Reduction ratio: 11/48
 *  - displacement/step: ((11/48) * 5.5mm) / STEPS_PER_REV = 0.007 mm
 *
 * Z2 (blade Z, tool #2):
 *
 *  - Reduction ratio: (11 * 11) / (42 * 48) = 121/2016
 *  - displacement/step: ((11/48 * 11/42) * 5.5mm) / STEPS_PER_REV = 0.0017 mm
 *
 * A (blade rotation, wormgear on Z2 head): see docs/spike-a-axis-blade-detection.md
 * — pin assignment vs Z2 pending on-hardware verification.
 *
 * X axis:
 *
 *  - Reduction ratio: 11/66 = 1/6
 *  - displacement/step: (PI * 6mm) /(6 * STEPS_PER_REV) = 0.01mm
 *
 * Y axis:
 *
 *  - Reduction ratio: 121/3780
 *  - displacement/step: (PI * 15.5mm * 121) / (3780 * STEPS_PER_REV) = 0.008 mm
 *
 **/

#ifndef __INC_HAL_CONFIG_H
#define __INC_HAL_CONFIG_H

#include <stdint.h>
#include "config/cutcutgo/peripheral/gpio/plib_gpio.h"

#define SYS_FREQ                 96000000

#define MOTOR_X_DRIVER_IN1       GPIO_PIN_RD0
#define MOTOR_X_DRIVER_PWM_IN2   RPD0R
#define MOTOR_X_DRIVER_IN2       GPIO_PIN_RD1
#define MOTOR_X_DRIVER_PWM_IN1   RPD1R
#define MOTOR_X_OCM              2
#define MOTOR_X_DRIVER_ENC_A     GPIO_PIN_RG0
#define MOTOR_X_DRIVER_ENC_B     GPIO_PIN_RG1

#define MOTOR_Y_DRIVER_IN1       GPIO_PIN_RD2
#define MOTOR_Y_DRIVER_PWM_IN1   RPD2R
#define MOTOR_Y_DRIVER_IN2       GPIO_PIN_RF1
#define MOTOR_Y_DRIVER_PWM_IN2   RPF1R
#define MOTOR_Y_DRIVER_ENC_A     GPIO_PIN_RG6
#define MOTOR_Y_DRIVER_ENC_B     GPIO_PIN_RG7
#define MOTOR_Y_OCM              3

#define MOTOR_Z2_DRIVER_IN1       GPIO_PIN_RB6
#define MOTOR_Z2_DRIVER_PWM_IN1   RPB6R
#define MOTOR_Z2_DRIVER_IN2       GPIO_PIN_RD5
#define MOTOR_Z2_DRIVER_PWM_IN2   RPD5R
#define MOTOR_Z2_DRIVER_ENC_A     GPIO_PIN_RG14
#define MOTOR_Z2_DRIVER_ENC_B     GPIO_PIN_RG15
#define MOTOR_Z2_OCM      2


#define MOTOR_Z1_DRIVER_IN1       GPIO_PIN_RF12
#define MOTOR_Z1_DRIVER_PWM_IN1   RPF12R
#define MOTOR_Z1_DRIVER_IN2       GPIO_PIN_RF8
#define MOTOR_Z1_DRIVER_PWM_IN2   RPF8R
#define MOTOR_Z1_DRIVER_ENC_A     GPIO_PIN_RG12
#define MOTOR_Z1_DRIVER_ENC_B     GPIO_PIN_RG13
#define MOTOR_Z1_OCM      5

#define MOTOR_A_DRIVER_IN1       GPIO_PIN_RD3
#define MOTOR_A_DRIVER_PWM_IN1   RPD3R
#define MOTOR_A_DRIVER_IN2       GPIO_PIN_RD11
#define MOTOR_A_DRIVER_PWM_IN2   RPD11R
#define MOTOR_A_DRIVER_ENC_A     GPIO_PIN_RG8
#define MOTOR_A_DRIVER_ENC_B     GPIO_PIN_RG9
#define MOTOR_A_OCM      4


#endif /* __INC_HAL_CONFIG_H */