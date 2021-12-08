// ** Checks for stalled or locked-up hardware & software lockups – and triggers a reset **

#include <WDTZero.h> // https://github.com/javos65/WDTZero

WDTZero wdt;

void watchdogSetup() {
  wdt.attachShutdown(watchdogShutdown);
  wdt.setup(WDT_SOFTCYCLE16S);
}

void watchdogLoop() {
  wdt.clear();
}

void watchdogShutdown() {
  if (state != RegulatorState::ACCUMULATE) {
    pinMode(TONE_PIN, OUTPUT);
    tone(TONE_PIN, BEEP_2, 400);
  }
  eventsWrite(WATCHDOG_EVENT, 0, 0);
  shutdown();
}
