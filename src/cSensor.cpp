#include "cSensor.h"

Sensor::Sensor() {
  Sensor::init();
}

Sensor::~Sensor() {
  /* do nothing */
}

void Sensor::init() {
  newData                    = true;     // flag to indicate new data to be displayed and transmitted vai MQTT
  // sensor
  sensorError                = false;    // filtered sensor error
  currentTemperature         = 0;        // current sensor temperature
  currentHumidity            = 0;        // current sensor humidity
  filteredTemperature        = 0;        // filtered sensor temperature; mean value of CFG_TEMP_SENSOR_FILTER_QUEUE_SIZE samples
  filteredHumidity           = 0;        // filtered sensor humidity; mean value of CFG_TEMP_SENSOR_FILTER_QUEUE_SIZE samples
  sensorFailureCounter       = 0;        // count sensor failures during read
  tempOffset                 = 0;        // offset in 0.1 *C
  tempFactor                 = 100;      // factor in percent
  // temperture filter
  tempValueQueueFilled       = false;
  tempValueSampleID          = 0;

  for (int16_t i=0; i < CFG_TEMP_SENSOR_FILTER_QUEUE_SIZE; i++) {
    tempValueQueue[i] = (int16_t)0;
  }

  // humidity filter
  humidValueQueueFilled      = false;
  humidValueSampleID         = 0;

  for (int16_t i=0; i < CFG_TEMP_SENSOR_FILTER_QUEUE_SIZE; i++) {
    humidValueQueue[i] = (int16_t)0;
  }
}

void Sensor::setup(int16_t calibFactor, int16_t calibOffset) {
  setSensorCalibData(calibFactor, calibOffset, false);
}
bool    Sensor::getNewData()                      { return newData; }
int16_t Sensor::getSensorFailureCounter(void)     { return sensorFailureCounter; }
int16_t Sensor::getCurrentTemperature(void)       { return currentTemperature; }
int16_t Sensor::getCurrentHumidity(void)          { return currentHumidity; }
int16_t Sensor::getFilteredTemperature(void)      { return filteredTemperature; }
int16_t Sensor::getFilteredHumidity(void)         { return filteredHumidity; }
bool    Sensor::getSensorError(void)              { return sensorError; }
bool    Sensor::getNewCalib(void)                 { return newCalib; }
int16_t Sensor::getSensorCalibOffset(void)        { return tempOffset; }
int16_t Sensor::getSensorCalibFactor(void)        { return tempFactor; }

void Sensor::resetNewCalib() { newCalib  = false; }

void Sensor::resetNewData() { newData  = false; }

// Sensor ===============================================================

void Sensor::setCurrentTemperature(int16_t value) {
  currentTemperature = ((int16_t)( static_cast<float> (value) * (static_cast<float> (tempFactor) / 100)) + tempOffset);

  // add new temperature to filter
  tempValueQueue[tempValueSampleID] = currentTemperature;
  tempValueSampleID++;
  if (tempValueSampleID == CFG_TEMP_SENSOR_FILTER_QUEUE_SIZE) {
    tempValueSampleID = 0;
    if (tempValueQueueFilled == false) {
      tempValueQueueFilled = true;
    }
  }

  // calculate new filtered temeprature
  float tempValue = (int16_t) 0;
  if (tempValueQueueFilled == true) {
    for (int16_t i=0; i < CFG_TEMP_SENSOR_FILTER_QUEUE_SIZE; i++) {
      tempValue += (tempValueQueue[i]);
    }
    tempValue = (tempValue / (int16_t) (CFG_TEMP_SENSOR_FILTER_QUEUE_SIZE));
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

void Sensor::setCurrentHumidity(int16_t value) {
  currentHumidity = value;

  // add new humdity to filter
  humidValueQueue[humidValueSampleID] = value;
  humidValueSampleID++;
  if (humidValueSampleID == CFG_TEMP_SENSOR_FILTER_QUEUE_SIZE) {
    humidValueSampleID = 0;
    if (humidValueQueueFilled == false) {
      humidValueQueueFilled = true;
    }
  }

  // calculate new filtered temeprature
  float humidValue = (int16_t) 0;
  if (humidValueQueueFilled == true) {
    for (int16_t i=0; i < CFG_TEMP_SENSOR_FILTER_QUEUE_SIZE; i++) {
      humidValue += (humidValueQueue[i]);
    }
    humidValue = (humidValue / (int16_t) (CFG_TEMP_SENSOR_FILTER_QUEUE_SIZE));
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

void Sensor::setLastSensorReadFailed(bool value) {
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

void Sensor::setSensorCalibData(int16_t factor, int16_t offset, bool calib) {
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
