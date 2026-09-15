/**
 * A-axis absolute homing (pure, host-compilable).
 *
 * The blade holder carries an absolute optical encoder read as GREEN
 * reflectance by the ISL29125: base 35-43, two slots (refl<15), a wide
 * polished chamfer = index peak (refl>55) at a fixed ~3500-step spacing after
 * a slot, and a narrow bump (39-47). This module drives A in short PWM pulses
 * and, with the motor stopped, samples the reflectance to recover absolute 0°
 * at the index peak in one revolution. See docs/a-axis-homing-encoder.md.
 *
 * Hardware-independent: every side effect (pulse, read reflectance, read A/Z2
 * encoder steps, abort) is injected through a_home_ctx_t callbacks, so this
 * file compiles unmodified on the host simulator (compile-the-real-code
 * boundary, same as hal/motor_encoder.c).
 */

#ifndef __INC_HAL_A_HOME_H
#define __INC_HAL_A_HOME_H

#include <stdint.h>

typedef enum {
    A_HOME_OK = 0,          /* home found at the index peak */
    A_HOME_ERR_BLADE_DOWN,  /* precondition: blade not verified raised */
    A_HOME_ERR_NOTRANS,     /* no slot+index within the scan window */
    A_HOME_ERR_LATCH,       /* A encoder frozen across pulses (thermal latch) */
    A_HOME_ERR_DRIFT,       /* Z2 moved during rotation */
    A_HOME_ERR_FAULT,       /* reflectance read fault */
    A_HOME_ERR_ABORT        /* operator abort */
} a_home_result_t;

typedef struct {
    /* GREEN reflectance byte (refl, 0-255); <0 on sensor fault. */
    int32_t (*read_refl)(void);
    /* A encoder steps (cumulative). */
    int32_t (*read_a_steps)(void);
    /* Z2 encoder steps (top-stall drift monitor). */
    int32_t (*read_z2_steps)(void);
    /* Energize A in dir (+1 = CW, -1 = CCW) for ms, then stop and settle. */
    void (*pulse)(int8_t dir, uint32_t ms);
    /* Precondition: 1 if blade is verified raised, 0 otherwise. */
    uint8_t (*blade_raised)(void);
    /* 1 if the operator requested an abort. */
    uint8_t (*abort)(void);
} a_home_ctx_t;

/*
 * Run the A homing state machine. On A_HOME_OK, *home_steps is the resolved
 * index-peak position. Blocks until done/abort (call from a system-command
 * handler, not the streaming path).
 */
a_home_result_t a_home_run(const a_home_ctx_t *ctx, int32_t *home_steps);

#endif /* __INC_HAL_A_HOME_H */
