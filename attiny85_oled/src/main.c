#include <avr/io.h>
#include <avr/pgmspace.h>
#include <avr/sleep.h>
#include <avr/interrupt.h>
#include "i2c.h"
#include "ssd1306.h"
#include "duck.h"

#define OLED_W      72
#define DUCK_W      24
#define DUCK_FRAMES 8
#define PACE        1

static void delay_ms(uint16_t ms) {
    volatile uint16_t i;
    while (ms--) {
        i = 100;  // ~1ms at 1MHz
        while (i--);
    }
}

// 128kHz용 delay
static void delay_ms_128k(uint16_t ms) {
    volatile uint16_t i;
    while (ms--) {
        i = 12;  // ~1ms at 128kHz
        while (i--);
    }
}

// read VCC using internal 1.1V bandgap reference
static uint16_t read_vcc(void) {
    ADMUX = 0x0C;  // VCC ref, bandgap input
    ADCSRA = (1 << ADEN) | (1 << ADPS1) | (1 << ADPS0);  // enable, /8
    delay_ms(2);

    ADCSRA |= (1 << ADSC);
    while (ADCSRA & (1 << ADSC));

    uint16_t adc_val = ADC;
    ADCSRA &= ~(1 << ADEN);  // disable ADC

    if (adc_val == 0) return 5000;
    return (uint16_t)(1125300UL / adc_val);
}

static int16_t map_val(int16_t x, int16_t in_min, int16_t in_max,
                       int16_t out_min, int16_t out_max) {
    return (int16_t)((int32_t)(x - in_min) * (out_max - out_min)
                     / (in_max - in_min) + out_min);
}

// watchdog interrupt - empty, just wakes MCU
ISR(WDT_vect) {}

// power-down sleep with specified WDP bits
static void sleep_wdt_raw(uint8_t wdp) {
    cli();
    MCUSR &= ~(1 << WDRF);
    WDTCR = (1 << WDCE) | (1 << WDE);
    WDTCR = (1 << WDIE) | wdp;
    sei();

    set_sleep_mode(SLEEP_MODE_PWR_DOWN);
    sleep_enable();
    sleep_cpu();
    sleep_disable();

    cli();
    MCUSR &= ~(1 << WDRF);
    WDTCR = (1 << WDCE) | (1 << WDE);
    WDTCR = 0;
    sei();
}

// ~125ms single sleep
static void sleep_125ms(void) {
    sleep_wdt_raw((1 << WDP1) | (1 << WDP0)) ;
}

int main(void) {
    uint8_t pre_x0 = 0;
    uint8_t anim_frame = 0;
    uint8_t c, pg;
    uint8_t x0;
    int8_t diff_x;
    int16_t voltage, gauge;

    ADCSRA &= ~(1 << ADEN);
    ACSR |= (1 << ACD);              // Analog Comparator 비활성화
    PRR = (1 << PRTIM1) | (1 << PRUSI);

    i2c_init();
    ssd1306_init();

    // ground line at page 4
    ssd1306_set_cursor(0, 4);
    ssd1306_data_start();
    for (c = 0; c < OLED_W; c++)
        ssd1306_data_byte(0x80);
    ssd1306_data_end();

    while (1) {
        voltage = (int16_t)read_vcc();
        gauge = map_val(voltage, 1300, 5000, 0, OLED_W - DUCK_W);
        if (gauge < 0) gauge = 0;
        if (gauge > OLED_W - DUCK_W) gauge = OLED_W - DUCK_W;

        x0 = (uint8_t)gauge;
        diff_x = (int8_t)(x0 - pre_x0);

        if (diff_x > PACE) {
            x0 = x0 - (diff_x - PACE);
            diff_x = PACE;
        } else if (diff_x < -PACE) {
            x0 = x0 - (diff_x + PACE);
            diff_x = -PACE;
        }

        if (diff_x > 0) {
            for (pg = 1; pg < 4; pg++) {
                ssd1306_set_cursor(pre_x0, pg);
                ssd1306_data_start();
                for (c = 0; c < (uint8_t)diff_x; c++)
                    ssd1306_data_byte(0x00);
                ssd1306_data_end();
            }
            ssd1306_set_cursor(pre_x0, 4);
            ssd1306_data_start();
            for (c = 0; c < (uint8_t)diff_x; c++)
                ssd1306_data_byte(0x80);
            ssd1306_data_end();
        } else if (diff_x < 0) {
            uint8_t trail = pre_x0 + DUCK_W + diff_x;
            for (pg = 1; pg < 4; pg++) {
                ssd1306_set_cursor(trail, pg);
                ssd1306_data_start();
                for (c = 0; c < (uint8_t)(-diff_x); c++)
                    ssd1306_data_byte(0x00);
                ssd1306_data_end();
            }
            ssd1306_set_cursor(trail, 4);
            ssd1306_data_start();
            for (c = 0; c < (uint8_t)(-diff_x); c++)
                ssd1306_data_byte(0x80);
            ssd1306_data_end();
        }

        ssd1306_bitmap(x0, 1, x0 + DUCK_W, 4, epd_bitmap_duckArray[anim_frame]);

        const uint8_t *fdata = epd_bitmap_duckArray[anim_frame];
        ssd1306_set_cursor(x0, 4);
        ssd1306_data_start();
        for (c = 0; c < DUCK_W; c++)
            ssd1306_data_byte(pgm_read_byte(&fdata[3 * DUCK_W + c]) | 0x80);
        ssd1306_data_end();

        anim_frame = (anim_frame + 1) % DUCK_FRAMES;
        pre_x0 = x0;

        sleep_125ms();
    }

    return 0;
}
