// ** The ledbar with alarm status and ElSens power consumption **

#include <Grove_LED_Bar.h>

Grove_LED_Bar ledBar(LEDBAR_CLOCK_PIN, LEDBAR_DATA_PIN, true);

void ledBarSetup() {
  ledBar.begin();
  
  // indicate startup
  ledBar.setLed(5, 1);
  ledBar.setLed(6, 1);
}

void ledBarLoop() {

  const byte ALARM_LED = 10; //red
  const byte BLINK_LED = 9;  // yellow
  const byte BYPASS_LED = 8; //green
  const byte MAX_LEVEL_LED = 7; // green

  const int REFRESH_INTERVAL = 1000;
  static unsigned long previousMillis = 0;
  static boolean blinkLedState = false;

  if (loopStartMillis - previousMillis < REFRESH_INTERVAL)
    return;
  previousMillis = loopStartMillis;
  blinkLedState = !blinkLedState;
  float level = 0;
  switch (state) {
    case RegulatorState::ALARM:
      switch (alarmCause) {
        case AlarmCause::MQTT:
          level = 1;
          break;
        case AlarmCause::NETWORK:
          level = 2;
          break;
        case AlarmCause::MODBUS:
          level = 3;
          break;
        default:
          break;
      }
      break;
    case RegulatorState::REGULATING:
        level = MAX_LEVEL_LED - MAX_LEVEL_LED * ((float) (MAX_POWER - elsensPower) / MAX_POWER); // setPower
      break;
    case RegulatorState::MANUAL_RUN:
        level = MAX_LEVEL_LED - MAX_LEVEL_LED * ((float) (MAX_POWER - elsensPower) / MAX_POWER); // setPower
      break;  
    default:
      break;
  }
  ledBar.setLevel(level);
  if (bypassRelayOn) {
    ledBar.setLed(BYPASS_LED, state == RegulatorState::ACCUMULATE && blinkLedState ? 0.5 : 1);
  }

if (state == RegulatorState::ACCUMULATE) {
    ledBar.setLed(BYPASS_LED, 0.5);
  }  

  if (state == RegulatorState::ALARM) {
    ledBar.setLed(ALARM_LED, 1);
  } 

  if (blinkLedState) {
    ledBar.setLed(BLINK_LED, 0.75);
  } else {
    ledBar.setLed(BLINK_LED, 0.25);
  }
}
