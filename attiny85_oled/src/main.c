#include "stc8g.h"
#include "i2c.h"
#include "ssd1306.h"
#include "duck.h"

#define OLED_W   72
#define PACE     1

// ---- delay (approximate, ~1ms at 24MHz 1T) ----
static void delay_ms(uint16_t ms) {
    uint16_t i;
    while (ms--) {
        i = 2400;
        while (i--);
    }
}

// ---- ADC: read VCC via internal bandgap (ch15) ----
static uint16_t read_vcc(void) {
    uint16_t adc_val;

    // right-justified result, slow ADC clock for accuracy
    ADCCFG = 0x2F;                      // RESFMT=1, speed=15
    ADC_CONTR = ADC_POWER | 0x0F;       // power on, ch15 (bandgap ~1.19V)
    delay_ms(1);

    ADC_CONTR |= ADC_START;
    while (!(ADC_CONTR & ADC_FLAG));
    ADC_CONTR &= ~ADC_FLAG;

    adc_val = ((uint16_t)ADC_RES << 8) | ADC_RESL;
    if (adc_val == 0) return 5000;

    // VCC(mV) = 1190 * 1024 / adc_val
    return (uint16_t)(1218560UL / adc_val);
}

// ---- map (same as Arduino map) ----
static int16_t map_val(int16_t x, int16_t in_min, int16_t in_max,
                       int16_t out_min, int16_t out_max) {
    return (int16_t)((int32_t)(x - in_min) * (out_max - out_min)
                     / (in_max - in_min) + out_min);
}

void main(void) {
    uint8_t pre_x0 = 0;
    uint8_t anim_frame = 0;
    uint8_t c, pg;
    uint8_t x0;
    int8_t diff_x;
    int16_t voltage, gauge;
    __code const uint8_t *fdata;

    WDT_CONTR = 0x00;          // disable watchdog

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
        gauge = map_val(voltage, 1300, 5000, 0, OLED_W - DUCK_WIDTH);
        if (gauge < 0) gauge = 0;
        if (gauge > OLED_W - DUCK_WIDTH) gauge = OLED_W - DUCK_WIDTH;

        x0 = (uint8_t)gauge;
        diff_x = (int8_t)(x0 - pre_x0);

        // pace control
        if (diff_x > PACE) {
            x0 = x0 - (diff_x - PACE);
            diff_x = PACE;
        } else if (diff_x < -PACE) {
            x0 = x0 - (diff_x + PACE);
            diff_x = -PACE;
        }

        // erase trailing columns
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
                ssd1306_data_byte(0x80);    // restore ground line
            ssd1306_data_end();
        } else if (diff_x < 0) {
            uint8_t trail = pre_x0 + DUCK_WIDTH + diff_x;
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

        // draw duck on pages 1-3
        ssd1306_bitmap(x0, 1, x0 + DUCK_WIDTH, 4, duck_frames[anim_frame]);

        // draw page 4 with ground line OR'd in
        fdata = duck_frames[anim_frame];
        ssd1306_set_cursor(x0, 4);
        ssd1306_data_start();
        for (c = 0; c < DUCK_WIDTH; c++)
            ssd1306_data_byte(fdata[3 * DUCK_WIDTH + c] | 0x80);
        ssd1306_data_end();

        anim_frame = (anim_frame + 1) % DUCK_FRAMES;
        pre_x0 = x0;

        delay_ms(10);
    }
}
