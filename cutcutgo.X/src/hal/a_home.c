/**
 * A-axis absolute homing state machine (pure logic; see a_home.h).
 */

#include "hal/a_home.h"

/* Reflectance signature thresholds (session-4 calibration; tune on-machine). */
#define A_HOME_SLOT_REFL        15
#define A_HOME_INDEX_REFL       55

/* Geometry (encoder steps). */
#define A_HOME_SLOT_TO_INDEX    3500
#define A_HOME_STEPS_PER_REV    17350
#define A_HOME_SCAN_MAX_STEPS   (A_HOME_STEPS_PER_REV + A_HOME_STEPS_PER_REV / 2)

/* Pulse timing. Kept < the narrow-feature width so the slot/index are not
 * skipped between samples; cooldown is enforced by the firmware pulse callback
 * (thermal-safety, A4950 latch). */
#define A_HOME_PULSE_MS         150
#define A_HOME_FINE_PULSE_MS    100

/* Safety. */
#define A_HOME_Z2_DZ_TOL        150
#define A_HOME_LATCH_LIMIT      5

enum {
    A_HOME_PHASE_SCAN = 0,
    A_HOME_PHASE_SEEK = 1,
    A_HOME_PHASE_PEAK = 2
};

static int32_t ah_abs(int32_t v)
{
    return v < 0 ? -v : v;
}

a_home_result_t a_home_run(const a_home_ctx_t *ctx, int32_t *home_steps)
{
    int32_t start;
    int32_t slot = 0;
    int32_t peak = 0;
    int32_t z2_start;
    int32_t prev_a;
    int32_t refl;
    int32_t a;
    uint8_t phase = A_HOME_PHASE_SCAN;
    uint32_t frozen = 0;

    if (!ctx->blade_raised())
        return A_HOME_ERR_BLADE_DOWN;

    start = ctx->read_a_steps();
    z2_start = ctx->read_z2_steps();
    prev_a = start;

    for (;;)
    {
        if (ctx->abort())
            return A_HOME_ERR_ABORT;

        if (ah_abs(ctx->read_z2_steps() - z2_start) > A_HOME_Z2_DZ_TOL)
            return A_HOME_ERR_DRIFT;

        refl = ctx->read_refl();
        if (refl < 0)
            return A_HOME_ERR_FAULT;

        a = ctx->read_a_steps();

        switch (phase)
        {
        case A_HOME_PHASE_SCAN:
            if (refl < A_HOME_SLOT_REFL)
            {
                slot = a;
                phase = A_HOME_PHASE_SEEK;
            }
            else if (ah_abs(a - start) > A_HOME_SCAN_MAX_STEPS)
            {
                return A_HOME_ERR_NOTRANS;
            }
            break;

        case A_HOME_PHASE_SEEK:
            if ((refl > A_HOME_INDEX_REFL) &&
                (ah_abs(a - slot) >= A_HOME_SLOT_TO_INDEX))
            {
                peak = a;
                phase = A_HOME_PHASE_PEAK;
            }
            break;

        case A_HOME_PHASE_PEAK:
            if (refl > A_HOME_INDEX_REFL)
                peak = a;
            else
            {
                *home_steps = peak;
                return A_HOME_OK;
            }
            break;
        }

        /* Pulse A forward, then verify the encoder advanced (latch guard). */
        prev_a = ctx->read_a_steps();
        ctx->pulse(1, (phase == A_HOME_PHASE_PEAK)
                        ? A_HOME_FINE_PULSE_MS : A_HOME_PULSE_MS);

        if (ctx->read_a_steps() == prev_a)
        {
            frozen++;
            if (frozen >= A_HOME_LATCH_LIMIT)
                return A_HOME_ERR_LATCH;
        }
        else
        {
            frozen = 0;
        }
    }
}
