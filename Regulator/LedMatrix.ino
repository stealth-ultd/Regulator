// ** The 4 segment LedMatrix display **

// https://github.com/adafruit/Adafruit_LED_Backpack/blob/master/examples/clock_sevenseg_ds1307/clock_sevenseg_ds1307.ino

#include <Adafruit_GFX.h>
#include "Adafruit_LEDBackpack.h"

#define DISPLAY_ADDRESS   0x70 // default address of 0x70

Adafruit_7segment ledMatrix = Adafruit_7segment(); // global variables that can be accessed from both the setup and loop function

void ledMatrixSetup() { 
  ledMatrix.begin(DISPLAY_ADDRESS);
  ledMatrix.setBrightness(0); // Brightness: 0 (min) to 15 (max)
}

void ledMatrixLoop() {

     // switch off display at night to prevent burn-in
     if (hourNow >= 22 && hourNow <= 23 && !buttonPressed) {
      ledMatrix.clear();
      ledMatrix.writeDisplay();    
       return;
    }  
    if (hourNow >= 0 && hourNow < 7 && !buttonPressed) { 
     ledMatrix.clear();
     ledMatrix.writeDisplay();
       return;
    }
    
int displayValue = inverterAC; // display inverter AC Power
  ledMatrix.print(displayValue, DEC);
  ledMatrix.writeDisplay();
   
  // Add zero padding 

    if (inverterAC < 1000) {
      ledMatrix.writeDigitNum(0, 0);  
    }
    if (inverterAC < 100) {
      ledMatrix.writeDigitNum(1, 0);
    }
    if (inverterAC < 10) {
      ledMatrix.writeDigitNum(3, 0);
    }
  // Now push out to the display the new values that were set above.
  ledMatrix.writeDisplay();
  
}
