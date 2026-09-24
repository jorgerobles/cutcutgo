/**
 * A-axis absolute homing state machine (pure logic; see a_home.h).
 *
 * The holder has two slots (N/S) and two polished chamfers (E/W): one WIDE
 * (5.14 mm, ~55.7 deg = ~2685 steps) = index 0 deg, one NARROW (<=4 mm,
 * <=~42.7 deg = ~2055 steps). Both chamfers sit ~3500 steps after a slot, so
 * the slot spacing alone cannot tell them apart: the state machine finds a
 * slot, then measures the WIDTH of the following reflectance peak and keeps
 * only the wide one (the narrow chamfer is skipped).
 */

#include "hal/a_home.h"

/* Reflectance signature thresholds (session-4 + on-machine sweep; tune). */
#define A_HOME_SLOT_REFL        30
#define A_HOME_INDEX_REFL       70

/* Geometry (encoder steps). Re-measured cold on-machine: slot-to-slot
 * (180 deg) ~13800 steps, two independent methods (dip-gap median and
 * autocorrelation P=11 with exact P=22 harmonic). 2 x 13800 = 27600.
 * Replaces the session-4 value (17350), which left the 1.5-rev scan limit
 * covering only ~0.94 real revolutions. */
#define A_HOME_STEPS_PER_REV    27600
#define A_HOME_SCAN_MAX_STEPS   (A_HOME_STEPS_PER_REV + A_HOME_STEPS_PER_REV / 2)

/* Chamfer discrimination: peak width >= this is the wide chamfer. At
 * 27600/360 = 76.7 steps/deg: wide ~55.7 deg (~4270 steps), narrow
 * <=42.7 deg (~3270 steps). Threshold sits between both. */
#define A_HOME_WIDE_MIN_STEPS   3750

/* Pulse timing. 150 ms is the shortest reliable pulse for A (motor must move;
 * session-4 minimum). Finer angular resolution comes from a slower PWM speed
 * (higher OCxRS), not from shorter pulses. Cooldown enforced by the firmware
 * pulse callback (thermal-safety). */
#define A_HOME_PULSE_MS         150

/* Safety. Blade engagement (collar rub) starts ~2900 steps from top; a drift
 * tolerance well below that (500 steps = ~0.63 mm) tolerates the small
 * mechanical coupling/settling of Z2 during A rotation while still catching a
 * real blade-engagement change. */
#define A_HOME_Z2_DZ_TOL        500
#define A_HOME_LATCH_LIMIT      5

enum {
    A_HOME_PHASE_SCAN = 0,    /* find a slot */
    A_HOME_PHASE_SEEK = 1,    /* find the peak start after the slot */
    A_HOME_PHASE_MEASURE = 2  /* measure peak width; keep wide, skip narrow */
};

static int32_t ah_abs(int32_t v)
{
    return v < 0 ? -v : v;
}

static int32_t ga_drift;
static int32_t ga_refl_min;
static int32_t ga_refl_max;

int32_t a_home_drift(void)
{
    return ga_drift;
}

int32_t a_home_refl_min(void)
{
    return ga_refl_min;
}

int32_t a_home_refl_max(void)
{
    return ga_refl_max;
}

a_home_result_t a_home_run(const a_home_ctx_t *ctx, int32_t *home_steps)
{
    int32_t start;
    int32_t peak_start = 0;
    int32_t peak_end = 0;
    int32_t z2_start;
    int32_t prev_a;
    int32_t refl;
    int32_t a;
    uint8_t phase = A_HOME_PHASE_SCAN;
    uint32_t frozen = 0;

    start = ctx->read_a_steps();
    z2_start = ctx->read_z2_steps();
    prev_a = start;
    ga_refl_min = 999;
    ga_refl_max = -1;

    for (;;)
    {
        if (ctx->abort())
            return A_HOME_ERR_ABORT;

        if (ah_abs(ctx->read_z2_steps() - z2_start) > A_HOME_Z2_DZ_TOL)
        {
            ga_drift = ctx->read_z2_steps() - z2_start;
            return A_HOME_ERR_DRIFT;
        }

        refl = ctx->read_refl();
        if (refl < 0)
            return A_HOME_ERR_FAULT;
        if (refl < ga_refl_min)
            ga_refl_min = refl;
        if (refl > ga_refl_max)
            ga_refl_max = refl;

        a = ctx->read_a_steps();

        switch (phase)
        {
        case A_HOME_PHASE_SCAN:
            if (refl < A_HOME_SLOT_REFL)
                phase = A_HOME_PHASE_SEEK;
            else if (ah_abs(a - start) > A_HOME_SCAN_MAX_STEPS)
                return A_HOME_ERR_NOTRANS;
            break;

        case A_HOME_PHASE_SEEK:
            if (refl > A_HOME_INDEX_REFL)
            {
                peak_start = a;
                peak_end = a;
                phase = A_HOME_PHASE_MEASURE;
            }
            else if (ah_abs(a - start) > A_HOME_SCAN_MAX_STEPS)
            {
                return A_HOME_ERR_NOTRANS;
            }
            break;

        case A_HOME_PHASE_MEASURE:
            if (refl > A_HOME_INDEX_REFL)
            {
                peak_end = a;
            }
            else
            {
                int32_t width = peak_end - peak_start;

                if (width >= A_HOME_WIDE_MIN_STEPS)
                {
                    *home_steps = peak_start + width / 2;
                    return A_HOME_OK;
                }
                /* Narrow chamfer: skip it, scan for the next slot + peak. */
                phase = A_HOME_PHASE_SCAN;
            }
            break;
        }

        /* Pulse A forward, then verify the encoder advanced (latch guard). */
        prev_a = ctx->read_a_steps();
        ctx->pulse(1, A_HOME_PULSE_MS);

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
