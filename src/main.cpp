#include <Arduino.h>

int ledPin = 4;    // LED connected to digital pin 9
int brightness = 0;   
int fadeAmount = 5;   


void setup() {
  // nothing happens in setup
}

void loop() {
  int delay_time = random(20, 150);
  analogWrite(ledPin, constrain(brightness + random(-20, 20), 50, 175)); 
  brightness = brightness + fadeAmount;

  if (delay_time%2 == 0) {
    if (brightness <= 0){
      fadeAmount = abs(fadeAmount);
    }
    else if(brightness >= 150){
      fadeAmount = -abs(fadeAmount);
    }
    else{
      fadeAmount = -fadeAmount;
    }  
  }

  delay(delay_time);  // 랜덤한 딜레이
}