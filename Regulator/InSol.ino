// ** The insolation value read from a 1W "reference" PV panel and translated to available PV power **

// wiring diagram notes:
// InSol Aviation Connector on Regulator housing: 1 = yellow, 2 = white, 3 = red, 4 = black
// InSol UTP connection cable:      1 = blue , 2 = green, 3 = orange, 4 = brown
// Insol Aviation Connector to PV panel: (1) brown = ground, (2) orange = Vpanel

// Voltaic Systems 1W 6V Solar Panel (0.19A Isc shortcut current), shorted with a 15Ω 5W resister, and connected to ADC

// ADC121 Grove I2C ADC
// Working Voltage 5.0 VDC
// Max ADC input: 3.0 VDC

#define I2C_ADDR_ADC121             0x50
#define V_REF 3.0

const byte REG_ADDR_RESULT = 0x00;
const byte REG_ADDR_CONFIG = 0x02;

float insolMAvg; // value for moving average to flatten out fluctuastions
long insolReport; // delay in updating insolPowerAv
unsigned long nSol; // number of samples

void init_adc()
{
  Wire.beginTransmission(I2C_ADDR_ADC121);    // transmit to device
  Wire.write(REG_ADDR_CONFIG);                // configuration Register
  Wire.write(REG_ADDR_RESULT);
  Wire.endTransmission();

  Wire.beginTransmission(I2C_ADDR_ADC121);    // transmit to device
  Wire.write(REG_ADDR_RESULT);                // get result
  Wire.endTransmission();
}
 
unsigned short read_adc()     //unsigned int *data
{
  unsigned long long adc_sum = 0;
  int n = 0;
  unsigned long start_adc_time = millis();
  
//  Wire.beginTransmission(I2C_ADDR_ADC121);    // transmit to device
//  Wire.write(REG_ADDR_RESULT);                // get result
//  Wire.endTransmission();

  while (millis() - start_adc_time < 10) { // measures samples within 10 ms
  
  Wire.requestFrom(I2C_ADDR_ADC121, 2);
  byte buff[2];
  Wire.readBytes(buff, 2);
  long v = (buff[0] << 8) | buff[1];
  adc_sum += v;
  n++;
    }
  return (adc_sum / n) *V_REF*2/4096*1667; // 4096 for 12 bit, scaler 1667 for 3 V_REF
}
   
void insolSetup()
{
  init_adc();
}
 
void insolLoop()
{
  
// Insolation measurement system  characteristics >> simplified version is used
//  const float IS_ANGLE_INTERVAL = PI * 0.01;
//  const float IS_ANGLE_SHIFT = PI * 0.01;

// Grove ADC reading Isc of reference panel
//  const int INSOL_MAX_VALUE = 2000; // 2000
  const float INSOL_VALUE_COEF = 1.05; // 1.11

  const int INSOL_VALUE_SHIFT = -15; // this corrects the baseline value
  const int INSOL_MIN_VALUE = 20; // this sets the "noise" level
  
  insol = read_adc();

  if (insol > INSOL_MIN_VALUE) {
//    float ratio = 1.0 - ((float) insol / INSOL_MAX_VALUE);
//    insolPower = (int) (insol * INSOL_VALUE_COEF * cos(IS_ANGLE_SHIFT + ratio * IS_ANGLE_INTERVAL)) + INSOL_VALUE_SHIFT;
    insolPower = (int) (insol * INSOL_VALUE_COEF) + INSOL_VALUE_SHIFT; // simplified version
  }
  else {
    insolPower = 0;
  }

 // floating average
  nSol++; // update sample count
  insolMAvg += (insolPower - insolMAvg) / nSol; // calculate moving average
    
  if (loopStartMillis - insolReport > 5 * 1000) { // 5 secs polling
  insolPowerAvg = insolMAvg; // retreive result after polling time
  if (insolPowerAvg < 0) {
    insolPowerAvg = 0;
  }
  insolReport = loopStartMillis;
  nSol = 0; // to start a new averaging period, set number of samples (n) to zero
  }    
}
