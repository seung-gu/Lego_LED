#include <msp430.h>
#include "st7735.h"

/* ---------- Pin macros ---------- */
#define CS_LOW()    (P1OUT &= ~BIT7)
#define CS_HIGH()   (P1OUT |=  BIT7)
#define DC_CMD()    (P2OUT &= ~BIT3)
#define DC_DATA()   (P2OUT |=  BIT3)
#define RST_LOW()   (P3OUT &= ~BIT2)
#define RST_HIGH()  (P3OUT |=  BIT2)

/* ---------- ST7735 commands ---------- */
#define ST_SWRESET  0x01
#define ST_SLPOUT   0x11
#define ST_NORON    0x13
#define ST_INVOFF   0x20
#define ST_INVON    0x21
#define ST_DISPON   0x29
#define ST_CASET    0x2A
#define ST_RASET    0x2B
#define ST_RAMWR    0x2C
#define ST_MADCTL   0x36
#define ST_COLMOD   0x3A

/* ---------- Panel offsets (TUNE for your specific 96x54 panel) ----------
 * Common 96x54 IPS panels need X_OFFSET around 2 and Y_OFFSET around 1.
 * If you see misaligned pixels or a colored stripe, adjust these.
 */
#define X_OFFSET    2
#define Y_OFFSET    1

/* ---------- Small delay (assumes ~16 MHz Energia default clock) ---------- */
static void delay_ms_local(uint16_t ms)
{
    while (ms--) {
        __delay_cycles(16000);
    }
}

/* ---------- SPI low-level ---------- */
static inline void spi_send(uint8_t b)
{
    while (!(UCA0IFG & UCTXIFG));   // wait TX buffer ready
    UCA0TXBUF = b;
}

static inline void spi_wait_done(void)
{
    while (UCA0STATW & UCBUSY);     // wait for shift register idle
}

static void send_cmd(uint8_t cmd)
{
    spi_wait_done();
    DC_CMD();
    CS_LOW();
    spi_send(cmd);
    spi_wait_done();
    CS_HIGH();
}

static void send_data(uint8_t d)
{
    spi_wait_done();
    DC_DATA();
    CS_LOW();
    spi_send(d);
    spi_wait_done();
    CS_HIGH();
}

/* ---------- Public init ---------- */
void st7735_hw_init(void)
{
    /* GPIO directions ------------------------------------------------ */
    // P1.1 LED-A backlight (start off)
    P1DIR |=  BIT1;
    P1OUT &= ~BIT1;

    // P1.7 CS, deasserted high
    P1DIR |=  BIT7;
    P1OUT |=  BIT7;

    // P2.3 DC, default data mode
    P2DIR |=  BIT3;
    P2OUT |=  BIT3;

    // P3.2 RESET, high (not in reset)
    P3DIR |=  BIT2;
    P3OUT |=  BIT2;

    /* Alternate function: P1.4 = UCA0SIMO, P1.6 = UCA0CLK ------------ */
    P1SEL0 |=  (BIT4 | BIT6);
    P1SEL1 &= ~(BIT4 | BIT6);

    /* MSP430FR2433 needs LOCKLPM5 cleared after GPIO config ---------- */
    PM5CTL0 &= ~LOCKLPM5;

    /* SPI on UCA0 ---------------------------------------------------- */
    UCA0CTLW0  =  UCSWRST;                  // hold reset
    UCA0CTLW0 |=  UCSSEL__SMCLK             // SMCLK source
              |   UCMST                     // master
              |   UCSYNC                    // synchronous (SPI)
              |   UCMSB                     // MSB first
              |   UCCKPH;                   // SPI mode 0 (CPOL=0, CPHA=0)
    // UCMODE bits left 0 → 3-pin SPI (CS handled manually)
    UCA0BRW = 2;                            // SMCLK / 2 → ~8 MHz @ 16 MHz SMCLK
    UCA0CTLW0 &= ~UCSWRST;                  // release reset
}

void st7735_backlight(uint8_t on)
{
    if (on) P1OUT |= BIT1;
    else    P1OUT &= ~BIT1;
}

void st7735_init(void)
{
    /* Hardware reset pulse */
    RST_HIGH();  delay_ms_local(10);
    RST_LOW();   delay_ms_local(10);
    RST_HIGH();  delay_ms_local(150);

    /* Software reset */
    send_cmd(ST_SWRESET);  delay_ms_local(150);

    /* Sleep out */
    send_cmd(ST_SLPOUT);   delay_ms_local(120);

    /* 16-bit color (RGB565) */
    send_cmd(ST_COLMOD);
    send_data(0x05);

    /* Memory access control — orientation. Try 0x00, 0x60, 0xC0, 0xA0. */
    send_cmd(ST_MADCTL);
    send_data(0x00);

    /* IPS panels usually need inversion ON */
    send_cmd(ST_INVON);

    /* Normal display on + DISPON */
    send_cmd(ST_NORON);    delay_ms_local(10);
    send_cmd(ST_DISPON);   delay_ms_local(100);

    /* Turn backlight on */
    st7735_backlight(1);
}

/* ---------- Address window ---------- */
static void set_window(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1)
{
    send_cmd(ST_CASET);
    send_data(0); send_data(x0 + X_OFFSET);
    send_data(0); send_data(x1 + X_OFFSET);

    send_cmd(ST_RASET);
    send_data(0); send_data(y0 + Y_OFFSET);
    send_data(0); send_data(y1 + Y_OFFSET);

    send_cmd(ST_RAMWR);
}

/* ---------- Public drawing API ---------- */
void st7735_fill_screen(uint16_t color)
{
    st7735_fill_rect(0, 0, LCD_WIDTH, LCD_HEIGHT, color);
}

void st7735_fill_rect(uint8_t x, uint8_t y, uint8_t w, uint8_t h, uint16_t color)
{
    uint8_t hi, lo;
    uint16_t total, i;

    if (x >= LCD_WIDTH || y >= LCD_HEIGHT) return;
    if (x + w > LCD_WIDTH)  w = LCD_WIDTH  - x;
    if (y + h > LCD_HEIGHT) h = LCD_HEIGHT - y;

    set_window(x, y, x + w - 1, y + h - 1);

    hi = (uint8_t)(color >> 8);
    lo = (uint8_t)(color & 0xFF);

    spi_wait_done();
    DC_DATA();
    CS_LOW();

    total = (uint16_t)w * (uint16_t)h;
    for (i = 0; i < total; i++) {
        while (!(UCA0IFG & UCTXIFG));
        UCA0TXBUF = hi;
        while (!(UCA0IFG & UCTXIFG));
        UCA0TXBUF = lo;
    }
    spi_wait_done();
    CS_HIGH();
}

void st7735_draw_pixel(uint8_t x, uint8_t y, uint16_t color)
{
    if (x >= LCD_WIDTH || y >= LCD_HEIGHT) return;
    set_window(x, y, x, y);

    spi_wait_done();
    DC_DATA();
    CS_LOW();
    spi_send((uint8_t)(color >> 8));
    spi_send((uint8_t)(color & 0xFF));
    spi_wait_done();
    CS_HIGH();
}
