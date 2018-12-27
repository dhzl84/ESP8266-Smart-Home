#ifndef CTHERMOSTAT_H_
#define CTHERMOSTAT_H_

#include "Arduino.h"

#ifndef minTargetTemp
  #define minTargetTemp (int16_t)150  // 15.0 °C
#endif

#ifndef maxTargetTemp
  #define maxTargetTemp (int16_t)250  // 25.0 °C
#endif

#ifndef maxHysteresis
  #define maxHysteresis (int16_t)20   // 2.0 °C
#endif

#ifndef minHysteresis
  #define minHysteresis (int16_t)2    // 0.2 °C
#endif

#ifndef CFG_MEDIAN_QUEUE_SIZE
  #define CFG_MEDIAN_QUEUE_SIZE 10
#endif

#define TH_HEAT true
#define TH_OFF  false

class Thermostat {
 public:
  Thermostat(void);
  ~Thermostat(void);
  void init(void);
  void setup(uint8_t gpio, uint8_t tarTemp, int16_t calibFactor, int16_t calibOffset, int16_t tHyst);
  void loop(void);
  // heating
  void setActualState(bool value);
  void setTargetTemperature(int16_t value);
  void increaseTargetTemperature(uint16_t value);
  void decreaseTargetTemperature(uint16_t value);
  void resetTransmitRequest();
  void resetNewData();
  bool getActualState(void);
  bool getNewData();
  bool getThermostatMode();
  void setThermostatMode(bool value);
  void toggleThermostatMode();
  int16_t getTargetTemperature(void);

  // sensor
  void setCurrentTemperature(int16_t temperature);
  void setCurrentHumidity(int16_t value);
  void setLastSensorReadFailed(bool value);
  void setThermostatHysteresis(int16_t hysteresis);
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
  int16_t getThermostatHysteresis(void);
  int16_t getThermostatHysteresisHigh(void);
  int16_t getThermostatHysteresisLow(void);

 private:
  // heating
  bool newData;
  bool thermostatMode;
  bool actualState;
  uint8_t targetTemperature;
  uint8_t relayGpio;
  int16_t thermostatHysteresis;
  int16_t thermostatHysteresisHigh;
  int16_t thermostatHysteresisLow;
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
  int16_t tempValueQueue[CFG_MEDIAN_QUEUE_SIZE];
  int16_t humidValueSampleID;
  int16_t humidValueQueue[CFG_MEDIAN_QUEUE_SIZE];
};

#endif  // CTHERMOSTAT_H_
