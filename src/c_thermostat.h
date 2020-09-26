#ifndef C_THERMOSTAT_H_
#define C_THERMOSTAT_H_

#include "Arduino.h"

#ifndef MINIMUM_TARGET_TEMP
  #define MINIMUM_TARGET_TEMP (uint8_t)150  // 15.0 °C
#endif

#ifndef MAXIMUM_TARGET_TEMP
  #define MAXIMUM_TARGET_TEMP (uint8_t)250  // 25.0 °C
#endif

#ifndef MAXIMUM_HYSTERESIS
  #define MAXIMUM_HYSTERESIS (uint8_t)20   // 2.0 °C
#endif

#ifndef MINIMUM_HYSTERESIS
  #define MINIMUM_HYSTERESIS (uint8_t)2    // 0.2 °C
#endif

#ifndef CFG_TEMP_SENSOR_FILTER_QUEUE_SIZE
  #define CFG_TEMP_SENSOR_FILTER_QUEUE_SIZE 10
#endif

#define TH_HEAT true
#define TH_OFF  false

#define SENSOR_FAILURE_COUNTER_INIT_VALUE -1

class Thermostat {
 public:
  Thermostat(void);
  ~Thermostat(void);
  void setup(uint8_t gpio, uint8_t tarTemp, int16_t calibFactor, int16_t calibOffset, uint8_t temperature_hysteresis, bool thermostat_mode);
  void loop(void);
  // heating
  void setActualState(bool value);
  void setTargetTemperature(uint8_t value);
  void increaseTargetTemperature(uint8_t value);
  void decreaseTargetTemperature(uint8_t value);
  void resetTransmitRequest();
  void resetNewData();
  bool getActualState(void);
  bool getNewData();
  bool getThermostatMode();
  void setThermostatMode(bool value);
  void toggleThermostatMode();
  uint8_t getTargetTemperature(void);

  // sensor
  void setCurrentTemperature(int16_t value);
  void setCurrentHumidity(int16_t value);
  void setLastSensorReadFailed(bool value);
  void setThermostatHysteresis(uint8_t hysteresis);
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
  uint8_t getThermostatHysteresis(void);
  uint8_t getThermostatHysteresisHigh(void);
  uint8_t getThermostatHysteresisLow(void);

  // outside temperature
  void setOutsideTemperature(int16_t value);
  int16_t getOutsideTemperature(void);
  bool    getOutsideTemperatureReceived(void);

 private:
  // heating
  bool new_data_;
  bool thermostat_mode_;
  bool actual_state_;
  uint8_t target_temperature_;
  uint8_t relay_gpio_;
  uint8_t thermostat_hysteresis_;
  uint8_t thermostat_hysteresis_high_;
  uint8_t thermostat_hysteresis_low_;
  // sensor
  bool sensor_error_;
  bool new_calib_;
  int16_t current_temperature_;
  int16_t current_humidity_;
  int16_t filtered_temperature_;
  int16_t filtered_humidity_;
  int8_t  sensor_failure_counter_;
  int16_t temperature_offset_;
  int16_t temperature_factor_;
  uint8_t sensor_error_threshold_;
  // outside temperature
  int16_t outside_temperature_;
  bool    outside_temperature_received_;
  // filter
  bool temperature_value_queue_filled_;
  bool humidity_value_queue_filled_;
  int16_t temperature_value_sample_id_;
  int16_t temperature_value_queue_[CFG_TEMP_SENSOR_FILTER_QUEUE_SIZE];
  int16_t humidity_value_sample_id_;
  int16_t humidity_value_queue_[CFG_TEMP_SENSOR_FILTER_QUEUE_SIZE];
};

#endif  // C_THERMOSTAT_H_
