#include "cThermostat.h"

Thermostat::Thermostat() {
  // heating
  this->thermostatMode             = TH_HEAT;
  this->actualState                = TH_OFF;
  this->targetTemperature          = 200;      // initial value if setup() is not called; resolution is 0.1 °C
  this->newData                    = true;     // flag to indicate new data to be displayed and transmitted vai MQTT
  this->relayGpio                  = 16;       // relay GPIO if setup() is not called
  this->thermostatHysteresis       = 2;        // hysteresis initialized to 0.2 °C
  this->thermostatHysteresisHigh   = 1;
  this->thermostatHysteresisLow    = 1;
  // sensor
  this->sensorError                = false;    // filtered sensor error
  this->currentTemperature         = 0;        // current sensor temperature
  this->currentHumidity            = 0;        // current sensor humidity
  this->filteredTemperature        = 0;        // filtered sensor temperature; mean value of CFG_TEMP_SENSOR_FILTER_QUEUE_SIZE samples
  this->filteredHumidity           = 0;        // filtered sensor humidity; mean value of CFG_TEMP_SENSOR_FILTER_QUEUE_SIZE samples
  this->sensorFailureCounter       = 0;        // count sensor failures during read
  this->tempOffset                 = 0;        // offset in 0.1 *C
  this->tempFactor                 = 100;      // factor in percent
  // temperture filter
  this->tempValueQueueFilled       = false;
  this->tempValueSampleID          = 0;

  for (int16_t i=0; i < CFG_TEMP_SENSOR_FILTER_QUEUE_SIZE; i++) {
    this->tempValueQueue[i] = (int16_t)0;
  }

  // humidity filter
  this->humidValueQueueFilled      = false;
  this->humidValueSampleID         = 0;

  for (int16_t i=0; i < CFG_TEMP_SENSOR_FILTER_QUEUE_SIZE; i++) {
    this->humidValueQueue[i] = (int16_t)0;
  }
}

Thermostat::~Thermostat() {
  /* do nothing */
}

void Thermostat::setup(uint8_t gpio, uint8_t tarTemp, int16_t calibFactor, int16_t calibOffset, int16_t tHyst, boolean mode) {
  setSensorCalibData(calibFactor, calibOffset, false);
  setThermostatHysteresis(tHyst);
  setThermostatMode(mode);

  /* limit target temperature range */
  if (tarTemp < minTargetTemp) {
    this->targetTemperature = minTargetTemp;
  } else if (tarTemp > maxTargetTemp) {
    this->targetTemperature = maxTargetTemp;
  } else {
    this->targetTemperature = tarTemp;
  }

  this->relayGpio = gpio;    /* assign GPIO */
  pinMode(this->relayGpio, OUTPUT);   /* configure GPIO */
  digitalWrite(this->relayGpio, HIGH); /* switch relay OFF */
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

bool    Thermostat::getActualState(void)              { return this->actualState; }
int16_t Thermostat::getTargetTemperature(void)        { return this->targetTemperature; }
bool    Thermostat::getNewData()                      { return this->newData; }
bool    Thermostat::getThermostatMode()               { return this->thermostatMode; }
int16_t Thermostat::getSensorFailureCounter(void)     { return this->sensorFailureCounter; }
int16_t Thermostat::getCurrentTemperature(void)       { return this->currentTemperature; }
int16_t Thermostat::getCurrentHumidity(void)          { return this->currentHumidity; }
int16_t Thermostat::getFilteredTemperature(void)      { return this->filteredTemperature; }
int16_t Thermostat::getFilteredHumidity(void)         { return this->filteredHumidity; }
bool    Thermostat::getSensorError(void)              { return this->sensorError; }
bool    Thermostat::getNewCalib(void)                 { return this->newCalib; }
int16_t Thermostat::getSensorCalibOffset(void)        { return this->tempOffset; }
int16_t Thermostat::getSensorCalibFactor(void)        { return this->tempFactor; }
int16_t Thermostat::getThermostatHysteresis(void)     { return this->thermostatHysteresis; }
int16_t Thermostat::getThermostatHysteresisHigh(void) { return this->thermostatHysteresisHigh; }
int16_t Thermostat::getThermostatHysteresisLow(void)  { return this->thermostatHysteresisLow; }

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
  if (hysteresis != this->thermostatHysteresis) {
    this->newData = true;
    this->thermostatHysteresis = hysteresis;

    if (hysteresis % 2 == 0) {
      this->thermostatHysteresisLow  = (hysteresis / 2);
      this->thermostatHysteresisHigh = (hysteresis / 2);
    } else {
      this->thermostatHysteresisLow  = ( (int16_t)(floorf( static_cast<float> (hysteresis) / 2) ) );
      this->thermostatHysteresisHigh = ( (int16_t)(ceilf( static_cast<float> (hysteresis) / 2) ) );
    }
  }
}

void Thermostat::resetNewCalib() { this->newCalib  = false; }

void Thermostat::setActualState(bool value) {
  if (value != this->actualState) {
    this->newData = true;
    if (value == TH_HEAT) {
        digitalWrite(this->relayGpio, LOW); /* switch relay ON */
    } else {
        digitalWrite(this->relayGpio, HIGH); /* switch relay OFF */
    }
  }
  this->actualState  = value;
}

