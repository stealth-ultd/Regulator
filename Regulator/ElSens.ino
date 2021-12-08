// ** The ACS712 current sensor reporting on TA-Regulator consumption on AC line in **

#include <avdweb_AnalogReadFast.h> // https://www.avdweb.nl/arduino/adc-dac/fast-10-bit-adc

const int ELSENS_ANALOG_MIDDLE_VALUE = 2048; // half value of 12-bit 4096

void elsensSetup() {
    analogReadResolution(12); // added for ADC with avdweb_AnalogReadFast.h, 10 bit = 2^10 or 1024
}

void elsensLoop() {

  // system's power factor characteristics
  const float PF_ANGLE_INTERVAL = PI * 0.20; // 0.22
  const float PF_ANGLE_SHIFT = PI * 0.31; // 0.286

  // ACS712 20A analogReadFast over MKR Connector Carrier A pin's voltage divider with capacitor removed
  const int ELSENS_MAX_VALUE = 760; // W
  const float ELSENS_VALUE_COEF = 2.99; // 1.86

  const int ELSENS_VALUE_SHIFT = 5; // this represents the base load (+/ 5 Watt)
  const int ELSENS_MIN_HEATING_VALUE = 40; // this sets the "noise" level

  elsens = readElSens();

  if (elsens > ELSENS_MIN_HEATING_VALUE) {
    float ratio = 1.0 - ((float) elsens / ELSENS_MAX_VALUE); // to 'guess' the 'power factor'
    elsensPower = (int) ((elsens * ELSENS_VALUE_COEF * cos(PF_ANGLE_SHIFT + ratio * PF_ANGLE_INTERVAL)) + ELSENS_VALUE_SHIFT);
  }
  else {
    elsensPower = 0;
  }
}

boolean elsensCheckTriac() { // alerts if TRIAC has shorted out

  const int TRIAC_MAX_TRESHOLD = 50 + (heatingPower * 1.2); 
  if (elsens > TRIAC_MAX_TRESHOLD) {
    alarmCause = AlarmCause::TRIAC;
    eventsWrite(TRIAC_EVENT, elsens, TRIAC_MAX_TRESHOLD);
    return false;
  }
  return true;
}

int readElSens() { // return value is RMS of sampled values

  const float RMS_INT_SCALE = 2.5; // c2.5 for 12 bit ADC, 10 for 10 bit ADC

  unsigned long long sum = 0;
  int n = 0;
  unsigned long start_time = millis();
  while (millis() - start_time < 200) { // in 200 ms measures 10 50Hz AC oscillations // was 200
    long v = (short) elsensAnalogRead() - ELSENS_ANALOG_MIDDLE_VALUE;
    sum += v * v;
    n++;
  }
  if (ELSENS_ANALOG_MIDDLE_VALUE == 0) {
    n = n / 2; // half of the values are zeros for removed negative voltages
  }
  return sqrt((double) sum / n) * RMS_INT_SCALE;
}

void elsensWaitZeroCrossing() {

  const int ZC_BAND = 10;

  unsigned long startMillis = millis();
  while (millis() - startMillis < 10) { // 10 milliseconds of AC half wave
    short v = elsensAnalogRead();
    if (v > ELSENS_ANALOG_MIDDLE_VALUE - ZC_BAND && v < ELSENS_ANALOG_MIDDLE_VALUE + ZC_BAND)
      break;
  }
}

unsigned short elsensAnalogRead() {
  return analogReadFast(ELSENS_PIN);
}
