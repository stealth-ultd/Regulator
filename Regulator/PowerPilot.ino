
// uncomment line 73-75 for MQTT_ON

void pilotTriacPeriod(float p) {
  Triac::setPeriod(p);
}

void pilotSetup() {
  Triac::setup(ZC_EI_PIN, TRIAC_PIN);
  pilotTriacPeriod(0);
}

const int MIN_POWER = 35; // W, lowest absolute value for Triac activation
const int MIN_START_POWER = 50; // W, lowest treshold for PowerPilot start
const int INSOL_BUFFER = 10; // W, safety net on low insol levels// 30
const int BYPASS_POWER = 700; // when the TA-heater goes in bypass mode (relay) // 700
const int BYPASS_MIN_START_POWER = BYPASS_POWER + 20;

float tresholdMAvg; // value for moving average
long tresholdReport; // delay in updating tresholdAv
unsigned long nTRS; // number of samples


void pilotLoop() {

// sum available power
   short availablePower = heatingPower + meterPower + (insolPowerAvg * 0.95) - inverterAC - INSOL_BUFFER; // 0.95 to be on safe side

// PV Throttled "overdraw" protection
// checks if heatingPower pushes meterPower into minus on underperfoming PV output (compared to PV reference panel, i.e. snow)
    tresholdAveraging(); // we have to be patient as the inverter needs time to ramp up to the power consumption demand
    if (((float) insolPowerAvg / (inverterAC + 1)  >= 1.20) && tresholdAvg <= -50) { // in case PV underperforms, don't draw power
    availablePower = 0;
    pilotThrottled = true; // status to display
  } else {
    pilotThrottled = false;
  }

// set pilot strategy

// case: MQTT set
  if (setPower > 0) { // MQTT setting overrules regulation
      availablePower = setPower; 
      state = RegulatorState::MANUAL_RUN;
    }
// case: NightCharge 
  if (nightCall) { // check if nightcharge is on    
  chargeSetLevel = (0.67 * MAX_ACCUMULATE) + (0.33* MAX_ACCUMULATE * chargeSetRatio / 100); // accumulation at 62,5%, 75%, 87,5%, or 100% of MAX
    if (nightChargeHours() && statsAccumulatedPowerToday() <= chargeSetLevel) {  
    availablePower = MAX_POWER;
    state = RegulatorState::ACCUMULATE;
    }
  }
    
// case: suspend on low startup power
  if (!MQTT_ON && heatingPower == 0 && availablePower < MIN_START_POWER) { // interferes with MQTT setPower = 0
    return;
    }
// case: Bypass relay active
  if (bypassRelayOn && availablePower > BYPASS_POWER)
    return;
// case: switch to MONITORING on low power
  if (availablePower < MIN_POWER) { // is power less than starting up treshold? 
    heatingPower = 0; // check this
    powerPilotRaw = 0;
    availablePower = 0;
    state = RegulatorState::MONITORING;
    
      if (bypassRelayOn) {
        waitZeroCrossing();
        // the actual switching is after 10 ms on next ZC
        digitalWrite(BYPASS_RELAY_PIN, LOW);
        bypassRelayOn = false;
        }
 
    if (!MQTT_ON) { 
//    return; // commented to prevent power being "stuck" with MQTT setPower = 0 <<<<<<<<<<<<<< !!
    }
  
  }
// case: regulating
if (availablePower >= MIN_START_POWER && setPower == 0 && !nightChargeHours()) { // – are we regulating?
    state = RegulatorState::REGULATING;    
    }  

// bypass the triac for max power
boolean bypass = availablePower > (bypassRelayOn ? BYPASS_POWER : BYPASS_MIN_START_POWER);

// set heating power
  float r = bypass ? 0 : power2TriacPeriod(availablePower);
  pilotTriacPeriod(r);
  powerPilotRaw = r * 1000; // to log 

if (bypass != bypassRelayOn) {
    waitZeroCrossing();
    digitalWrite(BYPASS_RELAY_PIN, bypass);
    bypassRelayOn = bypass;
  }

heatingPower = bypass ? BYPASS_POWER : (availablePower > MAX_POWER) ? MAX_POWER : availablePower;
}

void powerPilotStop() {
  pilotTriacPeriod(0);
  powerPilotRaw = 0;
}

float power2TriacPeriod(int power) {
  const float POWER2PERIOD_SHIFT = 0.027 ; // 0.03
  const float POWER2PERIOD_KOEF = 0.61; // 0.605

  if (power < MIN_POWER)
    return 0.0;
  if (power >= MAX_POWER)
    return 0.95;
  float ratio = (float) power / MAX_POWER;
  float linevoltagefactor = (float) 220 / voltage; // line voltage correction
  
  
 return POWER2PERIOD_SHIFT + POWER2PERIOD_KOEF * asin(sqrt(ratio)) * linevoltagefactor; // * linevoltage; 
}

void tresholdAveraging() { // floating average for PV "overdraw" protection check
  int treshold = heatingPower + meterPower;
  
  nTRS++; // update sample count
  tresholdMAvg += (treshold - tresholdMAvg) / nTRS; // calculate moving average
    
    if (loopStartMillis - tresholdReport > 10 * 1000) { // 10 secs polling
    tresholdAvg = tresholdMAvg; // retreive result after polling time
    tresholdReport = loopStartMillis;
    nTRS = 0; // to start a new averaging period, set number of samples (n) to zero
    }
}
