#include "i2c.h"
#include <avr/io.h>

// ATtiny85 I2C pins (same as TinyWireM USI pins)
#define SDA_BIT PB0   // pin 5 on DIP8
#define SCL_BIT PB2   // pin 7 on DIP8

#define SDA_HIGH() (PORTB |= (1 << SDA_BIT))
#define SDA_LOW()  (PORTB &= ~(1 << SDA_BIT))
#define SCL_HIGH() (PORTB |= (1 << SCL_BIT))
#define SCL_LOW()  (PORTB &= ~(1 << SCL_BIT))
#define SDA_READ() (PINB & (1 << SDA_BIT))

#define SDA_OUT()  (DDRB |= (1 << SDA_BIT))
#define SDA_IN()   (DDRB &= ~(1 << SDA_BIT))
#define SCL_OUT()  (DDRB |= (1 << SCL_BIT))

static void i2c_delay(void) {
    // at 128kHz, instructions are slow enough
    __asm__ __volatile__("nop");
}

void i2c_init(void) {
    SDA_HIGH();
    SCL_HIGH();
    SDA_OUT();
    SCL_OUT();
}

void i2c_start(void) {
    SDA_HIGH();
    SCL_HIGH();
    i2c_delay();
    SDA_LOW();
    i2c_delay();
    SCL_LOW();
}

void i2c_stop(void) {
    SDA_LOW();
    SCL_HIGH();
    i2c_delay();
    SDA_HIGH();
    i2c_delay();
}

uint8_t i2c_write(uint8_t dat) {
    uint8_t i, ack;
    for (i = 0; i < 8; i++) {
        if (dat & 0x80) SDA_HIGH();
        else SDA_LOW();
        dat <<= 1;
        i2c_delay();
        SCL_HIGH();
        i2c_delay();
        SCL_LOW();
    }
    SDA_IN();
    SDA_HIGH(); // pull-up
    i2c_delay();
    SCL_HIGH();
    i2c_delay();
    ack = SDA_READ() ? 1 : 0;
    SCL_LOW();
    SDA_OUT();
    return ack;
}
