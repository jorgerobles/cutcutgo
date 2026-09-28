/**
 * A-homing firmware binding (see a_home_hal.h).
 */

#include "hal/a_home_hal.h"
#include "hal/a_home.h"
#include "hal/z2_endstop.h"
#include "hal/motor.h"
#include "hal/sensors.h"
#include "hal/timer.h"
#include "grbl/grbl/grbl.h"

#define A_HOME_PULSE_SPEED   2200
#define A_HOME_COOLDOWN_MS   1000
#define A_HOME_Z2_SETTLE_MS  1500
/* 1 mm = 800 steps: the verified no-rub clearance (session-4 + on-machine
 * 2026-09-28: at 400/0.5 mm the blade scratched the frame and the A encoder
 * froze across pulses -> A_HOME_ERR_LATCH on the first $HA attempt). */
#define A_HOME_Z2_RETRACT    800
#define A_HOME_PULSE_MS      150
#define A_SCAN_MAX_STEPS     (27600 + 27600 / 2)
#define A_SCAN_MAX_PULSES    40

/* Diagnostic capture: port of the spike 'cap' command (the version validated
 * on hardware). Spins A continuously while sampling encoder steps + GREEN
 * reflectance into a ring buffer in a tight loop (no serial/USB work during
 * sampling — the I2C read is the only pacing), then dumps CSV. No Z2
 * precondition: the operator positions Z2 via $ZL/$ZR/$ZU first. The A
 * encoder has no Z index, so the host counts steps between the two slots
 * (180 deg apart) to derive steps/rev. */
#define AT_PULSE_SPEED   2000
#define AT_DURATION_MS   20000
#define AT_BUF_SIZE      2000

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
    /* Let the return spring rebound and settle FIRST, then retract ~1 mm
     * so the blade clears the frame while the chamfer stays in view. */
    delay_ms(A_HOME_Z2_SETTLE_MS);
    z2_raise_steps(A_HOME_Z2_RETRACT);

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
        printString("[AHOME:NOTRANS min=");
        printInteger((long)a_home_refl_min());
        printString(" max=");
        printInteger((long)a_home_refl_max());
        printString("]\r\n");
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

void a_home_hal_trace(void)
{
    typedef struct { int32_t a; int32_t z2; int16_t r; } at_sample_t;
    static at_sample_t at_buf[AT_BUF_SIZE];
    uint32_t head = 0, count = 0;
    uint32_t deadline = timer_get_ms() + AT_DURATION_MS;
    int32_t refl;
    uint32_t n, start, i;

    hal_motor_init(&HAL_MOTOR_A, HAL_MOTOR_PWM);
    hal_motor_set_manual(&HAL_MOTOR_A, true);
    hal_motor_set_speed(&HAL_MOTOR_A, AT_PULSE_SPEED);
    hal_motor_set_direction(&HAL_MOTOR_A, HAL_MOTOR_DIR_CW);

    /* Tight loop — no serial/USB work during sampling; the ISL29125 I2C read
     * is the only pacing (matches the spike 'cap' capture). */
    while ((int32_t)(timer_get_ms() - deadline) < 0) {
        refl = -1;
        mark_detector_read(&refl);
        at_buf[head].a = HAL_MOTOR_A.current_steps;
        at_buf[head].z2 = HAL_MOTOR_Z2.current_steps;
        at_buf[head].r = (int16_t)((refl < 0) ? -1 : (refl >> 8));
        head = (head + 1) % AT_BUF_SIZE;
        count++;
    }

    hal_motor_set_direction(&HAL_MOTOR_A, HAL_MOTOR_STOP);
    hal_motor_set_manual(&HAL_MOTOR_A, false);

    n = count < AT_BUF_SIZE ? count : AT_BUF_SIZE;
    start = (count >= AT_BUF_SIZE) ? head : 0;
    printString("[AT] total=");
    printInteger((long)count);
    printString(" dump=");
    printInteger((long)n);
    printString("\r\n");
    for (i = 0; i < n; i++) {
        uint32_t idx = (start + i) % AT_BUF_SIZE;

        printInteger((long)at_buf[idx].a);
        printString(",");
        printInteger((long)at_buf[idx].z2);
        printString(",");
        printInteger((long)at_buf[idx].r);
        printString("\r\n");
    }
    printString("[ATDONE]\r\n");
}
