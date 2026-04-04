#include "stc8g.h"
#include "i2c.h"
#include "ssd1306.h"
#include "duck.h"

#define OLED_W   72
#define PACE     1

// delay used at two speeds:
// - before clock switch: 11MHz (i=2400 ≈ 1ms)
// - after clock switch: 32KHz (i=2400 ≈ 344ms)
// use delay_ms_fast() before switch, delay_loops() after
static void delay_ms(uint16_t ms) {
    uint16_t i;
    while (ms--) {
        i = 2400;
        while (i--);
    }
}

// ~1ms at 32KHz (just a few loop iterations)
static void delay_32k(uint16_t ms) {
    uint16_t i;
    while (ms--) {
        i = 3;
        while (i--);
    }
}

void main(void) {
    uint8_t pre_x0 = 0;
    uint8_t anim_frame = 0;
    uint8_t c, pg;
    uint8_t x0;
    int8_t diff_x;
    __code const uint8_t *fdata;

    WDT_CONTR = 0x00;

    // OLED init at 11MHz (fast)
    i2c_init();
    ssd1306_init();

    // ground line at page 4
    ssd1306_set_cursor(0, 4);
    ssd1306_data_start();
    for (c = 0; c < OLED_W; c++)
        ssd1306_data_byte(0x80);
    ssd1306_data_end();

    // no clock switch — stay at 11MHz, use STOP mode for power saving

    while (1) {
        x0 = 0;
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

        ssd1306_bitmap(x0, 1, x0 + DUCK_WIDTH, 4, duck_frames[anim_frame]);

        fdata = duck_frames[anim_frame];
        ssd1306_set_cursor(x0, 4);
        ssd1306_data_start();
        for (c = 0; c < DUCK_WIDTH; c++)
            ssd1306_data_byte(fdata[3 * DUCK_WIDTH + c] | 0x80);
        ssd1306_data_end();

        anim_frame = (anim_frame + 1) % DUCK_FRAMES;
        pre_x0 = x0;

        // STOP mode sleep ~100ms, MCU ~0.4µA during stop
        // wake-up timer uses 32KHz IRC (still running in stop)
        // count = time_ms * 32.768 / 16 ≈ time_ms * 2
        WKTCL = 0x90;           // low byte (400 & 0xFF = 0x90)
        WKTCH = 0x81;           // bit7=enable, bit0=1 (400 >> 8 = 1) → ~200ms
        PCON |= 0x02;           // enter power-down
        __asm nop __endasm;
        __asm nop __endasm;
    }
}
