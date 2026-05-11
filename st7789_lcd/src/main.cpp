#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <SPI.h>

#define TFT_CS   10
#define TFT_DC    8
#define TFT_RST   9

Adafruit_ST7789 tft = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_RST);

void setup() {
  Serial.begin(9600);
  Serial.println(F("ST7789 init start"));

  tft.init(172, 320, SPI_MODE3);
  tft.setSPISpeed(1000000);
  tft.setRotation(0);

  Serial.println(F("init done"));
}

void loop() {
  tft.fillScreen(ST77XX_RED);
  delay(1000);
  tft.fillScreen(ST77XX_GREEN);
  delay(1000);
  tft.fillScreen(ST77XX_BLUE);
  delay(1000);
}
