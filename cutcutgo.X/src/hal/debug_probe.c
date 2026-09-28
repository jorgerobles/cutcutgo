#include <stdint.h>
#include <string.h>

#include "hal/debug_probe.h"
#include "grbl/grbl/grbl.h"
#include "hal/motor.h"

#define SCL_MASK  (1 << 2) /* RA2 */
#define SDA_MASK  (1 << 3) /* RA3 */

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

#define ISL_REG_ID      0x00
#define ISL_REG_CFG1    0x01
#define ISL_REG_CFG2    0x02
#define ISL_REG_CFG3    0x03
#define ISL_REG_STATUS  0x08
#define ISL_REG_GREEN_L 0x09
#define ISL_REG_RED_L   0x0B
#define ISL_REG_BLUE_L  0x0D

static uint8_t dbg_i2c_write_verify(uint8_t addr, uint8_t reg, uint8_t val)
{
    uint8_t i, rb;

    for (i = 0; i < 3; i++) {
        dbg_i2c_write(addr, reg, val);
        if (i2c_read_reg(addr, reg, &rb) && rb == val)
            return 1;
        delay_ms(10);
    }
    return 0;
}

static void dbg_rgb(uint8_t addr)
{
    uint8_t id, cfg1, st, lo, hi;
    uint16_t g, r, b;
    int i;

    i2c_recover();
    if (!i2c_read_reg(addr, ISL_REG_ID, &id)) {
        printString("[DBG] rgb: id fail\r\n");
        return;
    }
    printString("[DBG] rgb id=");
    print_hex8(id);
    printString("\r\n");

    dbg_i2c_write(addr, ISL_REG_ID, 0x46);
    delay_ms(10);
    dbg_i2c_write_verify(addr, ISL_REG_CFG1, 0x0D);
    dbg_i2c_write_verify(addr, ISL_REG_CFG2, 0x3F);

    for (i = 0; i < 4; i++)
        delay_ms(100);

    if (!i2c_read_reg(addr, ISL_REG_CFG1, &cfg1) ||
        !i2c_read_reg(addr, ISL_REG_STATUS, &st)) {
        printString("[DBG] rgb: cfg fail\r\n");
        return;
    }
    printString("[DBG] rgb cfg1=");
    print_hex8(cfg1);
    printString(" st=");
    print_hex8(st);
    printString("\r\n");

    if (!i2c_read_reg(addr, ISL_REG_GREEN_L, &lo) ||
        !i2c_read_reg(addr, ISL_REG_GREEN_L + 1, &hi))
        goto fail;
    g = (uint16_t)(((uint16_t)hi << 8) | lo);
    if (!i2c_read_reg(addr, ISL_REG_RED_L, &lo) ||
        !i2c_read_reg(addr, ISL_REG_RED_L + 1, &hi))
        goto fail;
    r = (uint16_t)(((uint16_t)hi << 8) | lo);
    if (!i2c_read_reg(addr, ISL_REG_BLUE_L, &lo) ||
        !i2c_read_reg(addr, ISL_REG_BLUE_L + 1, &hi))
        goto fail;
    b = (uint16_t)(((uint16_t)hi << 8) | lo);

    printString("[DBG] rgb g=");
    printInteger((long)g);
    printString(" r=");
    printInteger((long)r);
    printString(" b=");
    printInteger((long)b);
    printString("\r\n");
    return;
fail:
    printString("[DBG] rgb: data fail\r\n");
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

static void dbg_pwm_regs(void)
{
    printString("[PWMREGS] T2CON=");
    print_hex8((T2CON >> 24) & 0xFF); print_hex8(T2CON & 0xFF);
    printString(" PR2=");
    print_hex8((PR2 >> 8) & 0xFF); print_hex8(PR2 & 0xFF);
    printString(" OC4CON=");
    print_hex8((OC4CON >> 8) & 0xFF); print_hex8(OC4CON & 0xFF);
    printString(" OC4RS=");
    print_hex8((OC4RS >> 8) & 0xFF); print_hex8(OC4RS & 0xFF);
    printString(" RPD3R=");
    print_hex8(RPD3R & 0xFF);
    printString(" RPD11R=");
    print_hex8(RPD11R & 0xFF);
    printString(" TRISD=");
    print_hex8((TRISD >> 8) & 0xFF); print_hex8(TRISD & 0xFF);
    printString(" LATD=");
    print_hex8((LATD >> 8) & 0xFF); print_hex8(LATD & 0xFF);
    printString("\r\n");
}

static void dbg_motor(const char *name, hal_motor_driver_t *m, uint8_t ccw, uint32_t ms)
{
    if (ms == 0 || ms > 2000)
        ms = 500;

    hal_motor_init(m, HAL_MOTOR_PWM);
    hal_motor_set_manual(m, true);
    hal_motor_set_direction(m, ccw ? HAL_MOTOR_DIR_CCW : HAL_MOTOR_DIR_CW);
    hal_motor_set_speed(m, HAL_MOTOR_SPEED_MIN);
    printString("[DBG] motor ");
    printString(name);
    printString(" run\r\n");
    dbg_pwm_regs();
    delay_ms(ms);
    hal_motor_set_direction(m, HAL_MOTOR_STOP);
    hal_motor_set_manual(m, false);
    hal_motor_set_speed(m, HAL_MOTOR_SPEED_MIN);
    printString("[DBG] motor stop\r\n");
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
    if (strncmp(line, "$DBGMOTOR=", 10) == 0) {
        const char *s = line + 10;
        uint8_t ccw = 0;
        uint32_t ms = 500;
        hal_motor_driver_t *m = NULL;
        const char *nm = "";

        if (strncmp(s, "Z1,", 3) == 0) { m = &HAL_MOTOR_Z1; nm = "Z1"; s += 3; }
        else if (strncmp(s, "Z2,", 3) == 0) { m = &HAL_MOTOR_Z2; nm = "Z2"; s += 3; }
        else if (strncmp(s, "A,", 2) == 0) { m = &HAL_MOTOR_A; nm = "A"; s += 2; }
        else if (strncmp(s, "X,", 2) == 0) { m = &HAL_MOTOR_X; nm = "X"; s += 2; }
        else if (strncmp(s, "Y,", 2) == 0) { m = &HAL_MOTOR_Y; nm = "Y"; s += 2; }
        else
            return STATUS_INVALID_STATEMENT;

        if (strncmp(s, "CCW,", 4) == 0) { ccw = 1; s += 4; }
        else if (strncmp(s, "CW,", 3) == 0) { s += 3; }
        else
            return STATUS_INVALID_STATEMENT;

        while (*s >= '0' && *s <= '9') {
            ms = ms * 10 + (uint32_t)(*s - '0');
            s++;
        }
        dbg_motor(nm, m, ccw, ms);
        return STATUS_OK;
    }
    return STATUS_INVALID_STATEMENT;
}

#ifdef SENSOR_BENCH

#include "hal/timer.h"

typedef enum {
    BENCH_SCAN,
    BENCH_ID,
    BENCH_ALT,
    BENCH_CFG,
    BENCH_WAIT,
    BENCH_PWR,
    BENCH_WAIT2,
    BENCH_READ
} bench_state_t;

static bench_state_t bench_state = BENCH_SCAN;
static uint32_t bench_next_ms;

static uint8_t i2c_probe(uint8_t addr)
{
    uint8_t ack;

    i2c_start();
    i2c_write_byte((uint8_t)(addr << 1));
    ack = i2c_read_ack();
    i2c_stop();
    return ack;
}

void debug_bench_task(void)
{
    uint32_t now = timer_get_ms();

    if ((int32_t)(now - bench_next_ms) < 0)
        return;
    bench_next_ms = now + 200;

    switch (bench_state) {
    case BENCH_SCAN: {
        uint8_t a;

        printString("[B] bus:");
        for (a = 0x03; a < 0x78; a++) {
            if (i2c_probe(a)) {
                printString(" ");
                print_hex8(a);
            }
        }
        printString("\r\n");
        i2c_recover();
        bench_state = BENCH_ID;
        break;
    }
    case BENCH_ID: {
        uint8_t id;

        i2c_recover();

        if (!i2c_read_reg(0x44, ISL_REG_ID, &id))
            printString("[B] id fail\r\n");
        else {
            printString("[B] id=");
            print_hex8(id);
            printString("\r\n");
        }
        bench_state = BENCH_CFG;
        break;
    }
    case BENCH_ALT: {
        uint8_t a, reg, v;

        for (a = 0x03; a <= 0x04; a++) {
            i2c_recover();
            printString("[B] ");
            print_hex8(a);
            printString(":");
            for (reg = 0; reg < 4; reg++) {
                if (i2c_read_reg(a, reg, &v)) {
                    printString(" ");
                    print_hex8(v);
                } else
                    printString(" --");
            }
            printString("\r\n");
        }
        i2c_recover();
        printString("[B] 44:");
        for (reg = 0; reg < 16; reg++) {
            if (i2c_read_reg(0x44, reg, &v)) {
                printString(" ");
                print_hex8(v);
            } else
                printString(" --");
        }
        printString("\r\n");
        bench_state = BENCH_CFG;
        break;
    }
    case BENCH_CFG: {
        uint8_t ok = 1;

        i2c_recover();
        if (!dbg_i2c_write_verify(0x44, ISL_REG_CFG1, 0x0D))
            ok = 0;
        if (!dbg_i2c_write_verify(0x44, ISL_REG_CFG2, 0x00))
            ok = 0;
        printString(ok ? "[B] set ok\r\n" : "[B] set FAIL\r\n");
        bench_state = BENCH_WAIT;
        bench_next_ms = timer_get_ms() + 400;
        break;
    }
    case BENCH_WAIT:
        bench_state = BENCH_READ;
        break;
    case BENCH_PWR: {
        static const uint8_t led_tab[][2] = {
            { 0x03, 0x00 }, { 0x03, 0x10 }, { 0x03, 0x20 }, { 0x03, 0x40 },
            { 0x03, 0x80 }, { 0x03, 0xFF },
            { 0x04, 0x00 }, { 0x04, 0xFF },
            { 0x05, 0x00 }, { 0x05, 0xFF },
            { 0x06, 0x00 }, { 0x06, 0xFF },
            { 0x07, 0x00 }, { 0x07, 0xFF },
            { 0x01, 0x0C }, { 0x01, 0x0E }, { 0x01, 0x0F },
            { 0x01, 0x1D }, { 0x01, 0x2D }, { 0x01, 0x4D }, { 0x01, 0x8D },
            { 0x01, 0xCD }, { 0x01, 0xDD },
        };
        static uint8_t idx;

        i2c_recover();
        dbg_i2c_write_verify(0x44, ISL_REG_CFG1, 0x0D);
        dbg_i2c_write_verify(0x44, ISL_REG_CFG2, 0x00);
        dbg_i2c_write(0x44, led_tab[idx][0], led_tab[idx][1]);
        printString("[B] led r=");
        print_hex8(led_tab[idx][0]);
        printString(" v=");
        print_hex8(led_tab[idx][1]);
        printString("\r\n");
        idx = (uint8_t)((idx + 1) % (sizeof(led_tab) / sizeof(led_tab[0])));
        bench_state = BENCH_WAIT2;
        bench_next_ms = timer_get_ms() + 200;
        break;
    }
    case BENCH_WAIT2:
        bench_state = BENCH_READ;
        break;
    case BENCH_READ: {
        uint8_t d[6];
        uint8_t reg;

        i2c_recover();
        for (reg = 0; reg < 6; reg++) {
            if (!i2c_read_reg(0x44, (uint8_t)(ISL_REG_GREEN_L + reg), &d[reg]))
                goto read_fail;
        }
        printString("[B] d:");
        for (reg = 0; reg < 6; reg++) {
            printString(" ");
            print_hex8(d[reg]);
        }
        printString("\r\n");
        break;
read_fail:
        printString("[B] read FAIL\r\n");
        bench_state = BENCH_SCAN;
        break;
    }
    default:
        break;
    }
}

#endif /* SENSOR_BENCH */
