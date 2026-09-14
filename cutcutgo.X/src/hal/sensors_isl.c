#include <stdint.h>

#include "hal/sensors.h"
#include "config/cutcutgo/peripheral/gpio/plib_gpio.h"

#define SCL_MASK (1 << 2)
#define SDA_MASK (1 << 3)

#define ISL_ADDR           0x44
#define ISL_REG_ID         0x00
#define ISL_REG_CFG1       0x01
#define ISL_REG_CFG2       0x02
#define ISL_REG_CH_A       0x0A
#define ISL_ID_VALUE       0x7D
#define ISL_BLADE_THRESHOLD 50

static void dly(void)
{
    volatile int i;
    for (i = 0; i < 2400; i++) { }
}

static void scl_hi(void)
{
    volatile uint16_t t = 10000;

    GPIO_PortInputEnable(GPIO_PORT_A, SCL_MASK);
    while (t-- && !GPIO_PinRead(GPIO_PIN_RA2)) { }
}

static void scl_lo(void)
{
    GPIO_PortOutputEnable(GPIO_PORT_A, SCL_MASK);
    GPIO_PortClear(GPIO_PORT_A, SCL_MASK);
}

static void sda_out(uint8_t v)
{
    if (v) {
        GPIO_PortInputEnable(GPIO_PORT_A, SDA_MASK);
    } else {
        GPIO_PortOutputEnable(GPIO_PORT_A, SDA_MASK);
        GPIO_PortClear(GPIO_PORT_A, SDA_MASK);
    }
}

static void sda_in(void)
{
    GPIO_PortInputEnable(GPIO_PORT_A, SDA_MASK);
}

static uint8_t sda_read(void)
{
    return GPIO_PinRead(GPIO_PIN_RA3) ? 1 : 0;
}

static void i2c_start(void)
{
    sda_out(1);
    scl_hi();
    dly();
    sda_out(0);
    dly();
    scl_lo();
    dly();
}

static void i2c_stop(void)
{
    sda_out(0);
    scl_lo();
    dly();
    scl_hi();
    dly();
    sda_out(1);
    dly();
}

static void i2c_write_bit(uint8_t b)
{
    sda_out(b);
    dly();
    scl_hi();
    dly();
    scl_lo();
}

static void i2c_write_byte(uint8_t byte)
{
    int bit;

    for (bit = 7; bit >= 0; bit--)
        i2c_write_bit((byte >> bit) & 1);
}

static uint8_t i2c_read_ack(void)
{
    uint8_t ack;

    sda_in();
    scl_hi();
    dly();
    ack = (sda_read() == 0) ? 1 : 0;
    scl_lo();
    return ack;
}

static uint8_t i2c_read_byte(void)
{
    uint8_t byte = 0;
    int bit;

    sda_in();
    for (bit = 7; bit >= 0; bit--) {
        scl_hi();
        dly();
        if (sda_read())
            byte |= (uint8_t)(1 << bit);
        scl_lo();
        dly();
    }
    return byte;
}

static void i2c_write_ack(void)
{
    sda_out(0);
    dly();
    scl_hi();
    dly();
    scl_lo();
}

static void i2c_recover(void)
{
    int i;

    sda_in();
    for (i = 0; i < 9; i++) {
        scl_hi();
        dly();
        scl_lo();
        dly();
    }
    i2c_stop();
}

static uint8_t isl_read_reg(uint8_t reg, uint8_t *val)
{
    uint8_t i;

    for (i = 0; i < 2; i++) {
        i2c_start();
        i2c_write_byte((uint8_t)(ISL_ADDR << 1));
        if (i2c_read_ack()) {
            i2c_write_byte(reg);
            if (i2c_read_ack()) {
                i2c_start();
                i2c_write_byte((uint8_t)((ISL_ADDR << 1) | 1));
                if (i2c_read_ack()) {
                    *val = i2c_read_byte();
                    i2c_write_ack();
                    i2c_stop();
                    return 1;
                }
            }
        }
        i2c_stop();
        i2c_recover();
    }
    return 0;
}

uint8_t sensors_isl_read_reg(uint8_t reg, uint8_t *val)
{
    return isl_read_reg(reg, val);
}

/* Raw I2C presence probe at an arbitrary 7-bit address (debug/spike only). */
uint8_t sensors_isl_probe(uint8_t addr)
{
    uint8_t ack;

    i2c_recover();
    i2c_start();
    i2c_write_byte((uint8_t)(addr << 1));
    ack = i2c_read_ack();
    i2c_stop();
    return ack;
}

static uint8_t isl_write_reg(uint8_t reg, uint8_t val)
{
    uint8_t i;

    for (i = 0; i < 2; i++) {
        i2c_start();
        i2c_write_byte((uint8_t)(ISL_ADDR << 1));
        if (i2c_read_ack()) {
            i2c_write_byte(reg);
            if (i2c_read_ack()) {
                i2c_write_byte(val);
                if (i2c_read_ack()) {
                    i2c_stop();
                    return 1;
                }
            }
        }
        i2c_stop();
        i2c_recover();
    }
    return 0;
}

uint8_t sensors_isl_write_reg(uint8_t reg, uint8_t val)
{
    return isl_write_reg(reg, val);
}


static uint8_t isl_configured;

static sensor_status_t isl_setup(void)
{
    uint8_t v;

    if (isl_configured)
        return SENSOR_OK;

    i2c_recover();
    if (!isl_write_reg(ISL_REG_CFG1, 0x05) ||
        !isl_write_reg(ISL_REG_CFG2, 0x00))
        return SENSOR_FAULT;
    if (!isl_read_reg(ISL_REG_CFG1, &v) || v != 0x05)
        return SENSOR_FAULT;
    isl_configured = 1;
    return SENSOR_OK;
}

static sensor_status_t isl_selftest(void)
{
    uint8_t v;

    i2c_recover();
    if (!isl_read_reg(ISL_REG_ID, &v))
        return SENSOR_TIMEOUT;
    if (v != ISL_ID_VALUE)
        return SENSOR_FAULT;
    return SENSOR_OK;
}

sensor_status_t mark_detector_init(void)
{
    return isl_setup();
}

sensor_status_t mark_detector_read(int32_t *value_q16)
{
    uint8_t dummy, v;
    sensor_status_t st = isl_setup();

    if (st != SENSOR_OK)
        return st;
    if (!isl_read_reg(ISL_REG_CH_A - 1, &dummy))
        return SENSOR_TIMEOUT;
    if (!isl_read_reg(ISL_REG_CH_A, &v))
        return SENSOR_TIMEOUT;
    *value_q16 = (int32_t)v << 8;
    return SENSOR_OK;
}

sensor_status_t mark_detector_selftest(void)
{
    return isl_selftest();
}

sensor_status_t blade_detector_init(void)
{
    return isl_setup();
}

sensor_status_t blade_detector_read(uint8_t *detected)
{
    uint8_t dummy, v;
    sensor_status_t st = isl_setup();

    if (st != SENSOR_OK)
        return st;
    if (!isl_read_reg(ISL_REG_CH_A - 1, &dummy))
        return SENSOR_TIMEOUT;
    if (!isl_read_reg(ISL_REG_CH_A, &v))
        return SENSOR_TIMEOUT;
    *detected = (v > ISL_BLADE_THRESHOLD) ? 1 : 0;
    return SENSOR_OK;
}

sensor_status_t blade_detector_selftest(void)
{
    return isl_selftest();
}
