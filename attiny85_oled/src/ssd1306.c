#include "ssd1306.h"
#include "i2c.h"

void ssd1306_cmd(uint8_t cmd) {
    i2c_start();
    i2c_write(SSD1306_ADDR);
    i2c_write(0x00);    // Co=0, D/C#=0 → command
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
    i2c_write(0x40);    // Co=0, D/C#=1 → data
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
        for (col = 0; col < OLED_WIDTH; col++) {
            ssd1306_data_byte(0x00);
        }
        ssd1306_data_end();
    }
}

void ssd1306_bitmap(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1, __code const uint8_t *bmp) {
    uint16_t j = 0;
    uint8_t y, x;
    for (y = y0; y < y1; y++) {
        ssd1306_set_cursor(x0, y);
        ssd1306_data_start();
        for (x = x0; x < x1; x++) {
            ssd1306_data_byte(bmp[j++]);
        }
        ssd1306_data_end();
    }
}

void ssd1306_init(void) {
    ssd1306_cmd(0xAE);             // display off

    // --- from Tiny4kOLED init_72x40r ---
    ssd1306_cmd(0xC8);             // COM output scan direction (remapped)
    ssd1306_cmd(0xA1);             // segment remap
    ssd1306_cmd2(0xA8, 0x27);      // multiplex ratio: 40-1
    ssd1306_cmd2(0x8D, 0x14);      // charge pump enable

    // --- from main.cpp setup() ---
    ssd1306_cmd2(0xD3, 0x00);      // display offset: 0
    ssd1306_cmd2(0xAD, 0x10);      // internal IREF 19uA (low power)
    ssd1306_cmd2(0x81, 0x05);      // contrast: 5
    ssd1306_cmd2(0xD5, 0x11);      // display clock: divRatio=2, oscFreq=1 (~140kHz)

    // --- defaults ---
    ssd1306_cmd(0x40);             // start line: 0
    ssd1306_cmd2(0x20, 0x02);      // page addressing mode
    ssd1306_cmd2(0xDA, 0x12);      // COM pins config
    ssd1306_cmd2(0xD9, 0xF1);      // pre-charge period
    ssd1306_cmd2(0xDB, 0x40);      // VCOMH deselect level
    ssd1306_cmd(0xA4);             // display follows RAM
    ssd1306_cmd(0xA6);             // normal display (not inverted)

    ssd1306_clear();
    ssd1306_cmd(0xAF);             // display on
}

// minimal 4x8 hex font (0-F)
__code const uint8_t hex_font[][4] = {
    {0x7E,0x42,0x42,0x7E}, // 0
    {0x00,0x44,0x7E,0x40}, // 1
    {0x62,0x52,0x4A,0x46}, // 2
    {0x42,0x4A,0x4A,0x7E}, // 3
    {0x0E,0x08,0x08,0x7E}, // 4
    {0x4E,0x4A,0x4A,0x7A}, // 5
    {0x7E,0x4A,0x4A,0x7A}, // 6
    {0x02,0x02,0x02,0x7E}, // 7
    {0x7E,0x4A,0x4A,0x7E}, // 8
    {0x4E,0x4A,0x4A,0x7E}, // 9
    {0x7E,0x0A,0x0A,0x7E}, // A
    {0x7E,0x48,0x48,0x78}, // B (b)
    {0x7E,0x42,0x42,0x42}, // C
    {0x78,0x48,0x48,0x7E}, // D (d)
    {0x7E,0x4A,0x4A,0x42}, // E
    {0x7E,0x0A,0x0A,0x02}, // F
};

void ssd1306_print_hex(uint8_t x, uint8_t page, uint8_t val) {
    uint8_t i, nibble;
    // high nibble
    nibble = (val >> 4) & 0x0F;
    ssd1306_set_cursor(x, page);
    ssd1306_data_start();
    for (i = 0; i < 4; i++) ssd1306_data_byte(hex_font[nibble][i]);
    ssd1306_data_byte(0x00); // gap
    ssd1306_data_end();
    // low nibble
    nibble = val & 0x0F;
    ssd1306_set_cursor(x + 5, page);
    ssd1306_data_start();
    for (i = 0; i < 4; i++) ssd1306_data_byte(hex_font[nibble][i]);
    ssd1306_data_byte(0x00);
    ssd1306_data_end();
}
