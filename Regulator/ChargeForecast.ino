// ** The Zambretti algorithm wather forecast module **

// introduction to the Zambretti algorithm: https://web.archive.org/web/20110610213848/http://www.meteormetrics.com/zambretti.htm
// code implemented: https://create.arduino.cc/projecthub/Arduino_Genuino/gnome-forecaster-c386da
// BME280 temperature/humidity sensor: https://lastminuteengineers.com/bme280-arduino-tutorial/

#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>

#define SEALEVELPRESSURE_HPA (1013.25)

Adafruit_BME280 bme;  // initialise the BME280 sensor

boolean bmesensor = true;

int t_ForecastHour = 0;
int t_ForecastMinute = 0;

int pressureArray[10] = {0};  // here we store the pressure readings 
byte forecastCounter = 0;
byte delta_time = 0;
int Z = 0; // Zambretti number

int forecastIndex = 1; // 1 = sunny, 2 = sunny/cloudy, 3 = worsening, 4 = cloudy, 5 = rainy
int forecastTrend = 0; // 1 = raising, 2 = falling, 3 = steady, 0 = no data
// chargeSetRatio x/100 > sunny = 25, sunny/cloudy = 50, worsening = 75, cloudy = 100, rainy = 100

int temperature;
int humidity;
int pressure;
int altitude = 140; // the REAL altitude of the sensor, plot = 134, street = 132, 145 holds correction for temp


void chargeForecastSetup() {
  if(! bme.begin(0x76, &Wire)) {
  bmesensor = false; 
  }
}

void chargeForecastLoop() {

  temperature = (int)bme.readTemperature();
  humidity = (int)bme.readHumidity();
  pressure = (int)(bme.readPressure() / 100.0F);

  int seapressure = station2sealevel(pressure, altitude, temperature);
  
  if (hourNow != t_ForecastHour or minuteNow != t_ForecastMinute) {
    delta_time++;
    if (delta_time > 10) { // every minute we increment delta_time, then every 10 minutes
      delta_time = 0;      // we store the value in the array 

      if (forecastCounter == 10)  // if we read 10 values and filled up the array, we shift the array content
      {
        for (int i = 0; i < 9; i++) {   // we shift the array one position to the left
          pressureArray[i] = pressureArray[i + 1];
        }
        pressureArray[forecastCounter - 1] = seapressure;
      }
      else { // this code fills up the pressure array value by value until is filled up
        pressureArray[forecastCounter] = seapressure;
        forecastCounter++;
      }
    }

    Z = calc_zambretti((pressureArray[9] + pressureArray[8] + pressureArray[7]) / 3, (pressureArray[0] + pressureArray[1] + pressureArray[2]) / 3, month(now()));

// From here we generate output
 
    if (pressureArray[9] > 0 and pressureArray[0] > 0) {
      if (pressureArray[9] + pressureArray[8] + pressureArray[7] - pressureArray[0] - pressureArray[1] - pressureArray[2] >= 3) {
        // ## RAISING ##
        forecastTrend = 1;
        if (Z < 3) { // ("Sunny")
          forecastIndex = 1;
          chargeSetRatio= 25;
        }
        else if (Z >= 3 and Z <= 9) { // ("Sunny Cloudy")
          forecastIndex = 2;
          chargeSetRatio = 50;
        }
        else if (Z > 9 and Z <= 17) { // "Cloudy")
          forecastIndex = 4;
          chargeSetRatio = 100;
        }
        else if (Z > 17) { // ("Rainy")
          forecastIndex = 5;
          chargeSetRatio = 100;
        }
      }

      else if (pressureArray[0] + pressureArray[1] + pressureArray[2] - pressureArray[9] - pressureArray[8] - pressureArray[7] >= 3) {
        // ## FALLING ##
        forecastTrend = 2;
        if (Z < 4) { // ("Sunny")
          forecastIndex = 1;
          chargeSetRatio = 25;
        }
        else if (Z >= 4 and Z < 14) { // ("Sunny Cloudy");
          forecastIndex = 2;
          chargeSetRatio = 50;
        }
        else if (Z >= 14 and Z < 19) { // ("Worsening")
          forecastIndex = 3;
          chargeSetRatio = 75;
        }
        else if (Z >= 19 and Z < 21) { // ("Cloudy")
          forecastIndex = 4;
          chargeSetRatio = 100;
        }
        else if (Z >= 21) { // ("Rainy")
          forecastIndex = 5;
          chargeSetRatio = 100;
        }
      }
      else {
        // ## STEADY ##
        forecastTrend = 3;
        if (Z < 5) { // ("Sunny")
          forecastIndex = 1;
          chargeSetRatio = 25;
        }
        else if (Z >= 5 and Z <= 11) { // ("Sunny Cloudy")
          forecastIndex = 2;
          chargeSetRatio = 50;
        }
        else if (Z > 11 and Z < 14) { // ("Cloudy")
          forecastIndex = 4;
          chargeSetRatio = 100;
        }
        else if (Z >= 14 and Z < 19) { // ("Worsening")
          forecastIndex = 3;
          chargeSetRatio = 75;
        }
        else if (Z >= 19) { // ("Rainy")
          forecastIndex = 5;
          chargeSetRatio = 100;
        }
      }
    }
    else {
      forecastTrend = 0;
      if (seapressure < 1005) { // ("Rainy")
        forecastIndex = 5;
        chargeSetRatio = 100;
      }
      else if (seapressure >= 1005 and seapressure <= 1015) { // ("Cloudy")
        forecastIndex = 4;
        chargeSetRatio = 100;
      }
      else if (seapressure > 1015 and seapressure < 1025) { // ("Sunny Cloudy")
        forecastIndex = 2;
        chargeSetRatio = 50;
      }
      else { // ("Rainy")
        forecastIndex = 5;
        chargeSetRatio = 100;
      }
    }

    t_ForecastHour = hourNow;
    t_ForecastMinute = minuteNow;

  }
}

