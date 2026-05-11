#ifndef ST7735_H
#define ST7735_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define LCD_WIDTH   96
#define LCD_HEIGHT  54

// RGB565 colors
#define ST7735_BLACK    0x0000
#define ST7735_WHITE    0xFFFF
#define ST7735_RED      0xF800
#define ST7735_GREEN    0x07E0
#define ST7735_BLUE     0x001F
#define ST7735_CYAN     0x07FF
#define ST7735_MAGENTA  0xF81F
#define ST7735_YELLOW   0xFFE0

// Configures GPIO directions, alternate functions, and UCA0 SPI.
// Call once at startup before st7735_init().
void st7735_hw_init(void);

// Sends ST7735 init command sequence.
void st7735_init(void);

// Backlight control (P1.1 LED-A)
void st7735_backlight(uint8_t on);

// Fill entire screen with a 16-bit color.
void st7735_fill_screen(uint16_t color);

// Draw a single pixel.
void st7735_draw_pixel(uint8_t x, uint8_t y, uint16_t color);

// Fill a rectangle.
void st7735_fill_rect(uint8_t x, uint8_t y, uint8_t w, uint8_t h, uint16_t color);

#ifdef __cplusplus
}
#endif

#endif
