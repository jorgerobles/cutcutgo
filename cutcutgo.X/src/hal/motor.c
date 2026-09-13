/**
 * Motor driver library
 */

/**
 *
 * ISR à revoir, trop d'appels, instabilité.
 */

#include <string.h>
#include <stdbool.h>
#include "hal/motor.h"
#include "hal/motor_encoder.h"
#include "../grbl/grbl/stepper.h"
#include "definitions.h"
#include "config.h"
#include "../grbl/grbl/grbl.h"

#define M_CW    1
#define M_CCW   -1
#define M_NULL  0
#define M_ERR   2

#define M_DIR_CW    1
#define M_DIR_CCW   2

/* Define X MOTOR driver pins and default state. */
hal_motor_driver_t HAL_MOTOR_X = {
    MOTOR_X_OCM,
    MOTOR_X_DRIVER_IN1,
    MOTOR_X_DRIVER_IN2,
    MOTOR_X_DRIVER_ENC_A,
    MOTOR_X_DRIVER_ENC_B,
    &MOTOR_X_DRIVER_PWM_IN1,
    &MOTOR_X_DRIVER_PWM_IN2,
    0,
    200, /* Hard limit threshold. */
    HAL_MOTOR_PWM,
    HAL_MOTOR_STOP,
    0,
    0,
    HAL_MOTOR_IDLE,
    0,
    0,
    false
};

/* Define Y MOTOR driver pins and default state. */
hal_motor_driver_t HAL_MOTOR_Y = {
    MOTOR_Y_OCM,
    MOTOR_Y_DRIVER_IN1,
    MOTOR_Y_DRIVER_IN2,
    MOTOR_Y_DRIVER_ENC_A,
    MOTOR_Y_DRIVER_ENC_B,
    &MOTOR_Y_DRIVER_PWM_IN1,
    &MOTOR_Y_DRIVER_PWM_IN2,
    0,
    200, /* Hard limit threshold. */
    HAL_MOTOR_PWM,
    HAL_MOTOR_STOP,
    0,
    0,
    HAL_MOTOR_IDLE,
    0,
    0,
    false
};

/* Define Z1 MOTOR driver pins and default state. */
hal_motor_driver_t HAL_MOTOR_Z1 = {
    MOTOR_Z1_OCM,
    MOTOR_Z1_DRIVER_IN1,
    MOTOR_Z1_DRIVER_IN2,
    MOTOR_Z1_DRIVER_ENC_A,
    MOTOR_Z1_DRIVER_ENC_B,
    &MOTOR_Z1_DRIVER_PWM_IN1,
    &MOTOR_Z1_DRIVER_PWM_IN2,
    0,
    200, /* Hard limit threshold. */
    HAL_MOTOR_PWM,
    HAL_MOTOR_STOP,
    0,
    0,
    HAL_MOTOR_IDLE,
    0,
    0,
    true
};

/* Define Z2 MOTOR driver pins and default state. */
hal_motor_driver_t HAL_MOTOR_Z2 = {
    MOTOR_Z2_OCM,
    MOTOR_Z2_DRIVER_IN1,
    MOTOR_Z2_DRIVER_IN2,
    MOTOR_Z2_DRIVER_ENC_A,
    MOTOR_Z2_DRIVER_ENC_B,
    &MOTOR_Z2_DRIVER_PWM_IN1,
    &MOTOR_Z2_DRIVER_PWM_IN2,
    0,
    200, /* Hard limit threshold. */
    HAL_MOTOR_PWM,
    HAL_MOTOR_STOP,
    0,
    0,
    HAL_MOTOR_IDLE,
    0,
    0,
    true
};

/* Define A MOTOR driver pins and default state. */
hal_motor_driver_t HAL_MOTOR_A = {
    MOTOR_A_OCM,
    MOTOR_A_DRIVER_IN1,
    MOTOR_A_DRIVER_IN2,
    MOTOR_A_DRIVER_ENC_A,
    MOTOR_A_DRIVER_ENC_B,
    &MOTOR_A_DRIVER_PWM_IN1,
    &MOTOR_A_DRIVER_PWM_IN2,
    0,
    HAL_MOTOR_PWM,
    HAL_MOTOR_STOP,
    0,
    0,
    HAL_MOTOR_IDLE,
    0,
    0,
    false
};

