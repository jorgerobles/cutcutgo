#include "quadrature.h"

/*
 * Gray sequence (motor.c state_matrix contract): index in the cycle ->
 * 2-bit nibble. One CW step is +1 index, one CCW step is -1 index.
 * matrix[prev][cur] is M_CW exactly for these +1 transitions and M_CCW
 * for the -1 ones (verified bit-for-bit by test_quadrature_gray_roundtrip
 * against a local copy of the firmware matrix).
 */
static const uint8_t gray_seq[4] = { 0u, 1u, 3u, 2u };

/* Current position in the 4-state Gray cycle. */
static int seq_pos;

/* When armed, the next step moves 2 positions (skips one state), which
 * the firmware matrix decodes as a single M_ERR transition. */
static int pending_skip;

void quad_reset(void)
{
    seq_pos = 0;
    pending_skip = 0;
}

uint8_t quad_step(int dir)
{
    int delta = (dir > 0) - (dir < 0);   /* +1 CW, -1 CCW, 0 still */

    if (pending_skip)
    {
        /* Invalid transition: jump over one state of the sequence. */
        delta *= 2;
        pending_skip = 0;
    }

    seq_pos = (seq_pos + delta) & 3;

    return gray_seq[seq_pos];
}

void quad_inject_error(void)
{
    pending_skip = 1;
}
