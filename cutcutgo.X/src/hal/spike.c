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
    SPIKE_STREAM,
    SPIKE_G_SPIN
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
static uint32_t spike_deadline;
static int spike_g_k;
static uint32_t spike_jog_start;
static uint8_t blade_last;
static uint8_t blade_valid;

static void telemetry(void)
{
    int32_t refl = -1;

    if (mark_detector_read(&refl) != SENSOR_OK)
        refl = -1;

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
    printString(" refl=");
    printInteger(refl < 0 ? -1 : (refl >> 8));
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

static int32_t refl_raw(void)
{
    int32_t v = -1;

    if (mark_detector_read(&v) != SENSOR_OK)
        return -1;
    return v >> 8;
}

/* Candidate pins for POWER_TRIGGER (Q7 gate, head illumination rail).
 * All motor IN pins are excluded (they are fully accounted for by the HAL). */
typedef struct { const char *n; GPIO_PIN p; } gp_t;
static const gp_t probes[] = {
    { "rd8", GPIO_PIN_RD8 },   { "rd9", GPIO_PIN_RD9 },
    { "re0", GPIO_PIN_RE0 },   { "re1", GPIO_PIN_RE1 },   { "re2", GPIO_PIN_RE2 },
    { "re3", GPIO_PIN_RE3 },   { "re4", GPIO_PIN_RE4 },   { "re5", GPIO_PIN_RE5 },
    { "re6", GPIO_PIN_RE6 },   { "re7", GPIO_PIN_RE7 },   { "re8", GPIO_PIN_RE8 },
    { "re9", GPIO_PIN_RE9 },   { "rc1", GPIO_PIN_RC1 },   { "rc2", GPIO_PIN_RC2 },
    { "ra4", GPIO_PIN_RA4 },   { "ra5", GPIO_PIN_RA5 },   { "ra6", GPIO_PIN_RA6 },
    { "ra7", GPIO_PIN_RA7 },
};

static void probe_sweep(void)
{
    uint8_t i;
    int32_t hi, lo;

    printString("[P] slow sweep: watch the sensor area (phone cam) - 2s hold per pin\r\n");
    for (i = 0; i < sizeof(probes) / sizeof(probes[0]); i++) {
        GPIO_PinOutputEnable(probes[i].p);
        printString("[P] >>> NOW: ");
        printString(probes[i].n);
        printString(" HIGH 2s <<<\r\n");
        GPIO_PinWrite(probes[i].p, true);
        _delay_ms(2000);
        hi = refl_raw();

        GPIO_PinWrite(probes[i].p, false);
        _delay_ms(400);
        lo = refl_raw();

        GPIO_PinWrite(probes[i].p, true);
        printString("[P] ");
        printString(probes[i].n);
        printString(" high=");
        printInteger(hi);
        printString(" low=");
        printInteger(lo);
        printString("\r\n");
    }
    printString("[P] sweep done\r\n");
}

/* Dump every data channel of the head sensor (0x09..0x0E) — blade IR path
 * may live on a different register than the mark/visible channel 0x0A. */
static void channel_dump(void)
{
    uint8_t r, v;
    int32_t q;

    printString("[C]");
    for (r = 0x09; r <= 0x0E; r++) {
        v = 0;
        sensors_isl_read_reg(r, &v);
        printString(" ");
        printInteger(r);
        printString("=");
        printInteger(v);
    }
    q = refl_raw();
    printString(" q16hi=");
    printInteger(q);
    printString("\r\n");
}

/* CFG2 scan: find hidden LED/IR-enable bits. For each accepted value,
 * dump the max over data channels. */
static void ch_snapshot(uint8_t *best_ch, uint8_t *mx)
{
    uint8_t ch, d;

    *best_ch = 0xFF; *mx = 0;
    for (ch = 0x09; ch <= 0x0E; ch++) {
        d = 0;
        sensors_isl_read_reg(ch, &d);
        if (d > *mx) { *mx = d; *best_ch = ch; }
    }
}

static void cfg_scan(void)
{
    uint16_t v;
    uint8_t rb, best_ch, mx, reg;

    for (reg = 0; reg < 2; reg++) {
        printString("[S] --- scanning ");
        printString(reg ? "CFG1 (restore 0x0D)" : "CFG2 (restore 0x00)");
        printString(" ---\r\n");
        for (v = 0; v <= 255; v++) {
            if (!sensors_isl_write_reg(reg ? 0x01 : 0x02, (uint8_t)v))
                continue;
            if (!sensors_isl_read_reg(reg ? 0x01 : 0x02, &rb) || rb != (uint8_t)v)
                continue;
            _delay_ms(50);
            ch_snapshot(&best_ch, &mx);
            printString("[S] cfg");
            printInteger(reg + 1);
            printString("=");
            printInteger(v);
            printString(" max=");
            printInteger(mx);
            printString(" (ch ");
            printInteger(best_ch);
            printString(")\r\n");
        }
    }
    sensors_isl_write_reg(0x01, 0x0D);
    sensors_isl_write_reg(0x02, 0x00);
    printString("[S] scan done, cfg1=0x0D cfg2=0x00 restored\r\n");
}

/* Read every head-ribbon candidate pin as a digital input. The lateral
 * blade sensor's phototransistor output may sit on an "encoder-labelled"
 * line; diff blade-in vs blade-out to find it. */
static void input_dump(void)
{
    uint8_t i;
    static const gp_t ins[] = {
        { "rg8", GPIO_PIN_RG8 },   { "rg9", GPIO_PIN_RG9 },
        { "rg12", GPIO_PIN_RG12 }, { "rg13", GPIO_PIN_RG13 },
        { "rg14", GPIO_PIN_RG14 }, { "rg15", GPIO_PIN_RG15 },
        { "rd8", GPIO_PIN_RD8 },   { "rd9", GPIO_PIN_RD9 },
        { "rb12", GPIO_PIN_RB12 }, { "rb13", GPIO_PIN_RB13 },
        { "ra4", GPIO_PIN_RA4 },   { "ra5", GPIO_PIN_RA5 },
        { "ra6", GPIO_PIN_RA6 },   { "ra7", GPIO_PIN_RA7 },
    };

    printString("[I]");
    for (i = 0; i < sizeof(ins) / sizeof(ins[0]); i++) {
        GPIO_PinInputEnable(ins[i].p);
        printString(" ");
        printString(ins[i].n);
        printString("=");
        printInteger(GPIO_PinRead(ins[i].p) ? 1 : 0);
    }
    printString("\r\n");
}

/* Full register sweep of the head chip: the blade receiver may sit on a
 * register beyond the known 0x09-0x0E window. */
static void register_dump(void)
{
    uint8_t r, v;

    for (r = 0; r <= 0x3F; r++) {
        v = 0;
        if (!sensors_isl_read_reg(r, &v))
            continue;
        printString("[D] ");
        printInteger(r);
        printString("=");
        printInteger(v);
        printString("\r\n");
    }
    printString("[D] end\r\n");
}

/* Gemini-claims bench: addresses, TCS3472-style ID reg, and side-by-side
 * sampling of TCS "CDATA" (0x14/15) vs ISL GREEN (0x09/0x0A) during A spin. */
static uint16_t reg16(uint8_t lo_reg, uint8_t hi_reg)
{
    uint8_t lo = 0, hi = 0;

    sensors_isl_read_reg(lo_reg, &lo);
    sensors_isl_read_reg(hi_reg, &hi);
    return (uint16_t)((hi << 8) | lo);
}

static void gemini_bench(void)
{
    static const uint8_t addrs[] = { 0x29, 0x39, 0x44, 0x49, 0x4A, 0x50, 0x52, 0x57 };
    uint8_t i, v12 = 0;

    printString("[G] address probe (dec):\r\n");
    for (i = 0; i < sizeof(addrs) / sizeof(addrs[0]); i++) {
        printString("[G] addr ");
        printInteger(addrs[i]);
        printString(" -> ");
        printString(sensors_isl_probe(addrs[i]) ? "ACK\r\n" : "nack\r\n");
    }
    sensors_isl_read_reg(0x12, &v12);
    printString("[G] reg0x12 (TCS3472 ID claim) = ");
    printInteger(v12);
    printString(" (Gemini expects 0x44)\r\n");

    printString("[G] spin sampling: tcs_cdata(0x14/15) vs green(0x09/0A) red blue\r\n");
    hal_motor_set_manual(&HAL_MOTOR_A, true);
    hal_motor_set_speed(&HAL_MOTOR_A, 1800);
    hal_motor_set_direction(&HAL_MOTOR_A, HAL_MOTOR_DIR_CW);
    spike_g_k = 0;
    spike_deadline = timer_get_ms();
}

static void gemini_spin_tick(void)
{
    uint16_t tcs, g, r, b;

    if ((int32_t)(timer_get_ms() - spike_deadline) < 40)
        return;
    spike_deadline = timer_get_ms();
    tcs = reg16(0x14, 0x15);
    g = reg16(0x09, 0x0A);
    r = reg16(0x0B, 0x0C);
    b = reg16(0x0D, 0x0E);
    printString("[G] ");
    printInteger(spike_g_k);
    printString(" tcs=");
    printInteger(tcs);
    printString(" g=");
    printInteger(g);
    printString(" r=");
    printInteger(r);
    printString(" b=");
    printInteger(b);
    printString("\r\n");
    spike_g_k++;
    if (spike_g_k >= 30) {
        hal_motor_set_direction(&HAL_MOTOR_A, HAL_MOTOR_STOP);
        spike_state = SPIKE_STREAM;
        spike_next_ms = timer_get_ms();
        printString("[G] bench done\r\n");
    }
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
    if (n == 1 && !strcmp(w[0], "G")) {
        if (spike_state != SPIKE_STREAM) { printString("[SPIKE] busy\r\n"); return; }
        if (!a_armed) { printString("[SPIKE] REFUSED: 'G' spins A - 'arm' first (blade raised)\r\n"); return; }
        gemini_bench();
        spike_state = SPIKE_G_SPIN;
        return;
    }
    if (n == 1 && !strcmp(w[0], "d")) {
        if (spike_state != SPIKE_STREAM) { printString("[SPIKE] busy\r\n"); return; }
        register_dump();
        return;
    }
    if (n == 1 && !strcmp(w[0], "i")) {
        if (spike_state != SPIKE_STREAM) { printString("[SPIKE] busy\r\n"); return; }
        input_dump();
        return;
    }
    if (n == 1 && !strcmp(w[0], "c")) {
        if (spike_state != SPIKE_STREAM) { printString("[SPIKE] busy\r\n"); return; }
        channel_dump();
        return;
    }
    if (n == 3 && !strcmp(w[0], "w")) {
        long reg = strtol(w[1], 0, 0);
        long val = strtol(w[2], 0, 0);
        if (reg < 0 || reg > 0x14 || val < 0 || val > 255) {
            printString("[SPIKE] ?range\r\n");
            return;
        }
        if (!sensors_isl_write_reg((uint8_t)reg, (uint8_t)val)) {
            printString("[SPIKE] nack\r\n");
            return;
        }
        printString("[SPIKE] wrote\r\n");
        return;
    }
    if (n == 1 && !strcmp(w[0], "s")) {
        if (spike_state != SPIKE_STREAM) { printString("[SPIKE] busy\r\n"); return; }
        cfg_scan();
        return;
    }
    if (n == 1 && !strcmp(w[0], "p")) {
        if (spike_state != SPIKE_STREAM) { printString("[SPIKE] busy\r\n"); return; }
        probe_sweep();
        return;
    }
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
    if (spike_state == SPIKE_G_SPIN) {
        gemini_spin_tick();
        return;
    }

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

    case SPIKE_G_SPIN:
        /* handled above via gemini_spin_tick() */
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
