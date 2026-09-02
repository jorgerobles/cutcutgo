#include <string.h>

#include "grbl.h"
#include "blade_ref.h"
#include "hal/sensors.h"

#define BLADE_HOME_DIR             (-1)
#define BLADE_HOME_SEEK_RATE       300
#define BLADE_HOME_LATCH_RATE      60
#define BLADE_HOME_CHUNK_STEPS     50
#define BLADE_HOME_RETRACT_STEPS   200
#define BLADE_HOME_DEBOUNCE        3
#define BLADE_HOME_TOL_STEPS       20
#define BLADE_POLL_PRESCALER       64

static int32_t bh_ref_steps;
static uint8_t bh_ref_valid;
static uint8_t bh_degraded;
static int8_t bh_last_state = -1;
static blade_ref_integrity_t bh_integrity;

static int32_t bh_get_z(void)
{
    return sys_position[Z_AXIS];
}

static void bh_move_z(int32_t target_steps, int32_t rate)
{
    float target[N_AXIS];
    plan_line_data_t plan_data;
    int idx;

    memset(&plan_data, 0, sizeof(plan_data));
    plan_data.feed_rate = (float)rate;
    plan_data.condition = (PL_COND_FLAG_SYSTEM_MOTION | PL_COND_FLAG_NO_FEED_OVERRIDE);
    for (idx = 0; idx < N_AXIS; idx++)
        target[idx] = (float)sys_position[idx] / settings.steps_per_mm[idx];
    target[Z_AXIS] = (float)target_steps / settings.steps_per_mm[Z_AXIS];

    if (plan_buffer_line(target, &plan_data) == PLAN_EMPTY_BLOCK)
        return;
    sys.step_control = STEP_CONTROL_EXECUTE_SYS_MOTION;
    system_clear_exec_state_flag(EXEC_CYCLE_STOP);
    st_prep_buffer();
    st_wake_up();
    protocol_buffer_synchronize();
}

static void bh_stop(void)
{
    st_go_idle();
}

static uint8_t bh_abort(void)
{
    return sys.abort ? 1 : 0;
}

static void print_state(int8_t s)
{
    if (s < 0)
        printString("[BLADE:FAULT]\r\n");
    else
        printString(s ? "[BLADE:1]\r\n" : "[BLADE:0]\r\n");
}

uint8_t blade_home_run(void)
{
    static const blade_ref_motion_t m = {
        bh_get_z, bh_move_z, bh_stop, bh_abort
    };
    int32_t max_travel;
    int32_t ref = 0;
    uint8_t r;

    if (sys.state != STATE_IDLE)
        return STATUS_IDLE_ERROR;

    if (blade_detector_selftest() != SENSOR_OK) {
        bh_degraded = 1;
        printString("[BLADE:UNSUPPORTED]\r\n");
        return STATUS_OK;
    }

    max_travel = (int32_t)(settings.max_travel[Z_AXIS] * settings.steps_per_mm[Z_AXIS]);
    if (max_travel <= 0)
        max_travel = -max_travel;

    sys.state = STATE_HOMING;
    r = blade_ref_run(&m, BLADE_HOME_DIR, max_travel, BLADE_HOME_RETRACT_STEPS,
                      BLADE_HOME_CHUNK_STEPS, BLADE_HOME_SEEK_RATE,
                      BLADE_HOME_LATCH_RATE, BLADE_HOME_DEBOUNCE, &ref);

    if (sys.abort)
        return STATUS_OK;

    switch (r) {
    case BLADE_REF_OK:
        sys_position[Z_AXIS] = ref;
        bh_ref_steps = ref;
        bh_ref_valid = 1;
        blade_ref_integrity_init(&bh_integrity, 20);
        gc_sync_position();
        plan_sync_position();
        printString("[MSG:blade ref set]\r\n");
        sys.state = STATE_IDLE;
        return STATUS_OK;
    case BLADE_REF_ERR_NOTRANS:
        sys.state = STATE_IDLE;
        bh_degraded = 1;
        printString("[BLADE:NOTRANS]\r\n");
        return STATUS_OK;
    case BLADE_REF_ERR_UNSUPPORTED:
        sys.state = STATE_IDLE;
        bh_degraded = 1;
        printString("[BLADE:UNSUPPORTED]\r\n");
        return STATUS_OK;
    case BLADE_REF_ERR_ABORT:
        sys.state = STATE_IDLE;
        printString("[BLADE:ABORT]\r\n");
        return STATUS_OK;
    default:
        bh_degraded = 1;
        st_go_idle();
        sys.state = STATE_IDLE;
        system_set_exec_alarm(EXEC_ALARM_PROBE_FAIL_CONTACT);
        return STATUS_OK;
    }
}

void blade_home_after_homing(void)
{
    if (sys.abort)
        return;
    if (blade_detector_selftest() != SENSOR_OK) {
        bh_degraded = 1;
        printString("[BLADE:DEGRADED]\r\n");
        return;
    }
    blade_home_run();
}

void blade_home_poll(void)
{
    static uint8_t prescaler;
    uint8_t d;

    if (++prescaler < BLADE_POLL_PRESCALER)
        return;
    prescaler = 0;

    if (sys.state & (STATE_HOMING | STATE_CYCLE | STATE_HOLD | STATE_JOG | STATE_CHECK_MODE))
        return;

    if (blade_detector_read(&d) != SENSOR_OK) {
        if (bh_last_state != -1) {
            bh_last_state = -1;
            print_state(-1);
        }
        return;
    }

    if ((int8_t)d != bh_last_state) {
        bh_last_state = (int8_t)d;
        print_state((int8_t)d);
    }

    if (bh_ref_valid) {
        if (blade_ref_integrity_update(&bh_integrity, sys_position[Z_AXIS], d,
                                         bh_ref_steps, BLADE_HOME_TOL_STEPS,
                                         BLADE_HOME_DIR)) {
            bh_ref_valid = 0;
            bh_degraded = 1;
            printString("[BLADE:LOSS]\r\n");
        }
    }
}

void blade_home_report(void)
{
    printString("[BLADEQ:state=");
    printInteger(bh_last_state);
    printString(" ref=");
    printInteger(bh_ref_steps);
    printString(" valid=");
    printInteger(bh_ref_valid);
    printString(" degraded=");
    printInteger(bh_degraded);
    printString("]\r\n");
}
