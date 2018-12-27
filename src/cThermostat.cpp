#include "cThermostat.h"

Thermostat::Thermostat() {
  Thermostat::init();
}

Thermostat::~Thermostat() {
  /* do nothing */
}

void Thermostat::init() {
  // heating
  thermostatMode             = TH_HEAT;
  actualState                = TH_OFF;
  targetTemperature          = 200;      // initial value if setup() is not called; resolution is 0.1 °C
  newData                    = true;     // flag to indicate new data to be displayed and transmitted vai MQTT
  relayGpio                  = 16;       // relay GPIO if setup() is not called
  thermostatHysteresis       = 2;        // hysteresis initialized to 0.2 °C
  thermostatHysteresisHigh   = 1;
  thermostatHysteresisLow    = 1;
  // sensor
  sensorError                = false;    // filtered sensor error
  currentTemperature         = 0;        // current sensor temperature
  currentHumidity            = 0;        // current sensor humidity
  filteredTemperature        = 0;        // filtered sensor temperature; median of CFG_MEDIAN_QUEUE_SIZE samples
  filteredHumidity           = 0;        // filtered sensor humidity; median of CFG_MEDIAN_QUEUE_SIZE samples
  sensorFailureCounter       = 0;        // count sensor failures during read
  tempOffset                 = 0;        // offset in 0.1 *C
  tempFactor                 = 100;      // factor in percent
  // temperture filter
  tempValueQueueFilled       = false;
  tempValueSampleID          = 0;

  for (int16_t i=0; i < CFG_MEDIAN_QUEUE_SIZE; i++) {
    tempValueQueue[i] = (int16_t)0;
  }

  // humidity filter
  humidValueQueueFilled      = false;
  humidValueSampleID         = 0;

  for (int16_t i=0; i < CFG_MEDIAN_QUEUE_SIZE; i++) {
    humidValueQueue[i] = (int16_t)0;
  }
}

void Thermostat::setup(uint8_t gpio, uint8_t tarTemp, int16_t calibFactor, int16_t calibOffset, int16_t tHyst) {
  setSensorCalibData(calibFactor, calibOffset, false);
  setThermostatHysteresis(tHyst);

  /* limit target temperature range */
  if (tarTemp < minTargetTemp) {
    targetTemperature = minTargetTemp;
  } else if (tarTemp > maxTargetTemp) {
    targetTemperature = maxTargetTemp;
  } else {
    targetTemperature = tarTemp;
  }

  relayGpio = gpio;    /* assign GPIO */
  pinMode(relayGpio, OUTPUT);   /* configure GPIO */
  digitalWrite(relayGpio, HIGH); /* switch relay OFF */
}

void Thermostat::loop(void) {
  if (getSensorError()) {
    /* switch off heating if sensor does not provide values */
    #ifdef CFG_DEBUG
    Serial.println("not heating, sensor data invalid");
    #endif
    if (getActualState() == TH_HEAT) {
      setActualState(TH_OFF);
    }
  } else {  /* sensor is healthy */
    if (getThermostatMode() == TH_HEAT) {  /* check if heating is allowed by user */
      if (getFilteredTemperature() <= (getTargetTemperature() - getThermostatHysteresisLow() )) {  /* check if measured temperature is lower than heating target */
        if (getActualState() == TH_OFF) {  /* switch on heating if target temperature is higher than measured temperature */
          setActualState(TH_HEAT);
          #ifdef CFG_DEBUG
          Serial.println("heating");
          #endif
        }
      } else if (getFilteredTemperature() >= (getTargetTemperature() + getThermostatHysteresisHigh() )) {  /* check if measured temperature is higher than heating target */
        if (getActualState() == TH_HEAT) {  /* switch off heating if target temperature is lower than measured temperature */
          setActualState(TH_OFF);
          #ifdef CFG_DEBUG
          Serial.println("not heating");
          #endif
        }
      } else {
        /* remain in current heating state if temperatures are equal */
      }
    } else {
      /* disable heating if heating is set to not allowed by user */
      setActualState(TH_OFF);
    }
  }
}

bool    Thermostat::getActualState(void)              { return actualState; }
int16_t Thermostat::getTargetTemperature(void)        { return targetTemperature; }
bool    Thermostat::getNewData()                      { return newData; }
bool    Thermostat::getThermostatMode()               { return thermostatMode; }
int16_t Thermostat::getSensorFailureCounter(void)     { return sensorFailureCounter; }
int16_t Thermostat::getCurrentTemperature(void)       { return currentTemperature; }
int16_t Thermostat::getCurrentHumidity(void)          { return currentHumidity; }
int16_t Thermostat::getFilteredTemperature(void)      { return filteredTemperature; }
int16_t Thermostat::getFilteredHumidity(void)         { return filteredHumidity; }
bool    Thermostat::getSensorError(void)              { return sensorError; }
bool    Thermostat::getNewCalib(void)                 { return newCalib; }
int16_t Thermostat::getSensorCalibOffset(void)        { return tempOffset; }
int16_t Thermostat::getSensorCalibFactor(void)        { return tempFactor; }
int16_t Thermostat::getThermostatHysteresis(void)     { return thermostatHysteresis; }
int16_t Thermostat::getThermostatHysteresisHigh(void) { return thermostatHysteresisHigh; }
int16_t Thermostat::getThermostatHysteresisLow(void)  { return thermostatHysteresisLow; }

