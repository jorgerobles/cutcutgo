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
#define A_HOME_Z2_SETTLE_MS  1500
#define A_HOME_PULSE_MS      150
#define A_SCAN_MAX_STEPS     (17350 + 17350 / 2)
#define A_SCAN_MAX_PULSES    40

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

static uint8_t ah_abort(void)
{
    return sys.abort ? 1 : 0;
}

static const a_home_ctx_t ah_ctx = {
    ah_read_refl, ah_read_a, ah_read_z2, ah_pulse, ah_abort
};

uint8_t a_home_hal_run(void)
{
    a_home_result_t r;
    int32_t home = 0;

    if (sys.state != STATE_IDLE)
        return STATUS_IDLE_ERROR;

    /* Raise the blade first (safety), then lower to the bottom stall where the
     * reflective chamfer sits in the sensor's light path. */
    if (!z2_endstop_raise()) {
        printString("[AHOME:BLADE_DOWN]\r\n");
        return STATUS_OK;
    }
    if (!z2_endstop_lower()) {
        printString("[AHOME:FAULT]\r\n");
        return STATUS_OK;
    }
    /* Let the return spring settle (~1 mm rebound) before the A homing, so
     * the drift monitor starts from the settled reading position. */
    delay_ms(A_HOME_Z2_SETTLE_MS);

    r = a_home_run(&ah_ctx, &home);

    /* Always raise back to top, regardless of the homing result. */
    z2_endstop_raise();

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
        printString("[AHOME:DRIFT dz=");
        printInteger((long)a_home_drift());
        printString("]\r\n");
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

void a_home_hal_refl(void)
{
    int32_t refl;

    if (mark_detector_read(&refl) != SENSOR_OK)
    {
        printString("[AREFL:FAULT]\r\n");
        return;
    }
    printString("[AREFL:");
    printInteger((long)(refl >> 8));
    printString("]\r\n");
}

void a_home_hal_scan(void)
{
    int32_t refl;
    int32_t min = 999;
    int32_t max = -1;
    int32_t a_start = HAL_MOTOR_A.current_steps;
    int32_t d;
    int samples = 0;
    int i;

    for (i = 0; i < A_SCAN_MAX_PULSES; i++)
    {
        refl = ah_read_refl();
        if (refl < 0)
        {
            printString("[ASCAN:FAULT]\r\n");
            return;
        }
        if (refl < min)
            min = refl;
        if (refl > max)
            max = refl;
        samples++;

        ah_pulse(1, A_HOME_PULSE_MS);

        d = HAL_MOTOR_A.current_steps - a_start;
        if (d < 0)
            d = -d;
        if (d > A_SCAN_MAX_STEPS)
            break;
    }

    printString("[ASCAN:min=");
    printInteger((long)min);
    printString(" max=");
    printInteger((long)max);
    printString(" span=");
    printInteger((long)(max - min));
    printString(" samples=");
    printInteger((long)samples);
    printString("]\r\n");
}
