// ** Fronius Symo Hybrid SunSpec Modbus **

// inverter Modbus settings: tcp, port 502, offset 101, int + SF

const byte METER_UID = 240;
const int MODBUS_CONNECT_ERROR = -10;
const int MODBUS_NO_RESPONSE = -11;

const byte FNC_READ_REGS = 0x03;
const byte FNC_WRITE_SINGLE = 0x06;
const byte FNC_ERR_FLAG = 0x80;

enum {
  MODBUS_DELAY,
  INVERTER_DATA,
  METER_DATA
};

NetClient modbus;

void modbusSetup() {

  const byte GET_TIME_ATTEMPTS_COUNT = 3;

  for (int i = 0; i < GET_TIME_ATTEMPTS_COUNT; i++) {
    if (requestSymoRTC())
      break;
    delay(500);
  }
}

boolean modbusLoop() {

  const int DELAY_MILLIS = 4000;
  const int DATASET_MILLIS = 12000;
  const byte DATASET_FAIL_COUNT = 5;

  static byte datasetState = MODBUS_DELAY;
  static unsigned long datasetStart;
  static byte datasetFailCounter;

  if (datasetState != MODBUS_DELAY && loopStartMillis - datasetStart > DATASET_MILLIS) { // problems getting complete data-set in time
    modbusClearData(); // for UI
    byte failDataState = datasetState;
    datasetState = INVERTER_DATA; // start again
    datasetStart = loopStartMillis;
    datasetFailCounter++;
    if (datasetFailCounter == DATASET_FAIL_COUNT) { 
      eventsWrite(MODBUS_EVENT, 0, failDataState);
      alarmCause = AlarmCause::MODBUS;
      return false;
    }
  }

  switch (datasetState) {
    case MODBUS_DELAY: 
      if (loopStartMillis - datasetStart < DELAY_MILLIS) // this is the case at startup and during delay
        return false;
      datasetState = INVERTER_DATA;
      datasetStart = loopStartMillis;   
      break;
    case INVERTER_DATA:
      if (!requestInverter())
        return false;
      datasetState = METER_DATA;
      break;
    case METER_DATA:
      if (!requestMeter())
        return false;
      datasetState = MODBUS_DELAY;
      datasetFailCounter = 0;
      break;
  }
  return (datasetState == MODBUS_DELAY);
}

void modbusClearData() {
  meterPower = 0;
  meterPowerPhaseA = 0;
  meterPowerPhaseB = 0;
  meterPowerPhaseC = 0;
  inverterAC = 0;
  voltage = 0;
}

boolean requestSymoRTC() {
  short regs[2];
  int res = modbusRequest(1, 40222, 2, regs);
  if (modbusError(res))
    return false;
  // SunSpec has seconds from 1.1.2000 UTC, TimeLib uses 'epoch' (seconds from 1.1.1970)
  setTime(SECS_YR_2000 + (unsigned short) regs[0] * 65536L + (unsigned short) regs[1] + SECS_PER_HOUR); // Europe DST
  int m = month();
  if (m > 10 || m < 3) {
    setTime(now() - SECS_PER_HOUR);
  } else if (m == 3) {
    int d = 31 - ((((5 * year()) / 4) + 4) % 7);
    if (day() < d) {
      setTime(now() - SECS_PER_HOUR);
    }
  } else if (m == 10) {
    int d = 31 - ((((5 * year()) / 4) + 1) % 7);
    if (day() >= d) {
      setTime(now() - SECS_PER_HOUR);
    }
  }
  hourNow = hour();
  minuteNow = minute();
#ifdef ARDUINO_ARCH_SAMD
  rtc.setEpoch(now());
#endif
  return true;
}

boolean requestInverter() {
  short regs[2];
  int res = modbusRequest(1, 40083, 2, regs);
  if (modbusError(res))
    return false;
  inverterAC = regs[0] * pow(10, regs[1]); // ac power * scale
  return true;
}

boolean requestMeter() {
  short regs[16];
  int res = modbusRequest(METER_UID, 40076, 16, regs); // start counting registers: 40077 = [0] 
  if (modbusError(res))
    return false;
  voltage = regs[0] * pow(10, regs[8]); // ac voltage Ph(A-C) * scale, note: in Regulator regs[3] >> ac voltage F3 * scale
  float sf = pow(10, regs[15]); // 40092
  meterPower = -regs[11] * sf; // ac power * scale
  meterPowerPhaseA = regs[12] * sf; // 40089
  meterPowerPhaseB = regs[13] * sf;
  meterPowerPhaseC = regs[14] * sf;
  return true;
}

boolean modbusError(int err) {

  const byte ERROR_COUNT_ALARM = 8;
  
  static byte modbusErrorCounter = 0;
  static int modbusErrorCode = 0;

  if (modbusErrorCode != err) {
    modbusErrorCounter = 0;
    modbusErrorCode = err;
  }
  if (err == 0)
    return false;
  modbusErrorCounter++;
  switch (modbusErrorCounter) {
    case 1:
//      msg.printf(F("modbus error %d"), err);
    break;
    case ERROR_COUNT_ALARM:
//      msg.printf(F("modbus error %d %d times"), err, modbusErrorCounter);
      if (state != RegulatorState::ALARM) {
        eventsWrite(MODBUS_EVENT, err, 0);
        alarmCause = AlarmCause::MODBUS;
      }
      modbus.stop();
    break;
  }
  return true;
}

/*
 * return
 *   - 0 is success
 *   - negative is comm error
 *   - positive value is modbus protocol exception code
 *   - error 4 is SLAVE_DEVICE_FAILURE. Check if 'Inverter control via Modbus' is enabled.
 */
int modbusRequest(byte uid, unsigned int addr, byte len, short *regs) {

  const byte CODE_IX = 7;
  const byte ERR_CODE_IX = 8;
  const byte LENGTH_IX = 8;
  const byte DATA_IX = 9;

  int err = modbusConnection();
  if (err != 0)
    return err;

  byte request[] = {0, 1, 0, 0, 0, 6, uid, FNC_READ_REGS, (byte) (addr / 256), (byte) (addr % 256), 0, len};
  modbus.write(request, sizeof(request));

  int respDataLen = len * 2;
  byte response[max((int) DATA_IX, respDataLen)];
  int readLen = modbus.readBytes(response, DATA_IX);
  if (readLen < DATA_IX) {
    modbus.stop();
    return MODBUS_NO_RESPONSE;
  }
  switch (response[CODE_IX]) {
    case FNC_READ_REGS:
      break;
    case (FNC_ERR_FLAG | FNC_READ_REGS):
      return response[ERR_CODE_IX]; // 0x01, 0x02, 0x03 or 0x11
    default:
      return -3;
  }
  if (response[LENGTH_IX] != respDataLen)
    return -2;
  readLen = modbus.readBytes(response, respDataLen);
  if (readLen < respDataLen)
    return -4;
  for (int i = 0, j = 0; i < len; i++, j += 2) {
    regs[i] = response[j] * 256 + response[j + 1];
  }
  return 0;
}

int modbusConnection() {
  if (!modbus.connected()) {
    modbus.stop();
    if (!modbus.connect(galvoAddress, 502))
      return MODBUS_CONNECT_ERROR;
    modbus.setTimeout(2000);
  } else {
    while (modbus.read() != -1); // clean the buffer
  }
  return 0;
}