// Zambretti algorithm

int calc_zambretti(int curr_pressure, int prev_pressure, int mon) {
  if (curr_pressure < prev_pressure) {
    //FALLING
    if (mon >= 4 and mon <= 9)
      //summer
    {
      if (curr_pressure >= 1030)
        return 2;
      else if (curr_pressure >= 1020 and curr_pressure < 1030)
        return 8;
      else if (curr_pressure >= 1010 and curr_pressure < 1020)
        return 18;
      else if (curr_pressure >= 1000 and curr_pressure < 1010)
        return 21;
      else if (curr_pressure >= 990 and curr_pressure < 1000)
        return 24;
      else if (curr_pressure >= 980 and curr_pressure < 990)
        return 24;
      else if (curr_pressure >= 970 and curr_pressure < 980)
        return 26;
      else if (curr_pressure < 970)
        return 26;
    }
    else {
      //winter
      if (curr_pressure >= 1030)
        return 2;
      else if (curr_pressure >= 1020 and curr_pressure < 1030)
        return 8;
      else if (curr_pressure >= 1010 and curr_pressure < 1020)
        return 15;
      else if (curr_pressure >= 1000 and curr_pressure < 1010)
        return 21;
      else if (curr_pressure >= 990 and curr_pressure < 1000)
        return 22;
      else if (curr_pressure >= 980 and curr_pressure < 990)
        return 24;
      else if (curr_pressure >= 970 and curr_pressure < 980)
        return 26;
      else if (curr_pressure < 970)
        return 26;
    }
  }
  else if (curr_pressure > prev_pressure) {
    //RAISING
    if (mon >= 4 and mon <= 9) {
      //summer
      if (curr_pressure >= 1030)
        return 1;
      else if (curr_pressure >= 1020 and curr_pressure < 1030)
        return 2;
      else if (curr_pressure >= 1010 and curr_pressure < 1020)
        return 3;
      else if (curr_pressure >= 1000 and curr_pressure < 1010)
        return 7;
      else if (curr_pressure >= 990 and curr_pressure < 1000)
        return 9;
      else if (curr_pressure >= 980 and curr_pressure < 990)
        return 12;
      else if (curr_pressure >= 970 and curr_pressure < 980)
        return 17;
      else if (curr_pressure < 970)
        return 17;
    }
    else
      //winter
    {
      if (curr_pressure >= 1030)
        return 1;
      else if (curr_pressure >= 1020 and curr_pressure < 1030)
        return 2;
      else if (curr_pressure >= 1010 and curr_pressure < 1020)
        return 6;
      else if (curr_pressure >= 1000 and curr_pressure < 1010)
        return 7;
      else if (curr_pressure >= 990 and curr_pressure < 1000)
        return 10;
      else if (curr_pressure >= 980 and curr_pressure < 990)
        return 13;
      else if (curr_pressure >= 970 and curr_pressure < 980)
        return 17;
      else if (curr_pressure < 970)
        return 17;
    }
  }
  else {
    if (curr_pressure >= 1030)
      return 1;
    else if (curr_pressure >= 1020 and curr_pressure < 1030)
      return 2;
    else if (curr_pressure >= 1010 and curr_pressure < 1020)
      return 11;
    else if (curr_pressure >= 1000 and curr_pressure < 1010)
      return 14;
    else if (curr_pressure >= 990 and curr_pressure < 1000)
      return 19;
    else if (curr_pressure >= 980 and curr_pressure < 990)
      return 23;
    else if (curr_pressure >= 970 and curr_pressure < 980)
      return 24;
    else if (curr_pressure < 970)
      return 26;

  }
}

int station2sealevel(int presbme, int heightbme, int tempbme) {  // from pressure at our height to sea level
  return (double) presbme * pow(1 - 0.0065 * (double)heightbme / (tempbme + 0.0065 * (double)heightbme + 273.15), -5.275);
}
