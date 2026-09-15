/**
 * Z2 endstop cycle (firmware).
 *
 * Raises the blade Z plunger (CW = up) to the top frame stop in short manual
 * jogs and verifies the stall by encoder delta (dz <= tol). Chunked to avoid
 * sustained PWM against the stop. Constants are initial values to tune
 * on-machine.
 */

#include "hal/z2_endstop.h"
#include "hal/motor.h"
#include "grbl/grbl/nuts_bolts.h"

#define Z2_RAISE_SPEED      1800
#define Z2_RAISE_CHUNK_MS   300
#define Z2_RAISE_ATTEMPTS   20
#define Z2_LOWER_CHUNK_MS   300
#define Z2_LOWER_ATTEMPTS   30
#define Z2_STALL_DZ_TOL     150
#define Z2_STEP_ATTEMPTS    200

static int32_t z2_steps(void)
{
    return HAL_MOTOR_Z2.current_steps;
}

static void z2_jog(int32_t dir, uint32_t ms)
{
    hal_motor_set_manual(&HAL_MOTOR_Z2, true);
    hal_motor_set_speed(&HAL_MOTOR_Z2, Z2_RAISE_SPEED);
    hal_motor_set_direction(&HAL_MOTOR_Z2, dir);
    delay_ms((uint16_t)ms);
    hal_motor_set_direction(&HAL_MOTOR_Z2, HAL_MOTOR_STOP);
    hal_motor_set_manual(&HAL_MOTOR_Z2, false);
}

uint8_t z2_endstop_raise(void)
{
    int i;

    for (i = 0; i < Z2_RAISE_ATTEMPTS; i++)
    {
        int32_t before = z2_steps();
        int32_t dz;

        z2_jog(HAL_MOTOR_DIR_CW, Z2_RAISE_CHUNK_MS);

        dz = z2_steps() - before;
        if (dz < 0)
            dz = -dz;
        if (dz <= Z2_STALL_DZ_TOL)
            return 1;
    }

    return 0;
}

uint8_t z2_endstop_lower(void)
{
    int i;

    for (i = 0; i < Z2_LOWER_ATTEMPTS; i++)
    {
        int32_t before = z2_steps();
        int32_t dz;

        z2_jog(HAL_MOTOR_DIR_CCW, Z2_LOWER_CHUNK_MS);

        dz = z2_steps() - before;
        if (dz < 0)
            dz = -dz;
        if (dz <= Z2_STALL_DZ_TOL)
            return 1;
    }

    return 0;
}

uint8_t z2_raise_steps(int32_t steps)
{
    int32_t start = z2_steps();
    int32_t moved;
    int i;

    hal_motor_set_manual(&HAL_MOTOR_Z2, true);
    hal_motor_set_speed(&HAL_MOTOR_Z2, Z2_RAISE_SPEED);
    hal_motor_set_direction(&HAL_MOTOR_Z2, HAL_MOTOR_DIR_CW);

    for (i = 0; i < Z2_STEP_ATTEMPTS; i++)
    {
        delay_ms(10);
        moved = z2_steps() - start;
        if (moved < 0)
            moved = -moved;
        if (moved >= steps)
            break;
    }

    hal_motor_set_direction(&HAL_MOTOR_Z2, HAL_MOTOR_STOP);
    hal_motor_set_manual(&HAL_MOTOR_Z2, false);

    moved = z2_steps() - start;
    if (moved < 0)
        moved = -moved;
    return (moved >= steps) ? 1 : 0;
}
