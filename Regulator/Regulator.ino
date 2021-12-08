// Arduino IDE: 1.8.16 && SAMD Boards Package 1.8.11 - https://github.com/arduino/ArduinoCore-samd/releases
// if bricked press reset button twice fast

/******************************************************************************
                               --- TA Regulator ---

 DIY Arduino consumption regulator build to use excess solar power for charging
 a storage heater ("thermal battery") as means to enhance PV self-consumption
 ratio.
 
 It is based on the Regulator sketch as developed by Juraj Andrássy:
 https://github.com/jandrassy/Regulator. Check his version first – it provides
 many additional modules and functionalities that are not included in this
 version and is excellently set up, allowing for case-by-case modification.        
 
 This TA Regulator version has been tuned for "zero export" PV restriction,
 basically demanding a different mechanism to estheblish how much PV excess
 power exists, as any excess is by default suppressed by the export limiter and
 therefore not visible as surplus to the meter. To establish this untapped PV,
 a small reference PV panel placed next to the PV array is used. It tells how
 much PV (W) is potentially available from the sun through the "insolPower"
 value, derived from the shortcut current of the panel. This functionality is
 set up in the InSol module.
 
 ModBus data from the PV inverter and smartmeter are then used to see how much
 much PV is currently generated and consumed. The difference tells the untapped
 excess PV, which is then "regulated" as heating power for the storage heater.
 
 TA Regulator features MQTT, through which the heating power can be set
 independently ("manual") from the automatically regulated power. If power is
 set through MQTT, automatic regulation is disabled.
 
 A "night charging" function has been integrated through which a given amount
 of kWh can be set for over-night accumulatiuon in the heater, using cheap night
 tariff. It is enabled/disabled over a long press of the button. The level of
 night charge is determined with a weather prediction algorithm. With sunny
 skies predicted, the charge level is lower, with worsening or overcast skies
 the charge level is brought up. This is done through the ChargeForecast module.
 
 Functionality for an OLED display (128 x 64 or 128 x 128) is included, as well
 as a Led Matrix screen. Screens are switched off during night, but can get
 activated with a short pressing of the button.
 
 The platform used is ARDUINO_SAMD_MKRZERO with MKR Connector Carrier and ETH
 Shield. It can be made compatible with other platforms through by including
 platform specific instructions available from the original Regulator sketch.

 Hardware modifications made: a larger heatsink is connetced to the RobotDyn
 triac module (to prevent overheating), a subber is connected to the triac
 module (to prevent blowing-out the triac), and EMI line filters are put on
 the incoming and outgoing power line (to prevent distortion on the AC line
 from the dimmer regulation).
 
 packages required:
 StreamLib.h
 TimeLib.h
 MemoryFree.h
 ArduinoOTA.h
 TriacLib.h
 RTCZero.h
 lcdgfx.h
 avdweb_AnalogReadFast.h
 Grove_LED_Bar.h
 PubSubClient.h
 WDTZero.h
 Adafruit_Sensor.h
 Adafruit_BME280.h
 Encoder.h
 Adafruit_LEDBackpack.h
                             
******************************************************************************/

#include <StreamLib.h>
#include <TimeLib.h> // conflicts with RTCZero if Time.h is not removed: https://github.com/arduino-libraries/RTCZero/issues/28
#include <MemoryFree.h> // https://github.com/mpflaga/Arduino-MemoryFree

#include <Ethernet.h>
#define ETHERNET

boolean MQTT_ON = false; // "true" activates the MQTT modules for manual regulation, check TA-Pilot and MQTT module

#include <SD.h>
#define FS SD

#include <ArduinoOTA.h>

#define NO_EEPROM // ARDUINO_ARCH_SAMD

#include "const.h"
#include <TriacLib.h>

#include <Wire.h>

#include <RTCZero.h> // ARDUINO_ARCH_SAMD
RTCZero rtc;

#define NetServer EthernetServer
#define NetClient EthernetClient

unsigned long loopStartMillis;
byte hourNow; // current hour
byte minuteNow; // current minute
boolean setClock = false; // RTC is synchronised at BEGIN_HOUR daytime operation
const int BEGIN_HOUR = 8 ; // 08:00 start daytime operation
const int END_HOUR = 24; // 24:00 end daytime operation

const char* LOG_FN = "EVENTS.LOG";

RegulatorState state = RegulatorState::MONITORING;
AlarmCause alarmCause = AlarmCause::NOT_IN_ALARM;

