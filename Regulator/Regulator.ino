#include <MemoryFree.h>
#include <StreamLib.h>
#include <TimeLib.h>
#ifdef ARDUINO_AVR_UNO_WIFI_DEV_ED
#include <WiFiLink.h>
#include <UnoWiFiDevEdSerial1.h>
#else
#include <Ethernet2.h>
#include <SD.h>
#endif
#include "consts.h"

#ifndef __IN_ECLIPSE__
#define BLYNK_NO_BUILTIN
#define BLYNK_NO_INFO
#ifdef ethernet_h
#include <BlynkSimpleEthernet2.h>
#else
#include <BlynkSimpleWiFiLink.h>
#endif
#endif

#ifdef ethernet_h
#define NetServer EthernetServer
#define NetClient EthernetClient
#else
#define NetServer WiFiServer
#define NetClient WiFiClient
#endif

unsigned long loopStartMillis;
byte hourNow; // current hour

RegulatorState state = RegulatorState::MONITORING;
AlarmCause alarmCause = AlarmCause::NOT_IN_ALARM;

boolean buttonPressed = false;
boolean manualRunRequest = false;

boolean mainRelayOn = false;
boolean bypassRelayOn = false;
boolean valvesRelayOn = false;
boolean balboaRelayOn = false;

// SunSpec Modbus values
int b; // battery charging power
int soc; //state of charge
int m; // smart meter measured power
int inverterAC; // inverter AC power

// PowerPilot values
int availablePower;
int heatingPower;
int pwm;

//ElSens values (for logging and UI)
int elsens; // el.sensor measurement
int elsensPower; // power calculation

//Wemo Insight measurement
int wemoPower;

char msgBuff[32];
CStringBuilder msg(msgBuff, sizeof(msgBuff));

boolean sdCardAvailable = false;

void setup() {
  beep();
  Serial.begin(115200);
  Serial.println(version);
  Serial.print(F("mem "));
  Serial.println(freeMemory());

#ifdef ethernet_h
  IPAddress ip(192, 168, 1, 8);
  Ethernet.begin(mac, ip);
#else
  Serial1.begin(115200);
  Serial1.resetESP();
  delay(3000); //ESP init
  WiFi.init(&Serial1);
  // connection is checked in loop
#endif

#ifdef __SD_H__
  if (SD.begin(SD_SS_PIN)) {
    sdCardAvailable = true;
    Serial.println(F("SD card initialized"));
  }
#endif

  pinMode(MAIN_RELAY_PIN, OUTPUT);
  pinMode(BYPASS_RELAY_PIN, OUTPUT);

  buttonSetup();
  ledBarSetup();
  valvesBackSetup();
  balboaSetup();

  telnetSetup();
//  blynkSetup();
  webServerSetup();
  modbusSetup();
  eventsSetup();
  csvLogSetup();
  watchdogSetup();
  beep();
}

void loop() {

  loopStartMillis = millis();
  hourNow = hour(now());

  handleSuspendAndOff();

  watchdogLoop();
  eventsLoop();

  // user interface
  buttonPressed = false;
  buttonLoop();
  beeperLoop(); // alarm sound
  ledBarLoop();
//  blynkLoop();
  webServerLoop();
  telnetLoop(msg.length() != 0); // checks input commands and prints msg
  msg.reset();  //clear msg

  if (handleAlarm())
    return;

  manualRunLoop();

  valvesBackLoop();

  if (restHours())
    return;

#ifndef ethernet_h
  if (!wifiConnected())
    return;
#endif

  susCalibLoop();

  if (!modbusLoop()) // returns true if data set is ready
    return;

  pilotLoop();
  elsensLoop();
  wemoLoop();
  
  battSettLoop();
  balboaLoop();
  
  telnetLoop(true); // logs modbus and heating data
  csvLogLoop();
}

void handleSuspendAndOff() {

  static unsigned long lastOn = 0; // millis for the cool-down timer

  if (state != RegulatorState::REGULATING && state != RegulatorState::MANUAL_RUN) {
    heatingPower = 0;
  }
  if (heatingPower > 0) {
    lastOn = loopStartMillis;
  } else if (mainRelayOn) { // && heatingPower == 0
    analogWrite(PWM_PIN, 0);
    pwm = 0;
    if (bypassRelayOn) {
      digitalWrite(BYPASS_RELAY_PIN, LOW);
      bypassRelayOn = false;
    }
    if (loopStartMillis - lastOn > PUMP_STOP_MILLIS) {
      digitalWrite(MAIN_RELAY_PIN, LOW);
      mainRelayOn = false;
    }
  }
}

void clearData() {
  modbusClearData();
  availablePower = 0;
  pwm = 0;
  elsens = 0;
  elsensPower = 0;
  wemoPower = 0;
}

boolean handleAlarm() {

  if (alarmCause == AlarmCause::NOT_IN_ALARM)
    return false;
  if (state != RegulatorState::ALARM) {
    state = RegulatorState::ALARM;
    clearData();
    balboaReset();
    msg.printf(F("alarm %c"), (char) alarmCause);
  }
  boolean stopAlarm = false;
  switch (alarmCause) {
#ifndef ethernet_h
    case AlarmCause::WIFI:
      stopAlarm = (WiFi.status() == WL_CONNECTED);
      break;
#endif
    case AlarmCause::PUMP:
      stopAlarm = buttonPressed;
      break;
    case AlarmCause::MODBUS:
      stopAlarm = requestSymoRTC();
      break;
    default:
      break;
  }
  if (stopAlarm) {
    alarmCause = AlarmCause::NOT_IN_ALARM;
    state = RegulatorState::MONITORING;
  }
  return !stopAlarm;
}

boolean restHours() {

  const int BEGIN_HOUR = 9;
  const int END_HOUR = 22; // to monitor discharge

  if (balboaRelayOn)
    return false;

  if (hourNow >= BEGIN_HOUR && hourNow < END_HOUR) {
    if (state == RegulatorState::REST) {
      state = RegulatorState::MONITORING;
    }
    return false;
  }
  if (state == RegulatorState::MONITORING) {
    state = RegulatorState::REST;
    clearData();
  }
  return true;
}

boolean turnMainRelayOn() {
  if (mainRelayOn)
    return true;
  valvesBackReset();
  digitalWrite(MAIN_RELAY_PIN, HIGH);
  mainRelayOn = true;
  return elsensCheckPump();
}

#ifndef ethernet_h
boolean wifiConnected() {
  static int tryCount = 0;
  if (WiFi.status() == WL_CONNECTED) {
    tryCount = 0;
    return true;
  }
  tryCount++;
  if (tryCount == 30) {
    alarmCause = AlarmCause::WIFI;
    eventsWrite(WIFI_EVENT, WiFi.status(), 0);
  }
  return false;
}
#endif
