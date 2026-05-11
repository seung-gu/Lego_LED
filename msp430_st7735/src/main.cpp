/*
 * MSP430FR2433 + ST7735 96x54 IPS LCD
 *
 * Pin mapping:
 *   P1.4 → MOSI  (UCA0SIMO, hardware SPI)
 *   P1.6 → SCLK  (UCA0CLK,  hardware SPI)
 *   P1.7 → CS    (GPIO)
 *   P3.2 → RESET (GPIO)
 *   P2.3 → DC    (GPIO)
 *   P1.1 → LED-A backlight enable
 */

#include <Energia.h>
#include "st7735.h"

void setup(void)
{
    /* Energia handles WDT and clock (16 MHz default).
     * We configure GPIO/SPI registers directly inside the driver. */
    st7735_hw_init();
    st7735_init();
}

void loop(void)
{
    /* Color cycle demo */
    st7735_fill_screen(ST7735_RED);
    delay(500);
    st7735_fill_screen(ST7735_GREEN);
    delay(500);
    st7735_fill_screen(ST7735_BLUE);
    delay(500);
    st7735_fill_screen(ST7735_WHITE);
    delay(500);

    /* Rectangle demo */
    st7735_fill_screen(ST7735_BLACK);
    st7735_fill_rect( 8,  8, 20, 20, ST7735_RED);
    st7735_fill_rect(38,  8, 20, 20, ST7735_GREEN);
    st7735_fill_rect(68,  8, 20, 20, ST7735_BLUE);
    st7735_fill_rect( 8, 32, 80, 14, ST7735_YELLOW);
    delay(1500);
}