void Thermostat::setThermostatHysteresis(int16_t hysteresis) {
  /*
  if hysteresis can not be applied symmetrically due to the internal resolution of 0.1 °C, round up (ceil) hysteresis high and round down (floor) hysteresis low
  example:
    thermostatHysteresis of 0.5 would need a resultion of 0.05 °C to accomplis +/- 0.25 °C
  result:
    hysteresis high will be 0.3
    hysteresis low will be 0.2
  */
  if (hysteresis > maxHysteresis) {
    hysteresis = maxHysteresis;
  }
  if (hysteresis < minHysteresis) {
    hysteresis = minHysteresis;
  }
  if (hysteresis != thermostatHysteresis) {
    newData = true;
    thermostatHysteresis = hysteresis;

    if (hysteresis % 2 == 0) {
      thermostatHysteresisLow  = (hysteresis / 2);
      thermostatHysteresisHigh = (hysteresis / 2);
    } else {
      thermostatHysteresisLow  = ( (int16_t)(floorf( static_cast<float> (hysteresis) / 2) ) );
      thermostatHysteresisHigh = ( (int16_t)(ceilf( static_cast<float> (hysteresis) / 2) ) );
    }
  }
}

void Thermostat::resetNewCalib() { newCalib  = false; }

void Thermostat::setActualState(bool value) {
  if (value != actualState) {
    newData = true;
    if (value == TH_HEAT) {
        digitalWrite(relayGpio, LOW); /* switch relay ON */
    } else {
        digitalWrite(relayGpio, HIGH); /* switch relay OFF */
    }
  }
  actualState  = value;
}

void Thermostat::resetNewData() {
  newData  = false;
}

void Thermostat::increaseTargetTemperature(uint16_t value) {
  if ((targetTemperature + value) <= maxTargetTemp) {
    targetTemperature += value;
    newData = true;
  }
}

void Thermostat::decreaseTargetTemperature(uint16_t value) {
  if ((targetTemperature - value) >= minTargetTemp) {
    targetTemperature -= value;
    newData = true;
  }
}

void Thermostat::setTargetTemperature(int16_t value) {
  if (value > maxTargetTemp) {
    value = maxTargetTemp;
  }
  if (value < minTargetTemp) {
    value = minTargetTemp;
  }
  if (value != targetTemperature) {
    targetTemperature  = value;
    newData = true;
  }
}

void Thermostat::setThermostatMode(bool value) {
  if (value != thermostatMode) {
    newData = true;
  }
  thermostatMode = value;
}

void Thermostat::toggleThermostatMode() {
  if (thermostatMode == TH_OFF) {
    thermostatMode  = TH_HEAT;
  } else {
    thermostatMode = TH_OFF;
  }
  newData = true;
}

// Sensor ===============================================================

void Thermostat::setCurrentTemperature(int16_t value) {
  currentTemperature = ((int16_t)( static_cast<float> (value) * (static_cast<float> (tempFactor) / 100)) + tempOffset);

  // add new temperature to filter
  tempValueQueue[tempValueSampleID] = currentTemperature;
  tempValueSampleID++;
  if (tempValueSampleID == CFG_MEDIAN_QUEUE_SIZE) {
    tempValueSampleID = 0;
    if (tempValueQueueFilled == false) {
      tempValueQueueFilled = true;
    }
  }

  // calculate new filtered temeprature
  float tempValue = (int16_t) 0;
  if (tempValueQueueFilled == true) {
    for (int16_t i=0; i < CFG_MEDIAN_QUEUE_SIZE; i++) {
      tempValue += (tempValueQueue[i]);
    }
    tempValue = (tempValue / (int16_t) (CFG_MEDIAN_QUEUE_SIZE));
  } else {  /* return partially filtered value until queue is filled */
    if (tempValueSampleID > 0) {
      for (int16_t i=0; i < tempValueSampleID; i++) {
        tempValue += (tempValueQueue[i]);
      }
      tempValue = (tempValue / tempValueSampleID);
    }
  }
  filteredTemperature = tempValue;
}

void Thermostat::setCurrentHumidity(int16_t value) {
  currentHumidity = value;

  // add new humdity to filter
  humidValueQueue[humidValueSampleID] = value;
  humidValueSampleID++;
  if (humidValueSampleID == CFG_MEDIAN_QUEUE_SIZE) {
    humidValueSampleID = 0;
    if (humidValueQueueFilled == false) {
      humidValueQueueFilled = true;
    }
  }

  // calculate new filtered temeprature
  float humidValue = (int16_t) 0;
  if (humidValueQueueFilled == true) {
    for (int16_t i=0; i < CFG_MEDIAN_QUEUE_SIZE; i++) {
      humidValue += (humidValueQueue[i]);
    }
    humidValue = (humidValue / (int16_t) (CFG_MEDIAN_QUEUE_SIZE));
  } else {  /* return partially filtered value until queue is filled */
    if (humidValueSampleID > 0) {
      for (int16_t i=0; i < humidValueSampleID; i++) {
        humidValue += (humidValueQueue[i]);
      }
      humidValue = (humidValue / humidValueSampleID);
    }
  }
  filteredHumidity = humidValue;
}

void Thermostat::setLastSensorReadFailed(bool value) {
  /* filter sensor read failure here to avoid switching back and forth for single failure events */
  if (value == true) {
    sensorFailureCounter++;
  } else {
    if (sensorFailureCounter > 0) {
      sensorFailureCounter--;
    }
  }

  if (sensorError == false) {
    if (sensorFailureCounter >= sensorErrorThreshold) {
      newData = true;
      sensorError = true;
    }
  } else {
    if (sensorFailureCounter == 0) {
      newData = true;
      sensorError = false;
    }
  }
}

void Thermostat::setSensorCalibData(int16_t factor, int16_t offset, bool calib) {
  if (tempOffset != offset) {
    tempOffset = offset;
    newCalib   = calib;
  }
  if (tempFactor != factor) {
    if (factor != 0) {
      tempFactor = factor;
      newCalib   = calib;
    }
  }
}
