#define OLED_72x40

#include "Tiny4kOLED.h"
#include "SystemStatus.h"
#include "map.h"
#include "duck.h"

SystemStatus sys;

const uint8_t PACE = 1;

// display only x0~x1 area of fixed sized bitmap. width must be the same as bitmap width
void bitmap_roi(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1, uint8_t width, const uint8_t bitmap[]) 
{
  for (uint8_t y = 0; y < y1-y0; y++) {
    oled.setCursor(x0,y+y0);
    oled.startData();
    uint16_t j = 0;
    for (uint8_t x = x0; x < x1; x++) {
      oled.sendData(pgm_read_byte(&bitmap[y*width+j++]));
    }
    oled.endData();
  }
  oled.setCursor(0, 0);
}

// display repeated times of the bitmap
void bitmap_repeat(uint8_t x_off, uint8_t y0, uint8_t y1, uint8_t repeat, uint8_t edge, const uint8_t bitmap[]) {
  for (uint8_t r = 0; r <= repeat; r++){
    uint8_t x0 = x_off + r;
    for (uint8_t y = y0; y < y1; y++) {
      oled.setCursor(x0,y);
      oled.startData();
      if (x0 == XOFF || r == edge) // at edge points, to make surface rounded
        oled.sendData(pgm_read_byte(&fill_init[y]));
      else
        oled.sendData(pgm_read_byte(&bitmap[y]));
      oled.endData();
    }
  }
  oled.setCursor(0, 0);
}


void setup() {
  oled.begin(OLED_WIDTH, OLED_HEIGHT, sizeof(INIT(OLED_SIZE)), INIT(OLED_SIZE));
  oled.off();
  oled.clear();
  // ground line at bottom of display (page 4, row 39)
  oled.setCursor(0, 4);
  oled.startData();
  for (uint8_t c = 0; c < OLED_WIDTH; c++)
    oled.sendData(0x80);
  oled.endData();
  
  oled.setDisplayOffset(8); // fix COM offset for 72x40 panel
  oled.setInternalIref(false); // internal IREF 19uA (low power)
  oled.setContrast(5);       // oled brightness 0~255
  oled.setDisplayClock(2, 1); // clock speed down to 1/2 from default
  oled.on();
}


void loop() {
  static uint8_t pre_x0 = 0;
  static uint8_t anim_frame = 0; // animation frame for the blob / metaballs animation
  
  int voltage = sys.getVCC();
  int gauge = map(voltage, 1300, 5000, 0, OLED_WIDTH - BLOB_WIDTH);
  gauge = max(0, min(gauge, OLED_WIDTH - BLOB_WIDTH));

  uint8_t x0 = gauge;
  int8_t diff_x = x0 - pre_x0;

  // pace control
  if(diff_x > PACE){
    x0 = x0 - (diff_x - PACE);
    diff_x = PACE;
  }else if(diff_x < -PACE){
    x0 = x0 - (diff_x + PACE);
    diff_x = -PACE;
  }

  // erase only the trailing column(s) that the figure left behind
  if (diff_x > 0) {
    for (uint8_t pg = 1; pg < 4; pg++) {
      oled.setCursor(pre_x0, pg);
      oled.startData();
      for (uint8_t c = 0; c < (uint8_t)diff_x; c++) oled.sendData(0x00);
      oled.endData();
    }
    oled.setCursor(pre_x0, 4);
    oled.startData();
    for (uint8_t c = 0; c < (uint8_t)diff_x; c++) oled.sendData(0x80); // restore ground line
    oled.endData();
  } else if (diff_x < 0) {
    uint8_t trail = pre_x0 + BLOB_WIDTH + diff_x;
    for (uint8_t pg = 1; pg < 4; pg++) {
      oled.setCursor(trail, pg);
      oled.startData();
      for (uint8_t c = 0; c < (uint8_t)(-diff_x); c++) oled.sendData(0x00);
      oled.endData();
    }
    oled.setCursor(trail, 4);
    oled.startData();
    for (uint8_t c = 0; c < (uint8_t)(-diff_x); c++) oled.sendData(0x80);
    oled.endData();
  }

  // draw figure on pages 1-3 (figure data pages 0-2)
  oled.bitmap(x0, 1, x0 + BLOB_WIDTH, 4, epd_bitmap_duckArray[anim_frame]);

  // draw page 4 (figure data page 3) with ground line OR'd in (bit 7 = row 39)
  const unsigned char* fdata = epd_bitmap_duckArray[anim_frame];
  oled.setCursor(x0, 4);
  oled.startData();
  for (uint8_t c = 0; c < BLOB_WIDTH; c++)
    oled.sendData(pgm_read_byte(&fdata[3 * BLOB_WIDTH + c]) | 0x80);
  oled.endData();

  anim_frame = (anim_frame + 1) % (sizeof(epd_bitmap_duckArray)/sizeof(epd_bitmap_duckArray[0]));

  pre_x0 = x0;

  delay(10);
}