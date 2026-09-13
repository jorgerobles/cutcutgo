/**
 * Motor encoder dispatch and quadrature decoding (pure, host-compilable).
 *
 * Code moved verbatim from motor.c (lookup table, quadrature state machine)
 * plus a table-driven dispatch that replaces the hardcoded per-motor ISR
 * branches. The dispatch visits motors in registration order; each motor is
 * serviced at most once per call, gated on its encA 2-bit change mask —
 * identical semantics to the previous hardcoded branches.
 */

#include <stddef.h>
#include <stdint.h>
#include "hal/motor_encoder.h"
#include "grbl/grbl/limits.h"

#define M_CW    1
#define M_CCW   -1
#define M_NULL  0
#define M_ERR   2

/* Encoder pin lookup: indexed by port G pin number (encA/encB & 0x0F). */
static hal_motor_driver_t *ga_motor_lookup[16];

/* Ordered dispatch list (registration order, deduplicated). */
#define HAL_MOTOR_LIST_MAX 8
static hal_motor_driver_t *ga_motor_list[HAL_MOTOR_LIST_MAX];
static uint8_t gn_motor_count;

void hal_motor_lookup_clear(void)
{
    int i;

    for (i = 0; i < 16; i++)
        ga_motor_lookup[i] = NULL;

    gn_motor_count = 0;
}

void hal_motor_lookup_register(hal_motor_driver_t *p_motor)
{
    uint8_t i;

    ga_motor_lookup[p_motor->encA & 0x0F] = p_motor;
    ga_motor_lookup[p_motor->encB & 0x0F] = p_motor;

    for (i = 0; i < gn_motor_count; i++)
    {
        if (ga_motor_list[i] == p_motor)
            return;
    }

    if (gn_motor_count < HAL_MOTOR_LIST_MAX)
        ga_motor_list[gn_motor_count++] = p_motor;
}

void hal_motor_update_encoder_state(hal_motor_driver_t *motor, uint8_t enc_state)
{
    int8_t direction;

    int8_t state_matrix[4][4] = {
        {M_NULL, M_CW,   M_CCW,  M_ERR},
        {M_CCW,  M_NULL, M_ERR,  M_CW},
        {M_CW,   M_ERR,  M_NULL, M_CCW},
        {M_ERR,  M_CCW,  M_CW,   M_NULL}
    };

    /* Compare with previous encoder state. */
    direction = state_matrix[motor->enc_cur_state][enc_state];

    switch (direction)
    {
        case M_ERR:
        {
            motor->error_steps++;
            motor->enc_cur_state = enc_state;
        }
        break;

        case M_CW:
        case M_CCW:
        {
            /* Increment number of steps done. */
            if (direction == motor->direction)
            {
                if (motor->inv_encoder)
                    motor->current_steps--;
                else
                    motor->current_steps++;
            }
            else
            {
                if (motor->inv_encoder)
                    motor->current_steps++;
                else
                    motor->current_steps--;
            }

            /* Keep relative position up to date. */
            if (motor->inv_encoder)
                motor->rel_pos += (-direction);
            else
                motor->rel_pos += direction;

            /* Save current encoder state. */
            motor->enc_cur_state = enc_state;

            /* Shall we reset limits for the axis ? */
            if ((motor->current_steps > 0) && !motor->wd_armed)
            {
                /* motor is not blocked, no hard limit hit. */
                limits_set_state(motor->grbl_axis, false);

                /* Hard limit has been set for this move. */
                motor->wd_armed = true;
            }

            /* Shall we brake ? */
            if ((motor->state == HAL_MOTOR_DRIVEN) && (motor->current_steps >= motor->command_steps))
            {
                /* Stop motor. */
                hal_motor_set_direction(motor, HAL_MOTOR_STOP);
                motor->state = HAL_MOTOR_IDLE;

                /* Disarm watchdog. */
                motor->wd_armed = false;

                /* Go to next move (if not manual mode). */
                if (!motor->manual)
                {
                    //st_execute_next_step();
                }
            }
        }
        break;

        case M_NULL:
        {
            /* Save current encoder state. */
            motor->enc_cur_state = enc_state;
        }
        break;
    }
}

void hal_motor_service_encoders(uint32_t cnstatg, uint32_t portg)
{
    uint8_t i;
    uint8_t enc_state;

    for (i = 0; i < gn_motor_count; i++)
    {
        hal_motor_driver_t *motor = ga_motor_list[i];

        if (cnstatg & (0x3 << (motor->encA & 0x0F)))
        {
            /* Read encoder state. */
            enc_state = (portg >> (motor->encA & 0x0F)) & 0x03;

            /* Update encoder state. */
            hal_motor_update_encoder_state(motor, enc_state);
        }
    }
}

/** Called every 1ms. */

void hal_motor_stall_detection(hal_motor_driver_t *motor)
{
    if (motor->state == HAL_MOTOR_DRIVEN)
    {
#if 0
        if (!motor->wd_armed)
        {
            if (motor->current_steps != 0)
            {
                /* First pass, keep track of current steps and arm motor watchdog. */
                motor->wd_prev_steps = motor->current_steps;
                motor->wd_armed = true;

                /* We are not hitting any hard limit. */
                limits_set_state(motor->grbl_axis, false);
            }
        }
        else
#endif
        if ((motor->current_steps == motor->wd_prev_steps) && motor->wd_armed)
        {
            //if (motor->wd_stall_counter < 5)
            //{
            //    motor->wd_stall_counter++;
            //}
            //else
            {
                //printString("motor stalled !\r\n");

                /* Watchdog is armed and motor is stalled. First, cut motor. */
                hal_motor_set_direction(motor, HAL_MOTOR_STOP);
                motor->state = HAL_MOTOR_IDLE;

                motor->wd_armed = false;

                /* Tell GRBL we hit an hard limit. */
                limits_set_state(motor->grbl_axis, true);
            }

        } else {
            motor->wd_prev_steps = motor->current_steps;
            motor->wd_stall_counter = 0;
        }
    }
}
