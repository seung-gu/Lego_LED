#include "ssd1306.h"
#include "i2c.h"
#include <avr/pgmspace.h>

void ssd1306_cmd(uint8_t cmd) {
    i2c_start();
    i2c_write(SSD1306_ADDR);
    i2c_write(0x00);
    i2c_write(cmd);
    i2c_stop();
}

static void ssd1306_cmd2(uint8_t cmd, uint8_t arg) {
    i2c_start();
    i2c_write(SSD1306_ADDR);
    i2c_write(0x00);
    i2c_write(cmd);
    i2c_write(arg);
    i2c_stop();
}

void ssd1306_set_cursor(uint8_t x, uint8_t page) {
    uint8_t col = x + OLED_OFFSET_X;
    i2c_start();
    i2c_write(SSD1306_ADDR);
    i2c_write(0x00);
    i2c_write(0xB0 | (page & 0x07));
    i2c_write(0x10 | ((col >> 4) & 0x0F));
    i2c_write(col & 0x0F);
    i2c_stop();
}

void ssd1306_data_start(void) {
    i2c_start();
    i2c_write(SSD1306_ADDR);
    i2c_write(0x40);
}

void ssd1306_data_byte(uint8_t d) {
    i2c_write(d);
}

void ssd1306_data_end(void) {
    i2c_stop();
}

void ssd1306_clear(void) {
    uint8_t page, col;
    for (page = 0; page < 8; page++) {
        ssd1306_set_cursor(0, page);
        ssd1306_data_start();
        for (col = 0; col < OLED_WIDTH; col++)
            ssd1306_data_byte(0x00);
        ssd1306_data_end();
    }
}

void ssd1306_bitmap(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1, const uint8_t *bmp) {
    uint16_t j = 0;
    uint8_t y, x;
    for (y = y0; y < y1; y++) {
        ssd1306_set_cursor(x0, y);
        ssd1306_data_start();
        for (x = x0; x < x1; x++)
            ssd1306_data_byte(pgm_read_byte(&bmp[j++]));
        ssd1306_data_end();
    }
}

void ssd1306_init(void) {
    ssd1306_cmd(0xAE);
    ssd1306_cmd(0xC0);             // COM scan normal (0xC8=remapped, 0xC0=normal)
    ssd1306_cmd(0xA1);
    ssd1306_cmd2(0xA8, 0x27);
    ssd1306_cmd2(0x8D, 0x14);
    ssd1306_cmd2(0xD3, 0x08);
    ssd1306_cmd2(0xAD, 0x10);
    ssd1306_cmd2(0x81, 0x01);
    ssd1306_cmd2(0xD5, 0x11);
    ssd1306_cmd(0x40);
    ssd1306_cmd2(0x20, 0x02);
    ssd1306_cmd2(0xDA, 0x12);
    ssd1306_cmd(0xA4);
    ssd1306_cmd(0xA6);
    ssd1306_clear();
    ssd1306_cmd(0xAF);
}