/* Globals. */
bool gb_pwm_init = false;
bool gb_motor_lookup_init = false;

/**
 * Initialize the motor/encoder lookup table.
 *
 * Registration order defines the encoder dispatch order (X, Y, Z1,
 * Z2, A) â€” keep it stable across calls.
 */

void hal_motor_lookup_init(void)
{
    hal_motor_lookup_clear();

    hal_motor_lookup_register(&HAL_MOTOR_X);
    hal_motor_lookup_register(&HAL_MOTOR_Y);
    hal_motor_lookup_register(&HAL_MOTOR_Z1);
    hal_motor_lookup_register(&HAL_MOTOR_Z2);
    hal_motor_lookup_register(&HAL_MOTOR_A);
}

void hal_motor_enable_encoder(hal_motor_driver_t *motor, bool enabled)
{
    if (enabled)
    {
        /* Enable encA/encB pin change notification. */
        CNENG |= (1 << (motor->encA & 0x0F));
        CNENG |= (1 << (motor->encB & 0x0F));
    }
    else
    {
        /* Disable encA/encB pin change notification. */
        CNENG &= ~(1 << (motor->encA & 0x0F));
        CNENG &= ~(1 << (motor->encB & 0x0F));
    }
}   

/**
 * Motor API
 */

/**
 * Initialize the motor HAL.
 * 
 * This function sets TIMER2 if not already initialized. This timer is used
 * to drive PWM motor GPIOs. Motors are totally controlled by hardware and
 * interrupts (full-async).
 */

int hal_motor_pwm_init(void)
{
    if (!gb_pwm_init)
    {
        /* Set Timer 2.*/
        T2CON   = 0x0;      // Disable timer 2 when setting it up
        TMR2    = 0;        // Set timer 2 counter to 0

        /* Set up the period. Period = PBCLK3 frequency, which is SYS_FREQ / 2, divided by 20000Hz and then divided by 8 for our chosen pre-scaler. */
        PR2 = SYS_FREQ / 2 / 20000;
        
#if 0
        // Enable Timer2 interrupt
        IPC2SET = 0x00000005; /* Priority: 1, Sub-priority: 1 */
        IFS0 &= ~(1 << 9);
        IEC0 |= (1 << 9);
#endif

        // Set up the pre-scaler
        //T2CONbits.TCKPS = 0b101; // Pre-scale of 32
        T2CONbits.TCKPS = 0b000; // Pre-scale of 1

        // Turn on timer 2
        T2CONbits.TON  = 1;

        gb_pwm_init = true;
    }
    
    /* Success. */
    return 0;
}


/**
 * Deinitialize the motor HAL.
 * 
 * This function sets TIMER2 if not already initialized. This timer is used
 * to drive PWM motor GPIOs. Motors are totally controlled by hardware and
 * interrupts (full-async).
 */

int hal_motor_pwm_deinit(void)
{
    if (gb_pwm_init)
    {
        // Turn off timer 2
        T2CONbits.TON  = 0;

        gb_pwm_init = false;
    }
    
    /* Success. */
    return 0;
}

/**
 * Initialize a DC motor driver IN1/IN2 pins.
 * 
 * Sets IN1/IN2 pins as PWM output.
 * 
 * @param motor pointer to a motor structure.
 * @return Always 0.
 */

