/**
 * A-homing firmware binding (see a_home_hal.h).
 */

#include "hal/a_home_hal.h"
#include "hal/a_home.h"
#include "hal/z2_endstop.h"
#include "hal/motor.h"
#include "hal/sensors.h"
#include "grbl/grbl/grbl.h"

#define A_HOME_PULSE_SPEED   2000
#define A_HOME_COOLDOWN_MS   1000

static int32_t ah_last_home;
static uint8_t ah_last_ok;

static int32_t ah_read_refl(void)
{
    int32_t refl;

    if (mark_detector_read(&refl) != SENSOR_OK)
        return -1;
    return refl >> 8;
}

static int32_t ah_read_a(void)
{
    return HAL_MOTOR_A.current_steps;
}

static int32_t ah_read_z2(void)
{
    return HAL_MOTOR_Z2.current_steps;
}

static void ah_pulse(int8_t dir, uint32_t ms)
{
    hal_motor_set_manual(&HAL_MOTOR_A, true);
    hal_motor_set_speed(&HAL_MOTOR_A, A_HOME_PULSE_SPEED);
    hal_motor_set_direction(&HAL_MOTOR_A,
                            dir > 0 ? HAL_MOTOR_DIR_CW : HAL_MOTOR_DIR_CCW);
    delay_ms((uint16_t)ms);
    hal_motor_set_direction(&HAL_MOTOR_A, HAL_MOTOR_STOP);
    hal_motor_set_manual(&HAL_MOTOR_A, false);
    /* Thermal cooldown between pulses (A4950 latch mitigation). */
    delay_ms(A_HOME_COOLDOWN_MS);
}

static uint8_t ah_blade_raised(void)
{
    return z2_endstop_raise();
}

static uint8_t ah_abort(void)
{
    return sys.abort ? 1 : 0;
}

static const a_home_ctx_t ah_ctx = {
    ah_read_refl, ah_read_a, ah_read_z2, ah_pulse, ah_blade_raised, ah_abort
};

uint8_t a_home_hal_run(void)
{
    a_home_result_t r;
    int32_t home = 0;

    if (sys.state != STATE_IDLE)
        return STATUS_IDLE_ERROR;

    r = a_home_run(&ah_ctx, &home);

    ah_last_ok = (r == A_HOME_OK);
    ah_last_home = home;

    switch (r)
    {
    case A_HOME_OK:
        printString("[AHOME:ok steps=");
        printInteger((long)home);
        printString("]\r\n");
        return STATUS_OK;
    case A_HOME_ERR_BLADE_DOWN:
        printString("[AHOME:BLADE_DOWN]\r\n");
        return STATUS_OK;
    case A_HOME_ERR_NOTRANS:
        printString("[AHOME:NOTRANS]\r\n");
        return STATUS_OK;
    case A_HOME_ERR_LATCH:
        printString("[AHOME:LATCH]\r\n");
        return STATUS_OK;
    case A_HOME_ERR_DRIFT:
        printString("[AHOME:DRIFT]\r\n");
        return STATUS_OK;
    case A_HOME_ERR_FAULT:
        printString("[AHOME:FAULT]\r\n");
        return STATUS_OK;
    case A_HOME_ERR_ABORT:
        printString("[AHOME:ABORT]\r\n");
        return STATUS_OK;
    default:
        printString("[AHOME:FAULT]\r\n");
        return STATUS_OK;
    }
}

void a_home_hal_report(void)
{
    printString("[AHOMEQ:ok=");
    printInteger(ah_last_ok);
    printString(" steps=");
    printInteger((long)ah_last_home);
    printString("]\r\n");
}
