#include "blade_ref.h"
#include "hal/sensors.h"

static uint8_t seek_transition(const blade_ref_motion_t *m,
                               int32_t start,
                               int32_t dir,
                               int32_t max_travel_steps,
                               int32_t chunk_steps,
                               int32_t rate,
                               uint8_t debounce,
                               int32_t *where,
                               uint8_t *fault)
{
    int32_t target = start;
    int32_t limit = start + dir * max_travel_steps;
    uint8_t d;

    *fault = 0;
    while ((dir > 0 && target < limit) || (dir < 0 && target > limit)) {
        uint8_t i;
        uint8_t all;

        target += dir * chunk_steps;
        m->move_z(target, rate);
        if (m->abort_requested()) {
            m->stop();
            *fault = 0;
            return 2;
        }
        all = 1;
        for (i = 0; i < debounce; i++) {
            if (blade_detector_read(&d) != SENSOR_OK) {
                m->stop();
                *fault = 1;
                return 0;
            }
            if (!d)
                all = 0;
        }
        if (all) {
            *where = m->get_z_steps();
            return 1;
        }
    }
    return 0;
}

uint8_t blade_ref_run(const blade_ref_motion_t *m,
                      int32_t dir,
                      int32_t max_travel_steps,
                      int32_t retract_steps,
                      int32_t chunk_steps,
                      int32_t seek_rate,
                      int32_t latch_rate,
                      uint8_t debounce,
                      int32_t *ref_steps)
{
    int32_t start;
    int32_t seek_pos = 0;
    int32_t latch_pos = 0;
    uint8_t fault;
    uint8_t r;

    if (debounce == 0)
        debounce = 3;

    switch (blade_detector_selftest()) {
    case SENSOR_OK:
        break;
    case SENSOR_UNSUPPORTED:
        return BLADE_REF_ERR_UNSUPPORTED;
    default:
        return BLADE_REF_ERR_FAULT;
    }

    start = m->get_z_steps();

    r = seek_transition(m, start, dir, max_travel_steps, chunk_steps,
                        seek_rate, debounce, &seek_pos, &fault);
    if (r == 2)
        return BLADE_REF_ERR_ABORT;
    if (fault)
        return BLADE_REF_ERR_FAULT;
    if (r == 0)
        return BLADE_REF_ERR_NOTRANS;

    m->move_z(seek_pos - dir * retract_steps, latch_rate);
    if (m->abort_requested()) {
        m->stop();
        return BLADE_REF_ERR_ABORT;
    }

    r = seek_transition(m, seek_pos - dir * retract_steps, dir,
                        retract_steps + chunk_steps, 1, latch_rate,
                        debounce, &latch_pos, &fault);
    if (r == 2)
        return BLADE_REF_ERR_ABORT;
    if (fault)
        return BLADE_REF_ERR_FAULT;
    if (r == 0)
        return BLADE_REF_ERR_NOTRANS;

    *ref_steps = latch_pos;
    return BLADE_REF_OK;
}

void blade_ref_integrity_init(blade_ref_integrity_t *st, uint32_t threshold)
{
    st->mismatch = 0;
    st->threshold = threshold;
}

uint8_t blade_ref_integrity_update(blade_ref_integrity_t *st,
                                    int32_t z_steps,
                                    uint8_t detected,
                                    int32_t ref_steps,
                                    int32_t tol_steps,
                                    int32_t dir)
{
    uint8_t expected;

    if (dir > 0)
        expected = (z_steps >= ref_steps - tol_steps) ? 1 : 0;
    else
        expected = (z_steps <= ref_steps + tol_steps) ? 1 : 0;

    if (detected == expected) {
        st->mismatch = 0;
        return 0;
    }
    st->mismatch++;
    if (st->mismatch >= st->threshold) {
        st->mismatch = 0;
        return 1;
    }
    return 0;
}