int hal_motor_pwm_ocm_init(hal_motor_driver_t *motor)
{
    switch (motor->ocm)
    {
        case 1:
        {
            OC1CON = 0;
            OC1CONbits.OCTSEL = 0;
            OC1CONbits.OCM = 0b110;
            OC1RS = 0;
            OC1CONbits.ON = 1;
        }
        break;
        
        case 2:
        {
            OC2CON = 0;
            OC2CONbits.OCTSEL = 0;
            OC2CONbits.OCM = 0b110;
            OC2RS = 0;
            OC2CONbits.ON = 1;            
        }
        break;
        
        case 3:
        {
            OC3CON = 0;
            OC3CONbits.OCTSEL = 0;
            OC3CONbits.OCM = 0b110;
            OC3RS = 0;
            OC3CONbits.ON = 1;            
        }
        break;
        
        case 4:
        {
            OC4CON = 0;
            OC4CONbits.OCTSEL = 0;
            OC4CONbits.OCM = 0b110;
            OC4RS = 0;
            OC4CONbits.ON = 1;            
        }
        break;
        
        case 5:
        {
            OC5CON = 0;
            OC5CONbits.OCTSEL = 0;
            OC5CONbits.OCM = 0b110;
            OC5RS = 0;
            OC5CONbits.ON = 1;            
        }
        break;
    }
    
    /* Success. */
    return 0;
}


/**
 * Sets the direction of a motor.
 * 
 * Drive the DC motor driver corresponding to a given motor to make the motor
 * turn in one direction or another, or simply stops the motor.
 * 
 * @param motor pointer to a motor structure corresponding to the motor to drive.
 * @param direction Direction in which the motor should go.
 * @return  Always 0.
 */

int hal_motor_set_direction(hal_motor_driver_t *motor, hal_motor_direction_t direction)
{
    switch (motor->mode)
    {
        /* Direct drive of DC motor. */
        case HAL_MOTOR_DIRECT:
        {
            switch (direction)
            {
                case HAL_MOTOR_DIR_CW:
                {
                    /* Forward: IN1=1 IN2=0. */
                    GPIO_PinWrite(motor->in1, true);
                    GPIO_PinWrite(motor->in2, false);
                }
                break;

                case HAL_MOTOR_DIR_CCW:
                {
                    /* Forward: IN1=1 IN2=0. */
                    GPIO_PinWrite(motor->in1, false);
                    GPIO_PinWrite(motor->in2, true);            
                }
                break;

                default:
                case HAL_MOTOR_STOP:
                {
                    /* Forward: IN1=1 IN2=0. */
                    GPIO_PinWrite(motor->in1, false);
                    GPIO_PinWrite(motor->in2, false);       
                }
                break;
            }

        }
        break;
        
        case HAL_MOTOR_PWM:
        {
            /* Enable PWM pins based on direction. */
            switch (direction)
            {
                case HAL_MOTOR_DIR_CW:
                {
                    /* Forward slow decay: in1=Vcc, in2=PWM*/
                    *motor->pwm_in2 = 0b1011;
                    *motor->pwm_in1 = 0b0000;
                    GPIO_PinWrite(motor->in1, true);
                    
                }
                break;
                
                case HAL_MOTOR_DIR_CCW:
                {
                    /* Reverse slow decay: in1=PWM, in2=Vcc*/
                    *motor->pwm_in1 = 0b1011;
                    *motor->pwm_in2 = 0b0000;
                    GPIO_PinWrite(motor->in2, true);
                }
                break;
                
                default:
                case HAL_MOTOR_STOP:
                {
                    *motor->pwm_in1 = 0b0000;
                    *motor->pwm_in2 = 0b0000;
                    GPIO_PinWrite(motor->in1, true);
                    GPIO_PinWrite(motor->in2, true);
                }
                break;
            }
        }
        break;
    }    
    /* Save direction. */
    motor->direction = direction;
    
    /* Success. */
    return 0;
}

/**
 * Initializes a motor.
 * 
 * @param motor pointer to a motor structure (hal_motor_driver_t*)
 * @param mode Driving mode (PWM or direct)
 * @return Always 0.
 */

