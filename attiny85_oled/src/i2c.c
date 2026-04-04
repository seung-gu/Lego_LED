#include "i2c.h"
#include "stc8g.h"

#define SCL P3_2
#define SDA P3_3

static void i2c_delay(void) {
    // at 32kHz, instructions are already slow enough - no extra delay needed
}

void i2c_init(void) {
    // P3.2(SCL), P3.3(SDA) open-drain for I2C
    P3M0 |= 0x0C;
    P3M1 |= 0x0C;
    SCL = 1;
    SDA = 1;
}

void i2c_start(void) {
    SDA = 1;
    SCL = 1;
    i2c_delay();
    SDA = 0;
    i2c_delay();
    SCL = 0;
}

void i2c_stop(void) {
    SDA = 0;
    SCL = 1;
    i2c_delay();
    SDA = 1;
    i2c_delay();
}

uint8_t i2c_write(uint8_t dat) {
    uint8_t i, ack;
    for (i = 0; i < 8; i++) {
        SDA = (dat & 0x80) ? 1 : 0;
        dat <<= 1;
        i2c_delay();
        SCL = 1;
        i2c_delay();
        SCL = 0;
    }
    SDA = 1;
    i2c_delay();
    SCL = 1;
    i2c_delay();
    ack = SDA;
    SCL = 0;
    return ack;
}
