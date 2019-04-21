#ifndef CSENSOR_H_
#define CSENSOR_H_

#include "Arduino.h"

#ifndef CFG_TEMP_SENSOR_FILTER_QUEUE_SIZE
  #define CFG_TEMP_SENSOR_FILTER_QUEUE_SIZE 10
#endif

class Sensor {
 public:
  Sensor(void);
  ~Sensor(void);
  void init(void);
  void setup(int16_t calibFactor, int16_t calibOffset);
  void loop(void);
  // heating
  void resetNewData();
  bool getNewData();

  // sensor
  void setCurrentTemperature(int16_t temperature);
  void setCurrentHumidity(int16_t value);
  void setLastSensorReadFailed(bool value);
  void resetNewCalib();
  void setSensorCalibData(int16_t factor, int16_t offset, bool calib);
  bool getSensorError(void);
  bool getNewCalib();
  int16_t getSensorFailureCounter(void);
  int16_t getCurrentTemperature(void);
  int16_t getCurrentHumidity(void);
  int16_t getFilteredTemperature(void);
  int16_t getFilteredHumidity(void);
  int16_t getSensorCalibOffset(void);
  int16_t getSensorCalibFactor(void);

 private:
  // heating
  bool newData;
  // sensor
  bool sensorError;
  bool newCalib;
  int16_t currentTemperature;
  int16_t currentHumidity;
  int16_t filteredTemperature;
  int16_t filteredHumidity;
  int16_t sensorFailureCounter;
  int16_t tempOffset;
  int16_t tempFactor;
  const int16_t sensorErrorThreshold = 3;
  // filter
  bool tempValueQueueFilled;
  bool humidValueQueueFilled;
  int16_t tempValueSampleID;
  int16_t tempValueQueue[CFG_TEMP_SENSOR_FILTER_QUEUE_SIZE];
  int16_t humidValueSampleID;
  int16_t humidValueQueue[CFG_TEMP_SENSOR_FILTER_QUEUE_SIZE];
};

#endif  // CSENSOR_H_
