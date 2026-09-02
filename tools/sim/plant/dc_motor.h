/**
 * Deterministic DC motor plant model (host simulator only — this file does
 * NOT exist in the firmware; the firmware never models the motor).
 *
 * Physics (per 1 ms fixed-step Euler integration):
 *
 *   electrical : V = R*i + ke*omega          (quasi-static, no inductance)
 *   mechanical : J*domega/dt = kt*i - B*omega - Tload
 *   position   : dtheta/dt = omega
 *   encoder    : counts = theta/(2*pi) * counts_per_rev * reduction
 *
 * Steady state (Tload = 0): omega_inf = kt*V / (kt*ke + B*R)  ->  omega_inf
 * < V/ke always, and ~= V/ke when B*R << kt*ke. A load torque strictly
 * reduces omega_inf (verifiable physics, see test_plant.c).
 *
 * All parameters default to values coherent with the machine's documented
 * gearboxes (cutcutgo.X/src/hal/config.h header comment); the electrical
 * set is a generic small brushed motor to be fine-tuned against the real
 * machine in phase 2 (threat register T-1-11, accepted disposition).
 *
 * Floats are legitimate here: the fixed-point constraint applies to the
 * firmware target (no FPU), not to the host plant. Fully deterministic:
 * no randomness, no wall clock (T-1-12).
 */

#ifndef __INC_SIM_PLANT_DC_MOTOR_H
#define __INC_SIM_PLANT_DC_MOTOR_H

#include <stdint.h>

/* Gearbox reduction ratios, from cutcutgo.X/src/hal/config.h:1-24. */
#define DC_REDUCTION_X      (1.0f / 6.0f)       /* 11/66 */
#define DC_REDUCTION_Y      (121.0f / 3780.0f)
#define DC_REDUCTION_TOOL1  (11.0f / 48.0f)
#define DC_REDUCTION_TOOL2  (121.0f / 2016.0f)

/* Fixed plant tick: 1 ms, the same time base the firmware derives from
 * SYS_FREQ 96000000 (cutcutgo.X/src/hal/config.h:32) for its soft-timer. */
#define DC_TICK_S           (1.0e-3f)

typedef struct
{
    float R;                 /* Terminal resistance [ohm]. */
    float ke;                /* Back-EMF constant [V.s/rad]. */
    float kt;                /* Torque constant [N.m/A]. */
    float J;                 /* Rotor + reflected inertia [kg.m^2]. */
    float B;                 /* Viscous friction coefficient [N.m.s/rad]. */
    float counts_per_rev;    /* Encoder counts per motor-shaft revolution. */
    float reduction;         /* Gearbox reduction (one of DC_REDUCTION_*). */
} dc_params_t;

typedef struct
{
    dc_params_t p;           /* Parameters (copied at init). */

    float volts;             /* Applied armature voltage (H-bridge) [V]. */
    float tload;             /* Load torque at motor shaft [N.m]. */

    float i;                 /* Armature current [A]. */
    float omega;             /* Angular velocity, motor shaft [rad/s]. */
    float theta;             /* Angular position, motor shaft [rad]. */

    /* Accumulated encoder counts (firmware current_steps equivalent). */
    int64_t counts;
} dc_motor_t;

/**
 * @brief   Initialize a motor with a copy of the given parameters and a
 *          fully discharged state (zero current/speed/position/counts).
 */
void dc_init(dc_motor_t *m, const dc_params_t *p);

/**
 * @brief   Set the applied armature voltage [V] (signed: direction).
 */
void dc_set_voltage(dc_motor_t *m, float volts);

/**
 * @brief   Set the load torque at the motor shaft [N.m].
 */
void dc_set_load(dc_motor_t *m, float nm);

/**
 * @brief   Advance the model by exactly one 1 ms step (Euler).
 */
void dc_tick(dc_motor_t *m);

/**
 * @brief   Coherent default parameter set (X-axis reduction). Callers
 *          override `reduction` per axis with the DC_REDUCTION_* macros.
 */
dc_params_t dc_params_default(void);

#endif /* __INC_SIM_PLANT_DC_MOTOR_H */
