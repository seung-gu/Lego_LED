#include "Tiny4kOLED.h"
#include "SystemStatus.h"
#include "map.h"
SystemStatus sys;


const uint8_t numFrames = 10;
uint8_t listVoltage[numFrames] = {0};
uint8_t batteryperc = 0;

const uint8_t XOFF = 3;
const uint8_t X0 = 0;
const uint8_t X1 = 52;
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
	oled.begin(64, 48, sizeof(tiny4koled_init_64x48br), tiny4koled_init_64x48br);

	oled.off();
	oled.bitmap(0, 0, 3, 6, battery_indicator_part1);
	bitmap_repeat(3, 0, 6, 53, -1, battery_indicator_part2);
	oled.bitmap(56, 0, 64, 6, battery_indicator_part3);

	oled.on();
}


void loop() {
	static uint8_t pre_x0 = 0;
	static int count = 0;
	static uint8_t anim_frame = 0; // animation frame for the blob / metaballs animation
	
	listVoltage[(count/10) % numFrames] = map(sys.getVCC(), 1300, 4000, 0, 100);

	int sum = 0;
	for (int i = 0; i < numFrames; i++){
		sum += listVoltage[i];
	}
	batteryperc = sum / numFrames;
	
	int gauge = map(batteryperc, 0, 100, 0, 53);
	gauge = max(0, min(gauge, 52));

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
		bitmap_repeat(XOFF+pre_x0, 0, 6, diff_x, diff_x, fill);
	else if (diff_x < 0)
		bitmap_repeat(XOFF+x0, 0, 6, -diff_x, 0, battery_indicator_part2); // TODO: remove this part later to avoid flickering

	if (x0 + 32 < X1){
		oled.bitmap(XOFF+x0+1, 1, XOFF+x0+1+32, 5, epd_bitmap_allArray[anim_frame]);
		oled.bitmap(XOFF+x0+1+32, 0, XOFF+x0+2+32, 6, battery_indicator_part2); // to remove afterimage
	}else{
		bitmap_roi(XOFF+x0+1, 1, X1+XOFF+1, 5, 32, epd_bitmap_allArray[anim_frame]);
	}
  
	anim_frame = (anim_frame + 1) % (sizeof(epd_bitmap_allArray)/sizeof(epd_bitmap_allArray[0]));

	pre_x0 = x0;

	count++;
	if (count == 10000){ // to avoid overflow
		count = 0;
	}

	delay(5);
}