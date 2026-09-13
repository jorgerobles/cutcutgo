/**
 * Bare-metal motor spike (Z2/A pin verification + A motion vs blade detection).
 *
 * No GRBL: no warmup, no homing, no boot motion sequence — safe to run with
 * the tool inserted. Fully non-blocking state machine (BENCH-style): nothing
 * in APP_Initialize may wait on timers (interrupts are not ticking there).
 *
 * Sequence: boot → 3s settle → supervised jog sequence (speed ramping down —
 * the boot warmup scan showed values near HAL_MOTOR_SPEED_MIN do NOT move a
 * loaded axis) → continuous telemetry (encoder steps + blade state @10 Hz).
 */

#include <stdlib.h>
#include <string.h>

#include "definitions.h"
#include "hal/motor.h"
#include "hal/motor_encoder.h"
#include "hal/sensors.h"
#include "hal/timer.h"
#include "grbl/grbl/print.h"
#include "grbl/grbl/serial.h"
#include "hal/spike.h"

typedef struct {
    hal_motor_driver_t *motor;
    const char *name;
    uint8_t ccw;
    uint32_t ms;
    uint32_t speed;
} spike_jog_t;

typedef enum {
    SPIKE_BOOT,
    SPIKE_JOG,
    SPIKE_STREAM
} spike_state_t;

static spike_state_t spike_state = SPIKE_BOOT;
static hal_motor_driver_t *const spike_motors[5] = {
    &HAL_MOTOR_X, &HAL_MOTOR_Y, &HAL_MOTOR_Z1, &HAL_MOTOR_Z2, &HAL_MOTOR_A
};
static const spike_jog_t *spike_cur;
static spike_jog_t manual_jog;
static char rx_line[32];
static uint8_t rx_len;
static uint8_t a_armed;
static uint32_t spike_next_ms;
static uint32_t spike_jog_start;
static uint8_t blade_last;
static uint8_t blade_valid;

static void telemetry(void)
{
    printString("[T] x=");
    printInteger(HAL_MOTOR_X.current_steps);
    printString(" y=");
    printInteger(HAL_MOTOR_Y.current_steps);
    printString(" z1=");
    printInteger(HAL_MOTOR_Z1.current_steps);
    printString(" z2=");
    printInteger(HAL_MOTOR_Z2.current_steps);
    printString(" a=");
    printInteger(HAL_MOTOR_A.current_steps);
    printString(" blade=");
    printInteger(blade_valid ? blade_last : -1);
    printString("\r\n");
}

static void blade_poll(void)
{
    uint8_t d;

    if (blade_detector_read(&d) != SENSOR_OK) {
        if (blade_valid && blade_last != 0xFF) {
            blade_valid = 0;
            printString("[BLADE:FAULT]\r\n");
        }
        return;
    }

    blade_valid = 1;
    if (d != blade_last) {
        blade_last = d;
        printString("[BLADE:");
        printInteger(d);
        printString("]\r\n");
    }
}

static void jog_start(const spike_jog_t *j)
{
    printString("[JOG] ");
    printString(j->name);
    printString(j->ccw ? " CCW " : " CW ");
    printInteger(j->ms);
    printString("ms speed=");
    printInteger(j->speed);
    printString("\r\n");

    hal_motor_init(j->motor, HAL_MOTOR_PWM);
    hal_motor_set_manual(j->motor, true);
    hal_motor_set_speed(j->motor, j->speed);
    hal_motor_set_direction(j->motor,
        j->ccw ? HAL_MOTOR_DIR_CCW : HAL_MOTOR_DIR_CW);
}

static void jog_stop(const spike_jog_t *j)
{
    hal_motor_set_direction(j->motor, HAL_MOTOR_STOP);
    printString("[JOG] done ");
    printString(j->name);
    printString(j->ccw ? " CCW" : " CW");
    printString(" steps=");
    printInteger(j->motor->current_steps);
    printString(" err=");
    printInteger(j->motor->error_steps);
    printString("\r\n");
}

void spike_init(void)
{
    /* Absolute minimum here: SYS_Tasks must start for USB and timer ticks.
     * Per-motor/sensor bring-up happens in the task state machine. */
    hal_motor_driver_init();
    printString("[SPIKE] boot\r\n");
}

