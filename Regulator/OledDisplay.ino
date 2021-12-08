// ** The oled data screen **

#include "lcdgfx.h" // https://github.com/lexus2k/lcdgfx

// standard X/Y positions: 8, 16, 24, 32, 40, 48, 56, 64, 72, 80, 88, 96, 104, 112, 120, 128

DisplaySSD1327_128x128_I2C display(-1); // SSD1327 Oled 128 x 128

String s;
char str[16];

void displaySetup() {
    display.setFixedFont( ssd1306xled_font6x8 );
    display.begin();
    display.clear();
}

void displayClear() {
display.clear();
}

void displayLoop() {
  display.setColor(GRAY_COLOR4(200)); // 255 is max level
    if (state == RegulatorState::ALARM) {
    display.clear();
    
      switch (alarmCause) {
        case AlarmCause::MQTT:
        display.printFixed(40, 16, "!MQTT", STYLE_BOLD);             
          break;
        case AlarmCause::NETWORK:
        display.printFixed(16, 16, "!NETWORK", STYLE_BOLD);
          break;
          case AlarmCause::TRIAC:
        display.printFixed(32, 16, "!TRIAC", STYLE_BOLD);
          break;
        case AlarmCause::MODBUS:
        display.printFixed(24, 16, "!MODBUS", STYLE_BOLD);
          break;
        default:
          break;
      }
  return;
  } 
  else {     
     // switch off display at night to prevent burn-in
     if (hourNow >= 22 && hourNow <= 23 && !buttonPressed) {
     display.clear();
       return;
    }  
    if (hourNow >= 0 && hourNow < 7 && !buttonPressed) { 
     display.clear();
       return;
    }

// display nightCall status and power
    if (nightCall == true) {
    display.invertColors();
    display.printFixed(0, 6, "POWER:", STYLE_NORMAL); // nightCharge initiate
    display.invertColors();
    }
    if (nightCall == false) {
    display.printFixed(0, 6, "POWER:", STYLE_NORMAL); // nightCharge disabled
    }               
    sprintf(str,"%03d",elsensPower);
    display.setFixedFont( ssd1306xled_font8x16 );
    display.printFixed(48, 0, str, STYLE_BOLD); // ssd1327
    display.setFixedFont( ssd1306xled_font6x8 );
    display.printFixed(74, 8, "w", STYLE_NORMAL);

//  display state
    display.printFixed(0, 20, "STATE:", STYLE_NORMAL);
    if (state == RegulatorState::MONITORING) {
    display.printFixed(48, 20, "monitoring", STYLE_BOLD); 
    }
    if (state == RegulatorState::REGULATING) {
    display.printFixed(48, 20, "regulating", STYLE_NORMAL);
    }
    if (state == RegulatorState::MANUAL_RUN) {
    display.printFixed(48, 20, "manual    ", STYLE_NORMAL); 
    }    
    if (state == RegulatorState::ACCUMULATE) {
    display.printFixed(48, 20, "charging  ", STYLE_NORMAL); 
    }  

// display solar sensor average (W)
  display.printFixed(0, 32, "INsol:", STYLE_NORMAL);
  sprintf(str,"%04d",insolPowerAvg); // insol AV
  display.printFixed(48, 32, str, STYLE_NORMAL);
  
// display inverter AC Power
  display.printFixed(0, 40, "PVpow:", STYLE_NORMAL);
  sprintf(str,"%04d",inverterAC);
  display.printFixed(48, 40, str, STYLE_NORMAL);
// display throttled status
//  if (inverterThrottled == true) {
//  display.printFixed(76, 40, "T", STYLE_NORMAL);
//  }
//  else {
//  display.printFixed(76, 40, " ", STYLE_NORMAL);
//  }
  

// display Meter Power 
  display.printFixed(0, 48, "Meter:", STYLE_NORMAL);
  sprintf(str,"%04d",abs(meterPower));
  display.printFixed(48, 48, str, STYLE_NORMAL);
  if (meterPower < 0) {
  display.printFixed(40, 48, "-", STYLE_NORMAL);
  }
  if (meterPower >= 0) {
  display.printFixed(40, 48, " ", STYLE_NORMAL);
  }

// display throttled status
  if (pilotThrottled == true) {
  display.printFixed(76, 48, "T", STYLE_NORMAL);
  }
  else {
  display.printFixed(76, 48, " ", STYLE_NORMAL);
  }
 
// display forecastIndex > sunny = 1, sunny/cloudy = 2, worsening = 3, cloudy = 4, rainy = 5
  display.printFixed(0, 62, "Sky", STYLE_NORMAL);
  sprintf(str,"%02d",Z);
  display.printFixed(18, 62, str, STYLE_NORMAL);
  display.printFixed(30, 62, ":", STYLE_NORMAL);

  if (forecastTrend == 0) {
    display.printFixed(48, 62, "-//- ", STYLE_NORMAL);
  }
  else {
  if (forecastIndex == 1) {
    display.printFixed(48, 62, "sunny     ", STYLE_NORMAL);
  }
  if (forecastIndex == 2) {
    display.printFixed(48, 62, "partly sun", STYLE_NORMAL); // mixed sun
  }
  if (forecastIndex == 3) {
    display.printFixed(48, 62, "worsening ", STYLE_NORMAL);
  }
  if (forecastIndex == 4) {
    display.printFixed(48, 62, "cloudy    ", STYLE_NORMAL); // overcast
  }
  if (forecastIndex == 5) {
    display.printFixed(48, 62, "rainy     ", STYLE_NORMAL);
  }
  }
  
// display trend and pressure, 1 = raising, 2 = falling, 3 = steady, 0 = no data
  if (forecastTrend == 1) {
    display.printFixed(112, 62, "++", STYLE_NORMAL);
  }
  if (forecastTrend == 2) {
    display.printFixed(112, 62, "--", STYLE_NORMAL);
  }  
  if (forecastTrend == 3) {
    display.printFixed(112, 62, "<>", STYLE_NORMAL);
  }

// display kWh accumulated
  display.printFixed(0, 76, "kWh/d:", STYLE_NORMAL);
  sprintf(str,"%.2f",((float) statsAccumulatedPowerToday()/1000.0)); // kWh on day
  display.printFixed(48, 76, str, STYLE_NORMAL);
  display.printFixed(76, 76, "ACC", STYLE_NORMAL); // charge
  if (nightCall == true) { 
  display.printFixed(96, 76, "/", STYLE_NORMAL);
  sprintf(str,"%.2f",((float) chargeSetLevel/1000.0)); // kWh charge level set
  display.printFixed(104, 76, str, STYLE_NORMAL);
  }

// display kWh manual
  sprintf(str,"%.2f",((float) statsManualPowerToday()/1000.0)); // kWh on day
  display.printFixed(48, 84, str, STYLE_NORMAL);
  display.printFixed(76, 84, "MAN", STYLE_NORMAL); // manual
    
// display kWh regulated
  sprintf(str,"%.2f",((float) statsRegulatedPowerToday()/1000.0)); // kWh on day
  display.printFixed(48, 92, str, STYLE_NORMAL);
  display.printFixed(76, 92, "REG", STYLE_NORMAL); // regulating

  display.setColor(GRAY_COLOR4(20));

// display IP
  display.printFixed(48, 108, "192.168.0.61", STYLE_NORMAL);

// display Vac smart meter
  sprintf(str,"%.3d",voltage); // 
  display.printFixed(0, 120, str, STYLE_NORMAL);
  display.printFixed(20, 120, "V", STYLE_NORMAL);

// display Mem
  sprintf(str,"%.2d", freeMemory()/1000);
  display.printFixed(48, 120, str, STYLE_NORMAL);
  display.printFixed(62, 120, "kB", STYLE_NORMAL);

// display current time 

  sprintf(str,"%02d",hourNow); // hour
  display.printFixed(96, 120, str, STYLE_NORMAL);
  display.printFixed(108, 120, ":", STYLE_NORMAL);
  sprintf(str,"%02d",minuteNow); // minute
  display.printFixed(114, 120, str, STYLE_NORMAL);

// debugging

//  display current sensor
//    display.printFixed(96, 0, "#", STYLE_NORMAL);
//    sprintf(str,"%04d",elsens);
//    display.printFixed(104, 0, str, STYLE_NORMAL);

//  display heatingPower
//    display.printFixed(96, 8, "P", STYLE_NORMAL);
//    sprintf(str,"%04d",heatingPower);
//    display.printFixed(104, 8, str, STYLE_NORMAL);

// display solar sensor (raw) 
  display.printFixed(96, 32, "#", STYLE_NORMAL);
  sprintf(str,"%04d",insol); // insol
  display.printFixed(104, 32, str, STYLE_NORMAL);

// display Throttle threshold
//  sprintf(str,"%.4d",abs(tresholdAvg));
//  display.printFixed(104, 48, str, STYLE_NORMAL);
//   if (tresholdAvg < 0) {
//  display.printFixed(96, 48, "-", STYLE_NORMAL);
//  }
//  if (tresholdAvg >= 0) {
//  display.printFixed(96, 48, " ", STYLE_NORMAL);
//  }
  
  }
}
