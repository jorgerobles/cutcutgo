#include <stdint.h>
#include <string.h>

#include "hal/debug_probe.h"
#include "grbl/grbl/grbl.h"

#define SCL_MASK  (1 << 2) /* RA2 */
#define SDA_MASK  (1 << 3) /* RA3 */

static void dly(void)
{
    volatile int i;
    for (i = 0; i < 240; i++) { }
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

static void print_hex8(uint8_t v)
{
    char buf[5];
    const char *hex = "0123456789ABCDEF";

    buf[0] = '0';
    buf[1] = 'x';
    buf[2] = hex[(v >> 4) & 0xF];
    buf[3] = hex[v & 0xF];
    buf[4] = 0;
    printString(buf);
}

static void i2c_write_byte(uint8_t byte)
{
    int bit;
    for (bit = 7; bit >= 0; bit--)
        i2c_write_bit((byte >> bit) & 1);
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
            byte |= (1 << bit);
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

static uint8_t i2c_read_reg(uint8_t addr, uint8_t reg, uint8_t *val)
{
    i2c_start();
    i2c_write_byte((addr << 1) & 0xFE);
    if (!i2c_read_ack()) {
        i2c_stop();
        return 0;
    }
    i2c_write_byte(reg);
    if (!i2c_read_ack()) {
        i2c_stop();
        return 0;
    }
    i2c_start();
    i2c_write_byte((addr << 1) | 0x01);
    if (!i2c_read_ack()) {
        i2c_stop();
        return 0;
    }
    *val = i2c_read_byte();
    i2c_write_ack();
    i2c_stop();
    return 1;
}

static void dbg_i2c_read(uint8_t addr, uint8_t reg)
{
    uint8_t val;

    printString("[DBG] i2c read addr=");
    print_hex8(addr);
    printString(" reg=");
    print_hex8(reg);
    printString("\r\n");

    if (!i2c_read_reg(addr, reg, &val)) {
        printString("[DBG] i2c read fail\r\n");
        return;
    }
    printString("[DBG] i2c val=");
    print_hex8(val);
    printString("\r\n");
}

static void dbg_i2c_write(uint8_t addr, uint8_t reg, uint8_t val)
{
    i2c_start();
    i2c_write_byte((addr << 1) & 0xFE);
    if (!i2c_read_ack()) {
        printString("[DBG] i2c no ack (write)\r\n");
        i2c_stop();
        return;
    }
    i2c_write_byte(reg);
    if (!i2c_read_ack()) {
        printString("[DBG] i2c no ack (reg)\r\n");
        i2c_stop();
        return;
    }
    i2c_write_byte(val);
    if (!i2c_read_ack()) {
        printString("[DBG] i2c no ack (val)\r\n");
        i2c_stop();
        return;
    }
    i2c_stop();
    printString("[DBG] i2c write ok\r\n");
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

static void i2c_nack(void)
{
    sda_in();
    dly();
    scl_hi();
    dly();
    scl_lo();
}

static uint8_t i2c_set_pointer(uint8_t addr, uint8_t reg)
{
    i2c_start();
    i2c_write_byte((uint8_t)(addr << 1));
    if (!i2c_read_ack()) {
        i2c_stop();
        return 0;
    }
    i2c_write_byte(reg);
    if (!i2c_read_ack()) {
        i2c_stop();
        return 0;
    }
    i2c_stop();
    return 1;
}

static uint8_t i2c_read_bytes(uint8_t addr, uint8_t *buf, uint8_t n)
{
    uint8_t i;

    i2c_start();
    i2c_write_byte((uint8_t)((addr << 1) | 1));
    if (!i2c_read_ack()) {
        i2c_stop();
        return 0;
    }
    for (i = 0; i < n; i++) {
        buf[i] = i2c_read_byte();
        if ((uint8_t)(i + 1) < n)
            i2c_write_ack();
        else
            i2c_nack();
    }
    i2c_stop();
    return 1;
}

static void dbg_rgb(uint8_t addr)
{
    uint8_t buf[6];
    uint8_t cfg;
    uint16_t g, r, b;

    i2c_recover();
    dbg_i2c_write(addr, 0x01, 0x05);
    delay_ms(150);
    delay_ms(150);
    if (!i2c_set_pointer(addr, 0x01)) {
        printString("[DBG] rgb: ptr fail\r\n");
        return;
    }
    if (!i2c_read_reg(addr, 0x01, &cfg)) {
        printString("[DBG] rgb: cfg read fail\r\n");
        return;
    }
    printString("[DBG] rgb cfg1=");
    print_hex8(cfg);
    printString("\r\n");
    if (!i2c_set_pointer(addr, 0x04)) {
        printString("[DBG] rgb: ptr fail\r\n");
        return;
    }
    if (!i2c_read_bytes(addr, buf, 6)) {
        printString("[DBG] rgb: seq read fail\r\n");
        return;
    }
    g = (uint16_t)(((uint16_t)buf[1] << 8) | buf[0]);
    r = (uint16_t)(((uint16_t)buf[3] << 8) | buf[2]);
    b = (uint16_t)(((uint16_t)buf[5] << 8) | buf[4]);
    printString("[DBG] rgb g=");
    printInteger((long)g);
    printString(" r=");
    printInteger((long)r);
    printString(" b=");
    printInteger((long)b);
    printString("\r\n");
}

static void dbg_i2c_scan(void)
{
    uint8_t addr;

    printString("[DBG] i2c scan start\r\n");
    GPIO_PortInputEnable(GPIO_PORT_A, SDA_MASK);
    scl_hi();
    sda_out(1);
    for (addr = 0x08; addr < 0x78; addr++) {
        uint8_t byte = (uint8_t)(addr << 1);
        int bit;

        i2c_start();
        for (bit = 7; bit >= 0; bit--)
            i2c_write_bit((byte >> bit) & 1);
        if (i2c_read_ack()) {
            printString("[DBG] i2c ack ");
            print_hex8(addr);
            printString("\r\n");
        }
        i2c_stop();
    }
    sda_in();
    printString("[DBG] i2c scan done\r\n");
}

static void probe_pin(const char *name, GPIO_PIN pin, uint32_t port, uint32_t mask)
{
    uint8_t cur = GPIO_PinRead(pin) ? 1 : 0;
    uint8_t inlvl;
    uint8_t drv1;
    uint8_t drv0;

    GPIO_PortInputEnable(port, mask);
    delay_ms(1);
    inlvl = GPIO_PinRead(pin) ? 1 : 0;
    GPIO_PortOutputEnable(port, mask);
    GPIO_PortSet(port, mask);
    delay_ms(1);
    drv1 = GPIO_PinRead(pin) ? 1 : 0;
    GPIO_PortClear(port, mask);
    delay_ms(1);
    drv0 = GPIO_PinRead(pin) ? 1 : 0;
    GPIO_PortInputEnable(port, mask);

    printString("[DBG] pin ");
    printString(name);
    printString(" cur=");
    printInteger(cur);
    printString(" in=");
    printInteger(inlvl);
    printString(" drv1=");
    printInteger(drv1);
    printString(" drv0=");
    printInteger(drv0);
    printString("\r\n");
}

static void dbg_power(void)
{
    probe_pin("RA2", GPIO_PIN_RA2, GPIO_PORT_A, SCL_MASK);
    probe_pin("RA3", GPIO_PIN_RA3, GPIO_PORT_A, SDA_MASK);
    probe_pin("RD7", GPIO_PIN_RD7, GPIO_PORT_D, (1 << 7));
    probe_pin("RD8", GPIO_PIN_RD8, GPIO_PORT_D, (1 << 8));
    probe_pin("RD9", GPIO_PIN_RD9, GPIO_PORT_D, (1 << 9));
}

static void dbg_sample(uint32_t n)
{
    uint8_t prev[5];
    uint32_t trans[5];
    uint32_t i;
    int p;

    if (n == 0 || n > 2000)
        n = 200;

    prev[0] = GPIO_PinRead(GPIO_PIN_RA2) ? 1 : 0;
    prev[1] = GPIO_PinRead(GPIO_PIN_RA3) ? 1 : 0;
    prev[2] = GPIO_PinRead(GPIO_PIN_RD7) ? 1 : 0;
    prev[3] = GPIO_PinRead(GPIO_PIN_RD8) ? 1 : 0;
    prev[4] = GPIO_PinRead(GPIO_PIN_RD9) ? 1 : 0;
    trans[0] = trans[1] = trans[2] = trans[3] = trans[4] = 0;

    printString("[DBG] samp start n=");
    printInteger((long)n);
    printString("\r\n");

    for (i = 0; i < n; i++) {
        uint8_t now[5];

        now[0] = GPIO_PinRead(GPIO_PIN_RA2) ? 1 : 0;
        now[1] = GPIO_PinRead(GPIO_PIN_RA3) ? 1 : 0;
        now[2] = GPIO_PinRead(GPIO_PIN_RD7) ? 1 : 0;
        now[3] = GPIO_PinRead(GPIO_PIN_RD8) ? 1 : 0;
        now[4] = GPIO_PinRead(GPIO_PIN_RD9) ? 1 : 0;
        for (p = 0; p < 5; p++) {
            if (now[p] != prev[p]) {
                trans[p]++;
                prev[p] = now[p];
            }
        }
        delay_ms(1);
    }

    {
        static const char *names[5] = { "RA2", "RA3", "RD7", "RD8", "RD9" };

        for (p = 0; p < 5; p++) {
            printString("[DBG] samp ");
            printString(names[p]);
            printString(" t=");
            printInteger((long)trans[p]);
            printString(" last=");
            printInteger(prev[p]);
            printString("\r\n");
        }
    }
    printString("[DBG] samp done\r\n");
}

uint8_t debug_probe_execute(const char *line)
{
    if (strncmp(line, "$DBGI2CW", 8) == 0 && line[8] == '=') {
        uint8_t addr = 0;
        uint8_t reg = 0;
        uint8_t val = 0;
        const char *s = line + 9;

        while (*s >= '0' && *s <= '9') {
            addr = addr * 10 + (uint8_t)(*s - '0');
            s++;
        }
        if (*s == ',') {
            s++;
            while (*s >= '0' && *s <= '9') {
                reg = reg * 10 + (uint8_t)(*s - '0');
                s++;
            }
        }
        if (*s == ',') {
            s++;
            while (*s >= '0' && *s <= '9') {
                val = val * 10 + (uint8_t)(*s - '0');
                s++;
            }
        }
        dbg_i2c_write(addr, reg, val);
        return STATUS_OK;
    }
    if (strncmp(line, "$DBGRGB", 7) == 0) {
        uint8_t addr = 68;

        if (line[7] == '=') {
            const char *s = line + 8;

            addr = 0;
            while (*s >= '0' && *s <= '9') {
                addr = addr * 10 + (uint8_t)(*s - '0');
                s++;
            }
        }
        dbg_rgb(addr);
        return STATUS_OK;
    }
    if (strncmp(line, "$DBGI2CR", 8) == 0 && line[8] == '=') {
        uint8_t addr = 0;
        uint8_t reg = 0;
        const char *s = line + 9;

        while (*s >= '0' && *s <= '9') {
            addr = addr * 10 + (uint8_t)(*s - '0');
            s++;
        }
        if (*s == ',') {
            s++;
            while (*s >= '0' && *s <= '9') {
                reg = reg * 10 + (uint8_t)(*s - '0');
                s++;
            }
        }
        dbg_i2c_read(addr, reg);
        return STATUS_OK;
    }
    if (strncmp(line, "$DBGI2C", 7) == 0) {
        dbg_i2c_scan();
        return STATUS_OK;
    }
    if (strncmp(line, "$DBGPWR", 7) == 0) {
        dbg_power();
        return STATUS_OK;
    }
    if (strncmp(line, "$DBGSAMP", 8) == 0) {
        uint32_t n = 0;

        if (line[8] == '=') {
            const char *s = line + 9;

            while (*s >= '0' && *s <= '9') {
                n = n * 10 + (uint32_t)(*s - '0');
                s++;
            }
        }
        dbg_sample(n);
        return STATUS_OK;
    }
    return STATUS_INVALID_STATEMENT;
}
