// ** These are the constants **

#ifndef H_CONSTS_H
#define H_CONSTS_H

// secrets
byte mac[] = {0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED};
const char* mqtt_server = "192.168.0.60";
const char* mqtt_username = "xxx";
const char* mqtt_password = "xxx";
const int mqtt_port = 1883;

const char version[] = "build "  __DATE__ " " __TIME__;

#define FILE_NEW (O_READ | O_WRITE | O_CREAT)

// pinout for ARDUINO_SAMD_MKRZERO with MKR Connector Carrier > pin 4, 5, 8, 9, 10 are RESERVED
const byte TONE_PIN = 0; // Grove speaker module
const byte ZC_EI_PIN = 1; // Zero Crossing pin for TriacLib
const byte BYPASS_RELAY_PIN = 2; // SSRelay to bypass Triac
const byte TRIAC_PIN = 3;  // PWM on TCC0 WO pin for TriacLib

const byte NET_SS_PIN = 5;  // reserved for CS

// SPI 8, 9, 10 not on Carrier
// TWI 12, 11 used for I2C communication with Oled and ADC https://wiki.seeedstudio.com/Grove-I2C_ADC/
const byte ELSENS_PIN = A0; // CZH-LABS DIN Rail Mount +/-20Amp AC/DC Current Sensor Module, based on ACS712.
const byte TEMPSENS_PIN = A1; // Grove temp sensore MCP9808
const byte BUTTON_PIN = A2; // Pushbutton initiating NightCharge
const byte LEDBAR_DATA_PIN = 13; // connector labeled Serial (it is Serial1 RX)
const byte LEDBAR_CLOCK_PIN = LEDBAR_DATA_PIN + 1; //on one Grove connector (it is Serial1 TX)

const byte SD_SS_PIN = SDCARD_SS_PIN; // internal pin of MKR ZERO

// power settings, this needs to be checked
const int MAX_POWER = 760; // the max load of the TA-heater at 230V
const int MAX_ACCUMULATE = 4800; // Wh, 6400 is theoretical max, 4650 is usually achieved

const IPAddress galvoAddress(192,168,0,65);

enum struct RegulatorState {
  ALARM = 'A',
  MONITORING = 'M', // idle
  REGULATING = 'R', // surplus power > powerpilot
  MANUAL_RUN = 'H', // mqtt input > powerpilot
  ACCUMULATE = 'Y' // nightCharge
};

enum struct AlarmCause {
  NOT_IN_ALARM = '-',
  NETWORK = 'N',
  MODBUS = 'M',
  MQTT = 'Q',
  TRIAC = 'T'
};

// events array indexes
enum {
  EVENTS_SAVE_EVENT,
  RESTART_EVENT,
  WATCHDOG_EVENT,
  NETWORK_EVENT,
  MQTT_EVENT,
  MODBUS_EVENT,
  TRIAC_EVENT,
  STATS_SAVE_EVENT,
  EVENTS_SIZE
};

struct Stats {
  long heatingTime = 0; // minutes
  long consumedPower = 0; // watts
};

#endif