void Thermostat::resetNewData() {
  this->newData  = false;
}

void Thermostat::increaseTargetTemperature(uint16_t value) {
  if ((this->targetTemperature + value) <= maxTargetTemp) {
    this->targetTemperature += value;
    this->newData = true;
  }
}

void Thermostat::decreaseTargetTemperature(uint16_t value) {
  if ((this->targetTemperature - value) >= minTargetTemp) {
    this->targetTemperature -= value;
    this->newData = true;
  }
}

void Thermostat::setTargetTemperature(int16_t value) {
  if (value > maxTargetTemp) {
    value = maxTargetTemp;
  }
  if (value < minTargetTemp) {
    value = minTargetTemp;
  }
  if (value != this->targetTemperature) {
    this->targetTemperature  = value;
    this->newData = true;
  }
}

void Thermostat::setThermostatMode(bool value) {
  if (value != this->thermostatMode) {
    this->newData = true;
    this->thermostatMode = value;
  }
}

void Thermostat::toggleThermostatMode() {
  if (this->thermostatMode == TH_OFF) {
    this->thermostatMode  = TH_HEAT;
  } else {
    this->thermostatMode = TH_OFF;
  }
  this->newData = true;
}

// Sensor ===============================================================

void Thermostat::setCurrentTemperature(int16_t value) {
  this->currentTemperature = ((int16_t)( static_cast<float> (value) * (static_cast<float> (this->tempFactor) / 100)) + this->tempOffset);

  // add new temperature to filter
  this->tempValueQueue[this->tempValueSampleID] = this->currentTemperature;
  this->tempValueSampleID++;
  if (this->tempValueSampleID == CFG_TEMP_SENSOR_FILTER_QUEUE_SIZE) {
    this->tempValueSampleID = 0;
    if (this->tempValueQueueFilled == false) {
      this->tempValueQueueFilled = true;
    }
  }

  // calculate new filtered temperature
  float tempValue = (int16_t) 0;
  if (this->tempValueQueueFilled == true) {
    for (int16_t i=0; i < CFG_TEMP_SENSOR_FILTER_QUEUE_SIZE; i++) {
      tempValue += (this->tempValueQueue[i]);
    }
    tempValue = (tempValue / (int16_t) (CFG_TEMP_SENSOR_FILTER_QUEUE_SIZE));
  } else {  /* return partially filtered value until queue is filled */
    if (this->tempValueSampleID > 0) {
      for (int16_t i=0; i < this->tempValueSampleID; i++) {
        tempValue += (this->tempValueQueue[i]);
      }
      tempValue = (tempValue / this->tempValueSampleID);
    }
  }
  this->filteredTemperature = tempValue;
}

void Thermostat::setCurrentHumidity(int16_t value) {
  this->currentHumidity = value;

  // add new humdity to filter
  this->humidValueQueue[this->humidValueSampleID] = value;
  this->humidValueSampleID++;
  if (this->humidValueSampleID == CFG_TEMP_SENSOR_FILTER_QUEUE_SIZE) {
    this->humidValueSampleID = 0;
    if (this->humidValueQueueFilled == false) {
      this->humidValueQueueFilled = true;
    }
  }

  // calculate new filtered temeprature
  float humidValue = (int16_t) 0;
  if (this->humidValueQueueFilled == true) {
    for (int16_t i=0; i < CFG_TEMP_SENSOR_FILTER_QUEUE_SIZE; i++) {
      humidValue += (this->humidValueQueue[i]);
    }
    humidValue = (humidValue / (int16_t) (CFG_TEMP_SENSOR_FILTER_QUEUE_SIZE));
  } else {  /* return partially filtered value until queue is filled */
    if (this->humidValueSampleID > 0) {
      for (int16_t i=0; i < this->humidValueSampleID; i++) {
        humidValue += (this->humidValueQueue[i]);
      }
      humidValue = (humidValue / this->humidValueSampleID);
    }
  }
  this->filteredHumidity = humidValue;
}

void Thermostat::setLastSensorReadFailed(bool value) {
  /* filter sensor read failure here to avoid switching back and forth for single failure events */
  if (value == true) {
    this->sensorFailureCounter++;
  } else {
    if (this->sensorFailureCounter > 0) {
      this->sensorFailureCounter--;
    }
  }

  if (this->sensorError == false) {
    if (this->sensorFailureCounter >= this->sensorErrorThreshold) {
      this->newData = true;
      this->sensorError = true;
    }
  } else {
    if (this->sensorFailureCounter == 0) {
      this->newData = true;
      this->sensorError = false;
    }
  }
}

void Thermostat::setSensorCalibData(int16_t factor, int16_t offset, bool calib) {
  if (this->tempOffset != offset) {
    this->tempOffset = offset;
    this->newCalib   = calib;
  }
  if (this->tempFactor != factor) {
    if (factor != 0) {
      this->tempFactor = factor;
      this->newCalib   = calib;
    }
  }
}