int hal_motor_init(hal_motor_driver_t *motor, hal_motor_mode_t mode)
{
    /* Init timer2 if required. */
    hal_motor_pwm_init();
   
    /* Initialize motor lookup table if required. */
    hal_motor_lookup_init();
  
    /* Save mode. */
    hal_motor_set_mode(motor, mode);

    /* Set default direction. */
    hal_motor_set_direction(motor, HAL_MOTOR_STOP);
    
    /* Set default speed. */
    hal_motor_set_speed(motor, HAL_MOTOR_SPEED_MIN);
    
    /* Set encA/encB pins as input. */
    GPIO_PinInputEnable(motor->encA);
    GPIO_PinInputEnable(motor->encB);
    
    /* Enable pulldown for Change Notification. */
    hal_motor_enable_encoder(motor, true);
    
    /* Enable change notification for port G. */
    if (!CNCONGbits.ON)
    {
        CNCONGbits.ON = 1;
    }
    
    /* Initialize encoder state. */
    motor->enc_cur_state = (PORTG >> (motor->encA & 0x0F)) & 0x03;
    
    /* Initialize motor's relative position. */
    motor->rel_pos = 0;
    motor->rel_pos_th = 0;
    
    /* Manual mode disabled by default. */
    motor->manual = false;
       
    /* Success. */
    return 0;
}


/**
 * Deinitializes a motor.
 * 
 * @param motor pointer to a motor structure (hal_motor_driver_t*)
 * @return Always 0.
 */

int hal_motor_deinit(hal_motor_driver_t *motor)
{   
    /* Set direct mode and stop motor (in1 and in2 set to 0) */
    hal_motor_set_mode(motor, HAL_MOTOR_DIRECT);
    hal_motor_set_direction(motor, HAL_MOTOR_STOP);
    
    /* Enable pulldown for Change Notification. */
    hal_motor_enable_encoder(motor, false);
    
    /* Disable change notification for port G. */
    if (CNCONGbits.ON)
    {
        CNCONGbits.ON = 0;
    }
       
    /* Success. */
    return 0;
}

/**
 * Set motor mode.
 * 
 * @param motor pointer to the motor structure to drive.
 * @param mode PWM or direct
 * @return Always 0
 */

int hal_motor_set_mode(hal_motor_driver_t *motor, hal_motor_mode_t mode)
{
    switch (mode)
    {
        case HAL_MOTOR_DIRECT:
        {
            /* Set direction of motor driver pins. */
            GPIO_PinOutputEnable(motor->in1);
            GPIO_PinOutputEnable(motor->in2);
        }
        break;

        case HAL_MOTOR_PWM:
        {
            /* Set direction of motor driver pins. */
            GPIO_PinOutputEnable(motor->in1);
            GPIO_PinOutputEnable(motor->in2);
            
            /* Connect corresponding OCM to PWM pins. */
            hal_motor_pwm_ocm_init(motor);
        }
        break;

        default:
            break;
    }

    /* Save mode. */
    motor->mode = mode;
    
    /* Success. */
    return 0;
}


/**
 * Set motor speed.
 * 
 * @param motor pointer to a motor structure
 * @param speed speed in number of ticks (depending of TIMER2 configuration)
 * @return Always 0
 */

int hal_motor_set_speed(hal_motor_driver_t *motor, uint32_t speed)
{
    if (motor->mode == HAL_MOTOR_PWM)
    {
        switch (motor->ocm)
        {
            case 1:
            {
                OC1RS = speed;
            }
            break;

            case 2:
            {
                OC2RS = speed;
            }
            break;

            case 3:
            {
                OC3RS = speed;
            }
            break;

            case 4:
            {
                OC4RS = speed;
            }
            break;

            case 5:
            {
                OC5RS = speed;
            }
            break;
        }
    }
    
    /* Save speed. */
    motor->speed = speed;
            
    /* Success. */
    return 0;
}

/**
 * Ask the motor driver to turn a given number of steps. 
 * 
 * The motor driver will start driving the motor to make it
 * turn the given number of steps in the specified direction.
 * It will monitor the motor progress and makes it stop when
 * the number of steps have been reached.
 * 
 * TODO: handle motor hangs (watchdog)
 * 
 * @param motor pointer to a motor structure
 * @param steps number of steps to do
 * @param direction direction (CW or CCW)
 * @return Always 0.
 */