boolean buttonPressed = false;
boolean nightCall = false;
int chargeSetRatio = 100; // ratio of charge depending on AccForecast 0-100
int chargeSetLevel; // the aimed charge level from forecast

long displayReport = 0; // delay in updating display

boolean bypassRelayOn = false;

// SunSpec Modbus values
int meterPower; // smart meter measured power
int meterPowerAvg; // averaged measured power
int meterPowerPhaseA;
int meterPowerPhaseB;
int meterPowerPhaseC;
int voltage = 230; // smart meter measured AC voltage
int inverterAC; // inverter AC power
boolean inverterThrottled = false;

// PowerPilot values
int stateRelay; // this is nightCall (1/0) set via MQTT
int setPower = 0; // the desired power (W) via MQTT

int heatingPower;
int powerPilotRaw;
int tresholdAvg; // averaged "overdraw" protection treshold
boolean pilotThrottled = false; // prevention of draw on PV underperformance (snow, etc.)

//ElSens values (ACS712 current sensor for logging and UI)
int elsens; // raw sensor measurement
int elsensPower; // TA power consumption calculation

// InSol values (PV panel insolation for excess calculation)
int insol; // raw ADC measurement of Ishortcut from panel
int insolPower; // available measured PV power calculation
int insolPowerAvg; // averaged insolPower

char msgBuff[256];
CStringBuilder msg(msgBuff, sizeof(msgBuff));

bool sdCardAvailable = false;

#ifdef __SD_H__
void sdTimeCallback(uint16_t* date, uint16_t* time) {
  *date = FAT_DATE(year(), month(), day());
  *time = FAT_TIME(hour(), minute(), second());
}
#endif


/******************************************************************************
*                                 --- SETUP ---                               *
******************************************************************************/

void setup() {

// setup solid state relay for bypass
    pinMode(BYPASS_RELAY_PIN, OUTPUT);
    digitalWrite(BYPASS_RELAY_PIN, LOW);

// deselect the SD card on eth shield
   pinMode(4,OUTPUT);
   digitalWrite(4, HIGH);

    Wire.begin();
    Wire.setClock(400000); // 400000
  
    pilotSetup();
    elsensSetup();
    insolSetup();
    chargeForecastSetup();
    buttonSetup();
    ledBarSetup();
    ledMatrixSetup();
    displaySetup();
  
// setup serial communication
    Serial.begin(115200);  // TX can be used if Serial is not used

  beep();

  rtc.begin();
  setTime(rtc.getEpoch());

#ifdef __SD_H__
  pinMode(NET_SS_PIN, OUTPUT);
  digitalWrite(NET_SS_PIN, HIGH); // unselect network device on SPI bus
  if (!SD.begin(SD_SS_PIN)) {
    alarmSound();
  } else {
    SdFile::dateTimeCallback(sdTimeCallback);
    sdCardAvailable = true;
  } 
#endif    

//  setup internet connection, status is checked in loop
    IPAddress ip(192, 168, 0, 61);
    Ethernet.init(NET_SS_PIN);
    Ethernet.begin(mac, ip);
    delay(150);

    ArduinoOTA.beforeApply(shutdown);
    ArduinoOTA.begin(ip, "TA-pilot", "11000", InternalStorage);

//  set DAC resulution to full 1024
    analogWriteResolution(10); // default is 8 bits (0-256)
    
  if (MQTT_ON) {
    MQTTSetup();
  }
  
    webServerSetup();
    modbusSetup();
    eventsSetup();
    statsSetup();
    csvLogSetup();
    watchdogSetup();
 
  }

/******************************************************************************
*                                 --- LOOP ---                                *
******************************************************************************/

void loop() {   
    
    loopStartMillis = millis();
    hourNow = hour();
    minuteNow = minute();

    handleSuspendAndOff();
    statsLoop();

    watchdogLoop();
    ArduinoOTA.poll();
    eventsLoop();
    
//  user interface
    buttonPressed = false;
    buttonLoop();
    beeperLoop();
    ledBarLoop();

// display control    
    if (loopStartMillis - displayReport > 2000) { // display update timer 2 sec
    displayReport = loopStartMillis;
    displayLoop();
    ledMatrixLoop();
    }
    if (buttonPressed == true) { // show display if button is pressed
    displayLoop();
    ledMatrixLoop();
    }

    webServerLoop();
    
    if (handleAlarm())
      return;

    if (!networkConnected())
      return;
    
    if (!modbusLoop()) // returns true if data-set is ready, otherwise breaks loop
      return;
      
    if (MQTT_ON) {
      if (!mqttConnected()) // if not critical this can be done once a day
        return;
    }
     
    elsensLoop();

    insolLoop();

    chargeForecastLoop();
         
    if (MQTT_ON) {
       MQTTLoop();
    }

    pilotLoop();

    csvLogLoop();

    syncRTC();
    
}

