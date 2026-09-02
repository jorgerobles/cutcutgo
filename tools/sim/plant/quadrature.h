/**
 * Deterministic quadrature encoder plant model.
 *
 * Generates EXACTLY the 2-bit state sequence that the firmware's Gray
 * state_matrix (cutcutgo.X/src/hal/motor.c:694-760) decodes as CW/CCW:
 *
 *   CW : 0 -> 1 -> 3 -> 2 -> 0 ...   (00 -> 01 -> 11 -> 10)
 *   CCW: the exact reverse.
 *
 * The nibble layout matches the raw port read the firmware performs,
 * enc_state = (PORTG >> encA) & 0x03 (bit0 = ENC A pin, bit1 = ENC B pin).
 * The plant ONLY generates the sequence; decoding (and any control policy)
 * lives outside the plant (compile-the-real-code boundary: the tests decode
 * with a local copy of the firmware matrix, the plant never contains one).
 *
 * Fully deterministic: no randomness, no wall clock (T-1-12).
 */

#ifndef __INC_SIM_PLANT_QUADRATURE_H
#define __INC_SIM_PLANT_QUADRATURE_H

#include <stdint.h>

/**
 * @brief   Reset the encoder to the first state of the Gray sequence
 *          (nibble 0) and clear any pending error injection.
 */
void quad_reset(void);

/**
 * @brief   Advance one position along the valid Gray sequence.
 *
 * @param   dir  > 0 for one CW step (firmware M_CW), < 0 for one CCW step
 *               (firmware M_CCW), 0 for no movement (state unchanged).
 *
 * @return  The new 2-bit encoder nibble (the value the firmware would
 *          sample from the port after this edge).
 */
uint8_t quad_step(int dir);

/**
 * @brief   Arm an invalid-sequence injection: the NEXT quad_step skips one
 *          position of the Gray sequence, so the single transition it
 *          produces decodes as M_ERR in the firmware matrix (the firmware
 *          would count one error_step). The nibble itself stays a valid
 *          Gray state, so subsequent single steps decode valid again.
 */
void quad_inject_error(void);

#endif /* __INC_SIM_PLANT_QUADRATURE_H */