static void console_exec(char *line)
{
    char *w[4];
    int n = 0;
    hal_motor_driver_t *m = 0;
    const char *nm = "";
    char *tok = strtok(line, " ");
    long ms, speed;
    uint8_t ccw;

    while (tok && n < 4) { w[n++] = tok; tok = strtok(NULL, " "); }
    if (n == 0)
        return;

    if (n == 1 && !strcmp(w[0], "t")) { telemetry(); return; }
    if (n == 1 && !strcmp(w[0], "arm")) {
        a_armed = 1;
        printString("[SPIKE] A ARMED (operator confirms blade raised)\r\n");
        return;
    }
    if (n == 1 && !strcmp(w[0], "disarm")) {
        a_armed = 0;
        printString("[SPIKE] A disarmed\r\n");
        return;
    }

    if (n < 3) { printString("[SPIKE] usage: <x|y|z1|z2|a> <cw|ccw> <ms> [speed] | t | arm\r\n"); return; }

    if      (!strcmp(w[0], "x"))  { m = &HAL_MOTOR_X;  nm = "X";  }
    else if (!strcmp(w[0], "y"))  { m = &HAL_MOTOR_Y;  nm = "Y";  }
    else if (!strcmp(w[0], "z1")) { m = &HAL_MOTOR_Z1; nm = "Z1"; }
    else if (!strcmp(w[0], "z2")) { m = &HAL_MOTOR_Z2; nm = "Z2"; }
    else if (!strcmp(w[0], "a"))  { m = &HAL_MOTOR_A;  nm = "A";  }
    else { printString("[SPIKE] ?motor\r\n"); return; }

    if (m == &HAL_MOTOR_A && !a_armed) {
        printString("[SPIKE] REFUSED: A requires 'arm' first (blade must be raised)\r\n");
        return;
    }

    if (strcmp(w[1], "cw") && strcmp(w[1], "ccw")) {
        printString("[SPIKE] ?dir\r\n");
        return;
    }
    ccw = !strcmp(w[1], "ccw");

    ms = strtol(w[2], 0, 10);
    speed = (n >= 4) ? strtol(w[3], 0, 10) : 2000;
    if (ms < 1) ms = 1;
    if (ms > 2000) ms = 2000;
    if (speed < 800) speed = 800;
    if (speed > 2300) speed = 2300;

    if (spike_state != SPIKE_STREAM) { printString("[SPIKE] busy\r\n"); return; }

    manual_jog.motor = m;
    manual_jog.name = nm;
    manual_jog.ccw = ccw;
    manual_jog.ms = (uint32_t)ms;
    manual_jog.speed = (uint32_t)speed;
    spike_cur = &manual_jog;
    spike_state = SPIKE_JOG;
    jog_start(spike_cur);
    spike_jog_start = timer_get_ms();
}

static void console_poll(void)
{
    uint8_t c;

    while ((c = serial_read()) != SERIAL_NO_DATA) {
        if (c == '\r' || c == '\n') {
            if (rx_len) {
                rx_line[rx_len] = 0;
                console_exec(rx_line);
                rx_len = 0;
            }
        } else if (c >= 32 && c < 127 && rx_len < sizeof(rx_line) - 1) {
            rx_line[rx_len++] = (char)c;
        }
    }
}

void spike_task(void)
{
    uint32_t now = timer_get_ms();

    console_poll();

    switch (spike_state) {
    case SPIKE_BOOT: {
        uint8_t i;

        HAL_MOTOR_X.grbl_axis = 0;
        HAL_MOTOR_Y.grbl_axis = 1;
        HAL_MOTOR_Z1.grbl_axis = 2;
        HAL_MOTOR_Z2.grbl_axis = 2;
        HAL_MOTOR_A.grbl_axis = 2;

        for (i = 0; i < 5; i++) {
            hal_motor_init(spike_motors[i], HAL_MOTOR_PWM);
        }

        printString("[SPIKE] blade detector ");
        printString((blade_detector_selftest() != SENSOR_OK) ?
                    "UNSUPPORTED\r\n" : "OK\r\n");

        blade_valid = 0;
        blade_last = 0xFF;
        spike_next_ms = now;
        spike_state = SPIKE_STREAM;
        printString("[SPIKE] console ready - NO auto motion. cmds: <x|y|z1|z2|a> <cw|ccw> <ms> [speed] | t | arm | disarm\r\n");
    }
    break;

    case SPIKE_JOG:
        blade_poll();
        if ((int32_t)(now - (spike_jog_start + spike_cur->ms)) >= 0) {
            jog_stop(spike_cur);
            spike_next_ms = now;
            spike_state = SPIKE_STREAM;
        }
        break;

    case SPIKE_STREAM:
        if ((uint32_t)(now - spike_next_ms) >= 100) {
            spike_next_ms = now;
            blade_poll();
            telemetry();
        }
        break;
    }
}