/************************************************************************
*                           --- FUNCTIONS ---                           *
************************************************************************/

void shutdown() {
  eventsSave();
  statsSave();
  watchdogLoop();
}

void handleSuspendAndOff() {
  if (state != RegulatorState::REGULATING && state != RegulatorState::ACCUMULATE) {
    heatingPower = 0;
  }
}

void clearData() {
  modbusClearData();
  powerPilotRaw = 0;
  elsens = 0;
  elsensPower = 0;
  insol = 0;
  insolPower = 0; 
}

boolean handleAlarm() {

  static unsigned long modbusCheckMillis;
  const unsigned long MODBUS_CHECK_INTERVAL = 5000;
  
  if (alarmCause == AlarmCause::NOT_IN_ALARM)
    return false;
  if (state != RegulatorState::ALARM) {
    state = RegulatorState::ALARM;
    clearData();
  }
  boolean stopAlarm = false;
  switch (alarmCause) {
    
    case AlarmCause::NETWORK:
      stopAlarm = (Ethernet.linkStatus() != LinkOFF);
      break;
    if (MQTT_ON) {
      case AlarmCause::MQTT:
      stopAlarm = (mqttConnected() != false); 
      break;
    }
      case AlarmCause::TRIAC:
      stopAlarm = (elsensCheckTriac() != false); 
      break;        
    case AlarmCause::MODBUS:
      if (millis() - modbusCheckMillis > MODBUS_CHECK_INTERVAL) {
        stopAlarm = requestSymoRTC() && requestMeter(); // meter is off in Symo emergency power mode
        modbusCheckMillis = millis();
      }
      break;
    default:
      break;
  }
  if (stopAlarm) {
    alarmCause = AlarmCause::NOT_IN_ALARM;
    state = RegulatorState::MONITORING;
    displayClear();
  }
  return !stopAlarm;
}

boolean nightChargeHours() {

// daytime operation window

// (1) return false if within daytime operation window
  if (hourNow >= BEGIN_HOUR && hourNow < END_HOUR) {
    if (state == RegulatorState::ACCUMULATE) { // terminate ACCUMULATE if within daytime operation window

        if (bypassRelayOn) { // switch off bypass relay if ACCUMULATE get terminated
        waitZeroCrossing();
        digitalWrite(BYPASS_RELAY_PIN, LOW);
        bypassRelayOn = false;
        }
      return true;
    }
    return false;
  }

// (2) switch to ACCUMULATE if not in daytime operation window
 
  if (state == RegulatorState::MONITORING && nightCall) {
    setPower = 0; //
  }
  return true;
}

boolean turnBypassRelayOn() {
  if (bypassRelayOn)
    return true;
  digitalWrite(BYPASS_RELAY_PIN, HIGH);
  bypassRelayOn = true;
}

boolean networkConnected() {
  static int tryCount = 0;
  if (Ethernet.linkStatus() != LinkOFF) {
    tryCount = 0;
    return true;
  }
  tryCount++;
  if (tryCount == 30) {
    alarmCause = AlarmCause::NETWORK;
    eventsWrite(NETWORK_EVENT, Ethernet.linkStatus(), 0);
  }
  return false;
}

void syncRTC() {
  if (hourNow = BEGIN_HOUR) { // optional add: && minuteNow == 0
    if (setClock = false) {
    requestSymoRTC(); // every morning sync clock
    setClock = true;
      if (hour() < BEGIN_HOUR)  // sync can set the clock back           
        FS.remove(LOG_FN);
     }
  }
  if (hourNow > BEGIN_HOUR) {
    setClock = false;
  }
}

void waitZeroCrossing() {
  Triac::waitZeroCrossing();
}


void log(const char *msg) {
#ifdef FS
  File file = FS.open(LOG_FN, FILE_WRITE);
  if (file) {
    unsigned long t = now();
    char buff[10];
    sprintf_P(buff, PSTR("%02d:%02d:%02d "), hour(t), minute(t), second(t));
    file.print(buff);
    file.println(msg);
    file.close();
  }
#endif

}
