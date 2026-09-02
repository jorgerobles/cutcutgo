#ifndef __INC_GRBL_BLADE_REF_H
#define __INC_GRBL_BLADE_REF_H

#include <stdint.h>

typedef struct {
    int32_t (*get_z_steps)(void);
    void (*move_z)(int32_t target_steps, int32_t rate);
    void (*stop)(void);
    uint8_t (*abort_requested)(void);
} blade_ref_motion_t;

#define BLADE_REF_OK            0
#define BLADE_REF_ERR_FAULT     1
#define BLADE_REF_ERR_NOTRANS   2
#define BLADE_REF_ERR_UNSUPPORTED 3
#define BLADE_REF_ERR_ABORT     4

uint8_t blade_ref_run(const blade_ref_motion_t *m,
                      int32_t dir,
                      int32_t max_travel_steps,
                      int32_t retract_steps,
                      int32_t chunk_steps,
                      int32_t seek_rate,
                      int32_t latch_rate,
                      uint8_t debounce,
                      int32_t *ref_steps);

typedef struct {
    uint32_t mismatch;
    uint32_t threshold;
} blade_ref_integrity_t;

void blade_ref_integrity_init(blade_ref_integrity_t *st, uint32_t threshold);

uint8_t blade_ref_integrity_update(blade_ref_integrity_t *st,
                                    int32_t z_steps,
                                    uint8_t detected,
                                    int32_t ref_steps,
                                    int32_t tol_steps,
                                    int32_t dir);

#endif /* __INC_GRBL_BLADE_REF_H */