int hal_motor_step(hal_motor_driver_t *motor, int steps, hal_motor_direction_t direction)
{
    //char dbg[256];
    int offset = 0;

#if 0
    snprintf(dbg, 256, "rel. pos: %d\r\n", motor->rel_pos);
    printString(dbg);
    snprintf(dbg, 256, "rel. pos th.: %d\r\n", motor->rel_pos_th);
    printString(dbg);
#endif
        
    /* Save command. */
    //motor->command_steps = steps; 
        
    switch (direction)
    {
        case HAL_MOTOR_DIR_CCW:
        {
            offset = (motor->rel_pos - motor->rel_pos_th);
        }
        break;
        
        case HAL_MOTOR_DIR_CW:
        {      
            offset = motor->rel_pos_th - motor->rel_pos;            
        }
        break;
        
        default:
            break;
    }
    
    /* Do not exceed an extra step. */
    if (offset > 8)
    {
        offset = 8;
    }
    
    motor->command_steps = 8 + offset;
    motor->current_steps = 0;
    motor->error_steps = 0;
    motor->wd_prev_steps = 0;
    motor->wd_armed = false;
    motor->wd_stall_counter = 0;
    motor->state = HAL_MOTOR_DRIVEN;
    motor->rel_pos_th += 8 * direction;
    
    /* Set motor speed to kick speed. */
    //motor->nominal_speed = HAL_MOTOR_SPEED_MIN;
    //hal_motor_set_speed(motor, HAL_MOTOR_SPEED_KICK);
    
    /* Enable the motor's encoders. */
    hal_motor_enable_encoder(motor, true);
    
    /* Start motor in the given direction. */
    hal_motor_set_direction(motor, direction);
    
    //delay_ms(2);
        
    /* Success. */
    return 0;
}


/**
 * Wait until a given motor finished its action.
 * 
 * @param motor pointer to a motor structure.
 */

void hal_motor_wait(hal_motor_driver_t *motor)
{
    while (motor->state != HAL_MOTOR_IDLE)
    {
        SYS_Tasks();
    }
}

/*********************************
 * Interrupt related routines
 * 
 * (Don't mess with that. Please.)
 ********************************/

void hal_motor_update_callback(void)
{
    /* Dispatch all registered motor encoders (pure logic in motor_encoder.c). */
    hal_motor_service_encoders(CNSTATG, PORTG);
}

hal_motor_state_t hal_motor_get_state(hal_motor_driver_t *motor)
{
    return motor->state;
}

int hal_motor_get_current_steps(hal_motor_driver_t *motor)
{
    return motor->current_steps;
}

void hal_motor_stop(hal_motor_driver_t *motor)
{
    /* Stop motor. */
    hal_motor_set_direction(motor, HAL_MOTOR_STOP);
    motor->state = HAL_MOTOR_IDLE;
}

void hal_motor_driver_init(void)
{
    /* Force RG6, RG7, RG8 and RG9 to digital I/O. */
    ANSELGbits.ANSG6 = 0;
    ANSELGbits.ANSG7 = 0;
    ANSELGbits.ANSG8 = 0;
    ANSELGbits.ANSG9 = 0;
    
    /* Enable pull-downs on encoders input. */
    CNPDG = 0xF3CF;
    
    /* Enable encoder for X axis. */
    //hal_motor_enable_encoder(&HAL_MOTOR_X, true);
    HAL_MOTOR_X.grbl_axis = X_AXIS;
    HAL_MOTOR_Y.grbl_axis = Y_AXIS;
    HAL_MOTOR_Z1.grbl_axis = Z_AXIS;
    HAL_MOTOR_Z2.grbl_axis = Z_AXIS;
    
    /* Enable IEC1<18> (Change notification for Port G) */
    IEC1bits.CNGIE = 1;
    IFS1bits.CNGIF = 0;
}

void hal_motor_driver_deinit(void)
{
    /* Stop PWM timer. */
    hal_motor_pwm_deinit();
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

void hal_motor_safety_checks(void)
{
    /* Apply safety checks on all motors. */
    hal_motor_stall_detection(&HAL_MOTOR_X);
    hal_motor_stall_detection(&HAL_MOTOR_Y);
    hal_motor_stall_detection(&HAL_MOTOR_Z1);
    hal_motor_stall_detection(&HAL_MOTOR_Z2);
}

void hal_motor_set_manual(hal_motor_driver_t *motor, bool manual_mode)
{
    motor->manual = manual_mode;
}
