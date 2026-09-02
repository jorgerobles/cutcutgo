#include "hal/sensors.h"
#include "sensors_sim.h"

static int32_t mark_value;
static uint8_t mark_timeout;
static uint8_t mark_fault;

static uint8_t blade_detected;
static uint8_t blade_timeout;
static uint8_t blade_fault;
static uint8_t blade_unsupported;

void sim_mark_set(int32_t value_q16)
{
    mark_value = value_q16;
}

void sim_mark_timeout(uint8_t on)
{
    mark_timeout = on;
}

void sim_mark_fault(uint8_t on)
{
    mark_fault = on;
}

void sim_blade_set(uint8_t detected)
{
    blade_detected = detected;
}

void sim_blade_timeout(uint8_t on)
{
    blade_timeout = on;
}

void sim_blade_fault(uint8_t on)
{
    blade_fault = on;
}

void sim_blade_unsupported(uint8_t on)
{
    blade_unsupported = on;
}

sensor_status_t mark_detector_init(void)
{
    return SENSOR_OK;
}

sensor_status_t mark_detector_read(int32_t *value_q16)
{
    if (mark_timeout)
        return SENSOR_TIMEOUT;
    if (mark_fault)
        return SENSOR_FAULT;
    *value_q16 = mark_value;
    return SENSOR_OK;
}

sensor_status_t mark_detector_selftest(void)
{
    return mark_fault ? SENSOR_FAULT : SENSOR_OK;
}

sensor_status_t blade_detector_init(void)
{
    return SENSOR_OK;
}

sensor_status_t blade_detector_read(uint8_t *detected)
{
    if (blade_unsupported)
        return SENSOR_UNSUPPORTED;
    if (blade_timeout)
        return SENSOR_TIMEOUT;
    if (blade_fault)
        return SENSOR_FAULT;
    *detected = blade_detected;
    return SENSOR_OK;
}

sensor_status_t blade_detector_selftest(void)
{
    if (blade_unsupported)
        return SENSOR_UNSUPPORTED;
    return blade_fault ? SENSOR_FAULT : SENSOR_OK;
}
