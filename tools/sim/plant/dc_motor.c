#include "dc_motor.h"

#define DC_TWO_PI   6.2831853f

void dc_init(dc_motor_t *m, const dc_params_t *p)
{
    m->p = *p;
    m->volts = 0.0f;
    m->tload = 0.0f;
    m->i = 0.0f;
    m->omega = 0.0f;
    m->theta = 0.0f;
    m->counts = 0;
}

void dc_set_voltage(dc_motor_t *m, float volts)
{
    m->volts = volts;
}

void dc_set_load(dc_motor_t *m, float nm)
{
    m->tload = nm;
}

void dc_tick(dc_motor_t *m)
{
    /* Electrical equation (quasi-static): V = R*i + ke*omega. */
    m->i = (m->volts - m->p.ke * m->omega) / m->p.R;

    /* Mechanical dynamics: J*domega/dt = kt*i - B*omega - Tload. */
    m->omega += DC_TICK_S * (m->p.kt * m->i - m->p.B * m->omega - m->tload)
                / m->p.J;

    /* Position integration. */
    m->theta += DC_TICK_S * m->omega;

    /* Expose accumulated encoder counts (output-shaft scaled). */
    m->counts = (int64_t)(m->theta * (m->p.counts_per_rev * m->p.reduction)
                          / DC_TWO_PI);
}

dc_params_t dc_params_default(void)
{
    dc_params_t p;

    /* Generic small brushed DC motor: omega_inf = kt*V/(kt*ke + B*R) sits
     * within 0.2 % of V/ke for these numbers (B*R << kt*ke), so the
     * plan's V/ke steady-state criterion holds with wide margin. */
    p.R = 4.0f;
    p.ke = 0.05f;
    p.kt = 0.05f;
    p.J = 1.0e-5f;
    p.B = 1.0e-6f;

    /* Encoder scaled like the machine's 4-state Gray encoders per motor
     * rev times a documented sector factor; exact value tuned against the
     * machine in phase 2 (T-1-11). */
    p.counts_per_rev = 12.0f;

    /* Machine's X-axis gearbox by default (config.h: 11/66 = 1/6). */
    p.reduction = DC_REDUCTION_X;

    return p;
}
