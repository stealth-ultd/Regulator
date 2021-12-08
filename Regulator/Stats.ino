// ** Compiles the Stats and saves to SD card (FAT 32) **

const char* STATS_FILENAME = "STATS.DAT";

  struct {
  time_t timestamp = 0; 
  Stats day;
  Stats month;
  Stats year;
  Stats dayAccumulate;
  Stats monthAccumulate;
  Stats yearAccumulate;
  Stats dayManualRun;
  Stats monthManualRun;
  Stats yearManualRun;
  } statsData;

unsigned long statsMilliwats = 0;
unsigned long statsMilliwatMilis = 0;
bool statsAccumulateFlag;
bool statsManualRunFlag;
bool statsRegulateFlag;
unsigned long statsSaveTimer = 0;


void statsSetup() {
  File file = FS.open(STATS_FILENAME, FILE_READ);
  if (file) {
    file.readBytes((byte*) &statsData, sizeof(statsData));
    file.close();
  }
}

void statsLoop() {

  const unsigned long STATS_MILLIWATS_INTERVAL = 5 * 60000; // 5 minutes
  const unsigned long STATS_SAVE_INTERVAL = 30 * 60000; // 30 minutes
  static unsigned long lastPowerChangeMillis = 0;
  static int lastPower = 0;

  if (lastPowerChangeMillis == 0) { // not counting
    lastPowerChangeMillis = loopStartMillis; // start counting
    statsSaveTimer = loopStartMillis;
  }
  time_t t = statsData.timestamp;
  if (day(now()) != day(t)) {

    char buff[32];
    sprintf(buff, "DATA%d.CSV", year(t));
    File file = FS.open(buff, FILE_WRITE);
    if (file) {
      sprintf(buff, "%d-%02d-%02d;%04d;%04d;%04d", // "%d-%02d-%02d;%ld;%ld;%ld"
          year(t), month(t), day(t),
          statsData.day.consumedPower,
          statsData.dayAccumulate.consumedPower,
          statsData.dayManualRun.consumedPower);
      file.println(buff);
      file.close();
    }

    if (month(now()) != month(statsData.timestamp)) {
//      memset(&statsData, 0, sizeof(statsData.month.consumedPower));
//      memset(&statsData, 0, sizeof(statsData.monthAccumulate.consumedPower));
//      memset(&statsData, 0, sizeof(statsData.monthManualRun.consumedPower));
      statsData.month.consumedPower = 0;
      statsData.monthAccumulate.consumedPower = 0;
      statsData.monthManualRun.consumedPower = 0;  
    }
    if (year(now()) != year(statsData.timestamp)) { // this section is new
      memset(&statsData, 0, sizeof(statsData));   
    }
    else {
      statsData.day.consumedPower = 0;
      statsData.dayAccumulate.consumedPower = 0;
      statsData.dayManualRun.consumedPower = 0;
    }
    statsData.timestamp = now();
  }

  unsigned long interval = loopStartMillis - lastPowerChangeMillis;
  int power = statsEvalCurrentPower();
  if (power != lastPower || interval > STATS_MILLIWATS_INTERVAL) {
    if (lastPower > 0) {
      statsMilliwats += (float) lastPower/3.6 * (float) interval/1000;
      statsMilliwatMilis += interval;
      if (power == 0 || statsMilliwatMilis > STATS_MILLIWATS_INTERVAL) {
        statsAddMilliwats();
      }
    }
    lastPower = power;
    lastPowerChangeMillis = loopStartMillis;
  }

  if (loopStartMillis - statsSaveTimer > STATS_SAVE_INTERVAL) {
    statsSave();
  }

}

int statsEvalCurrentPower() {

  switch (state) {
    case RegulatorState::ACCUMULATE:
    case RegulatorState::MANUAL_RUN:
    case RegulatorState::REGULATING:
      return elsensPower;
    default:
      return 0;
  }
}

void statsAddMilliwats() {
  int heatingTime =  round((float) statsMilliwatMilis / 60000); // minutes
  int consumedPower = round((float) statsMilliwats / 1000); // watt
  statsMilliwatMilis = 0;
  statsMilliwats = 0;
  
  statsAccumulateFlag = (state == RegulatorState::ACCUMULATE);
  statsManualRunFlag = (state == RegulatorState::MANUAL_RUN);
  statsRegulateFlag = (state == RegulatorState::REGULATING || state == RegulatorState::MONITORING);

        
 if (statsAccumulateFlag) {
    statsData.dayAccumulate.consumedPower += consumedPower;
    statsData.monthAccumulate.consumedPower += consumedPower;
    statsData.yearAccumulate.consumedPower += consumedPower;
    }
  if (statsManualRunFlag) {
    statsData.dayManualRun.consumedPower += consumedPower;
    statsData.monthManualRun.consumedPower += consumedPower;
    statsData.yearManualRun.consumedPower += consumedPower;
    }
  if (statsRegulateFlag) {
    statsData.day.consumedPower += consumedPower;
    statsData.month.consumedPower += consumedPower;
    statsData.year.consumedPower += consumedPower;
    
  }
}

void statsSave() {
  if (!statsMilliwatMilis)
    return;
  statsAddMilliwats();

  File file = FS.open(STATS_FILENAME, FILE_NEW);
  if (file) {
    file.write((byte*) &statsData, sizeof(statsData));
    file.close();
  }

  eventsWrite(STATS_SAVE_EVENT, (loopStartMillis - statsSaveTimer) / 60000, 0);
  statsSaveTimer = loopStartMillis;
}

int statsRegulatedPowerToday() { // this is used by display and webserver
  if (day(now()) != day(statsData.timestamp))
    return 0;
  return statsData.day.consumedPower; // Wh
}

int statsAccumulatedPowerToday() { // this is used by display and webserver
  if (day(now()) != day(statsData.timestamp))
    return 0;
  return statsData.dayAccumulate.consumedPower; // Wh
}

int statsManualPowerToday() { // this is used by display and webserver
  if (day(now()) != day(statsData.timestamp))
    return 0;
  return statsData.dayManualRun.consumedPower; // Wh
}

void statsPrintJson(FormattedPrint& out) {
  out.printf(F("{\"timestamp\":%lu,"
      "\"emptyline\":%ld,"
      "\"dayConsumedPower\":%ld,"
      "\"monthConsumedPower\":%ld,"
      "\"yearConsumedPower\":%ld,"
      "\"dayAccumulatePower\":%ld,"
      "\"monthAccumulatePower\":%ld,"
      "\"yearAccumulatePower\":%ld,"    
      "\"dayManualRunPower\":%ld,"
      "\"monthManualRunPower\":%ld,"
      "\"yearManualRunPower\":%ld"),
      statsData.timestamp,
      statsData.day.consumedPower,
      statsData.month.consumedPower,
      statsData.year.consumedPower,
      statsData.dayAccumulate.consumedPower,
      statsData.monthAccumulate.consumedPower,
      statsData.yearAccumulate.consumedPower,    
      statsData.dayManualRun.consumedPower,
      statsData.monthManualRun.consumedPower,
      statsData.yearManualRun.consumedPower);
   out.printf(",\"fn\":\"DATA%d.CSV\"", year());
   out.print('}');
}
