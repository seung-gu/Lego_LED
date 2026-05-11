# ST7789 1.47" 12-pin Bare Panel Pinout (CONFIRMED WORKING)

User has a bare 1.47" ST7789 LCD panel (FPC, 12 pins, COG type).
Markings: NFP1478-01A and 147-1732 TBWIG 01H - H12.
No public datasheet exists for this exact variant.

**Why:** Pinout was reverse-engineered through testing. A reference schematic for a similar 1.47" panel was found but did NOT match exactly — pins 5 and 6 are NC (tied together internally) on this panel, shifting all signal pins by one position.

**Reference schematic source:** [Waveshare ESP32-C6-LCD-1.47 schematics](https://files.waveshare.com/wiki/ESP32-C6-LCD-1.47/ESP32-C6-LCD-1.47_schemetics.pdf)

## Confirmed working pinout

| Pin | Function |
|-----|----------|
| 1 | GND |
| 2 | GND/LEDK |
| 3 | LEDA |
| 4 | VDD (3.3V) |
| 5 | NC (tied to pin 6) |
| 6 | NC (tied to pin 5) |
| 7 | DC |
| 8 | CS |
| 9 | SCL (SCK) |
| 10 | SDA (MOSI) |
| 11 | RST |
| 12 | NC (or GND) |

## Arduino Uno wiring (verified)

| LCD pin | Arduino |
|---------|---------|
| 1 | GND |
| 2 | GND |
| 3 | 100Ω → 3.3V |
| 4 | 3.3V (VDD) |
| 5, 6 | leave unconnected |
| 7 | D8 (DC) |
| 8 | D10 (CS) |
| 9 | D13 (SCK) |
| 10 | D11 (MOSI) |
| 11 | D9 (RST) |
| 12 | leave unconnected |

## Reverse-engineering clues that mattered

- Pin 3 = LEDA: confirmed by backlight test (3.3V via 100Ω → pin 3, GND → pin 2 lights backlight)
- Pin 5-6 internally tied: continuity beep only between 5-6, not other pairs (and same on a different working LCD of same model). Diagnosed as a NC dummy pair, NOT a K-A LED pair (LED would be diode, not near-short)
- This shifted the standard signal pin order by one position vs the reference schematic

## Code-side notes

- Adafruit_ST7789 library: `tft.init(172, 320, SPI_MODE3)` and `tft.setSPISpeed(1000000)` — MODE3, slower SPI was used to get it working
- TFT_CS=10, TFT_DC=8, TFT_RST=9 in the sketch
- For 172x320, library needs column offset 34 (not auto-applied) — `setColRowStart(34, 0)` is protected, needs subclass to expose, or shift coordinates manually
- Hardware SPI: D11 = MOSI, D13 = SCK (Arduino Uno defaults)

## Hardware notes

- 5V Arduino signals connected directly to 3.3V chip (no level shifter — accepted risk, works for testing)
- 100Ω current limit on LEDA (datasheet typ: Vf=2.8-3.2V, If=40mA)
