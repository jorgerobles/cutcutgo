#include "hal/sensors.h"

sensor_status_t mark_detector_init(void)
{
    return SENSOR_UNSUPPORTED;
}

sensor_status_t mark_detector_read(int32_t *value_q16)
{
    (void)value_q16;
    return SENSOR_UNSUPPORTED;
}

sensor_status_t mark_detector_selftest(void)
{
    return SENSOR_FAULT;
}

sensor_status_t blade_detector_init(void)
{
    return SENSOR_UNSUPPORTED;
}

sensor_status_t blade_detector_read(uint8_t *detected)
{
    (void)detected;
    return SENSOR_UNSUPPORTED;
}

sensor_status_t blade_detector_selftest(void)
{
    return SENSOR_FAULT;
}
