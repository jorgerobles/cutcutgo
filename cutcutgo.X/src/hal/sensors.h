#ifndef __INC_HAL_SENSORS_H
#define __INC_HAL_SENSORS_H

#include <stdint.h>

typedef enum {
    SENSOR_OK = 0,
    SENSOR_TIMEOUT,
    SENSOR_FAULT,
    SENSOR_UNSUPPORTED
} sensor_status_t;

sensor_status_t mark_detector_init(void);
sensor_status_t mark_detector_read(int32_t *value_q16);
sensor_status_t mark_detector_selftest(void);

/* Raw register access to the head sensor (debug/spike only). */
uint8_t sensors_isl_read_reg(uint8_t reg, uint8_t *val);
uint8_t sensors_isl_write_reg(uint8_t reg, uint8_t val);

sensor_status_t blade_detector_init(void);
sensor_status_t blade_detector_read(uint8_t *detected);
sensor_status_t blade_detector_selftest(void);

#endif /* __INC_HAL_SENSORS_H */
