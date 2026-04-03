#ifndef SSD1306_H
#define SSD1306_H

#include <stdint.h>

#define SSD1306_ADDR    0x78    // 0x3C << 1
#define OLED_WIDTH      72
#define OLED_HEIGHT     40
#define OLED_PAGES      5
#define OLED_OFFSET_X   28      // (128 - 72) / 2

void ssd1306_init(void);
void ssd1306_cmd(uint8_t cmd);
void ssd1306_set_cursor(uint8_t x, uint8_t page);
void ssd1306_data_start(void);
void ssd1306_data_byte(uint8_t d);
void ssd1306_data_end(void);
void ssd1306_clear(void);
void ssd1306_bitmap(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1, __code const uint8_t *bmp);

#endif
