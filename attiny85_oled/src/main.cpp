#define OLED_64x32

#include "Tiny4kOLED.h"
#include "SystemStatus.h"
#include "map.h"

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
  //display battery indicator
  oled.bitmap(IMAGE_OFF_X, 0, IMAGE_OFF_X+BATTERY_PART1_WIDTH, BATTERY_HEIGHT/8, battery_indicator_part1);
  bitmap_repeat(IMAGE_OFF_X+BATTERY_PART1_WIDTH, 0, BATTERY_HEIGHT/8, BATTERY_PART2_WIDTH, -1, battery_indicator_part2);
  oled.bitmap(IMAGE_OFF_X+BATTERY_PART1_WIDTH+BATTERY_PART2_WIDTH, 0, IMAGE_OFF_X+BATTERY_WIDTH, BATTERY_HEIGHT/8, battery_indicator_part3);
  
  oled.setContrast(20);       // oled brightness 0~255
  oled.setDisplayClock(2, 1); // clock speed down to 1/2 from default
  oled.on();
}


void loop() {
  static uint8_t pre_x0 = 0;
  static uint8_t anim_frame = 0; // animation frame for the blob / metaballs animation
  
  int voltage = sys.getVCC();
  int gauge = map(voltage, 1300, 5000, 0, X1); //1.3V to 5V to operate OLED
  gauge = max(0, min(gauge, X1));

  uint8_t x0 = gauge;
  int8_t diff_x = x0 - pre_x0;

  // pace control of the battery gauge to avoid flickering
  if(diff_x > PACE){
    x0 = x0 - (diff_x - PACE);
    diff_x = PACE;
  }else if(diff_x < -PACE){
    x0 = x0 - (diff_x + PACE);
    diff_x = -PACE;
  }

  if(diff_x > 0)
    bitmap_repeat(XOFF+pre_x0, 0, OLED_HEIGHT/8, diff_x, diff_x, fill);
  else if (diff_x < 0)
    oled.bitmap(XOFF+pre_x0-1, 0, XOFF+pre_x0, OLED_HEIGHT/8, fill_init);

  if (x0 + BLOB_WIDTH < X1){
    oled.bitmap(XOFF+x0+1, BLOB_Y1, XOFF+x0+1+BLOB_WIDTH, BLOB_Y2, epd_bitmap_allArray[anim_frame]);
    oled.bitmap(XOFF+x0+1+BLOB_WIDTH, 0, XOFF+x0+2+BLOB_WIDTH, OLED_HEIGHT/8, battery_indicator_part2); // to remove afterimage
  }else{
    bitmap_roi(XOFF+x0+1, BLOB_Y1, X1+XOFF+1, BLOB_Y2, BLOB_WIDTH, epd_bitmap_allArray[anim_frame]);
  }
  
  anim_frame = (anim_frame + 1) % (sizeof(epd_bitmap_allArray)/sizeof(epd_bitmap_allArray[0]));

  pre_x0 = x0;

  delay(1);
}