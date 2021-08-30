#include "c_thermostat.h"

/* target_temperature_ : initial value if setup() is not called; resolution is 0.1 °C */
/* new_data_ : flag to indicate new data to be displayed and transmitted vai MQTT */
/* relay_gpio_ : relay GPIO if setup() is not called */
/* thermostat_hysteresis_ : hysteresis initialized to 0.2 °C */
/* sensor_error_ : filtered sensor error */
/* current_temperature_ : current sensor temperature */
/* current_humidity_ : current sensor humidity */
/* filtered_temperature_ : filtered sensor temperature; mean value of CFG_TEMP_SENSOR_FILTER_QUEUE_SIZE samples */
/* filtered_humidity_ : filtered sensor humidity; mean value of CFG_TEMP_SENSOR_FILTER_QUEUE_SIZE samples */
/* sensor_failure_counter_ : count sensor failures during read */
/* temperature_offset_ : offset in 0.1 *C */
/* temperature_factor_ : factor in percent */

Thermostat::Thermostat()
  : new_data_(true), \
    thermostat_mode_(TH_HEAT), \
    actual_state_(TH_OFF), \
    target_temperature_(200), \
    relay_gpio_(16), \
    thermostat_hysteresis_(4), \
    thermostat_hysteresis_high_(2), \
    thermostat_hysteresis_low_(2), \
    sensor_error_(false), \
    new_calib_(0), \
    current_temperature_(0), \
    current_humidity_(0), \
    filtered_temperature_(0), \
    filtered_humidity_(0), \
    start_averaging_(2), \
    stop_averaging_(CFG_TEMP_SENSOR_FILTER_QUEUE_SIZE - 3), \
    filter_size_(stop_averaging_ - start_averaging_), \
    sensor_failure_counter_(SENSOR_FAILURE_COUNTER_INIT_VALUE), \
    temperature_offset_(0), \
    temperature_factor_(100), \
    sensor_error_threshold_(3), \
    outside_temperature_(0), \
    outside_temperature_received_(false), \
    temperature_value_queue_filled_(false), \
    humidity_value_queue_filled_(false), \
    temperature_value_sample_id_(0), \
    humidity_value_sample_id_(0) {
  for (int16_t i=0; i < CFG_TEMP_SENSOR_FILTER_QUEUE_SIZE; i++) {
    temperature_value_queue_[i] = (int16_t)0;
  }

  for (int16_t i=0; i < CFG_TEMP_SENSOR_FILTER_QUEUE_SIZE; i++) {
    humidity_value_queue_[i] = (int16_t)0;
  }
}

Thermostat::~Thermostat() {
  /* do nothing */
}

void Thermostat::setup(uint8_t gpio, uint8_t tarTemp, int16_t calibFactor, int16_t calibOffset, uint8_t temperature_hysteresis, bool thermostat_mode) {
  setSensorCalibData(calibFactor, calibOffset, false);
  setThermostatHysteresis(temperature_hysteresis);
  setThermostatMode(thermostat_mode);

  /* limit target temperature range */
  if (tarTemp < MINIMUM_TARGET_TEMP) {
    target_temperature_ = MINIMUM_TARGET_TEMP;
  } else if (tarTemp > MAXIMUM_TARGET_TEMP) {
    target_temperature_ = MAXIMUM_TARGET_TEMP;
  } else {
    target_temperature_ = tarTemp;
  }

  relay_gpio_ = gpio;    /* assign GPIO */
  pinMode(relay_gpio_, OUTPUT);   /* configure GPIO */
  digitalWrite(relay_gpio_, HIGH); /* switch relay OFF */
}

void Thermostat::loop(void) {
  /* prevent heating if sensor has a confirmed error */
  if (sensor_error_ == true) {
    /* switch off heating if sensor does not provide values */
    if (actual_state_ == TH_HEAT) {
      setActualState(TH_OFF);
    }
  } else {  /* sensor is healthy */
    if (thermostat_mode_ == TH_HEAT) {  /* check if heating is allowed by user */
      if (filtered_temperature_ <= static_cast<int16_t>(target_temperature_ - thermostat_hysteresis_low_ )) {  /* check if measured temperature is lower than heating target */
        if (actual_state_ == TH_OFF) {  /* switch on heating if target temperature is higher than measured temperature */
          setActualState(TH_HEAT);
          #ifdef CFG_DEBUG
          Serial.println("heating");
          #endif
        }
      } else if (filtered_temperature_ >= static_cast<int16_t>(target_temperature_ + thermostat_hysteresis_high_)) {  /* check if measured temperature is higher than heating target */
        if (actual_state_ == TH_HEAT) {  /* switch off heating if target temperature is lower than measured temperature */
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

bool    Thermostat::getActualState(void)                  { return actual_state_; }
uint8_t Thermostat::getTargetTemperature(void)            { return target_temperature_; }
bool    Thermostat::getNewData()                          { return new_data_; }
bool    Thermostat::getThermostatMode()                   { return thermostat_mode_; }
int16_t Thermostat::getSensorFailureCounter(void)         { return sensor_failure_counter_; }
int16_t Thermostat::getCurrentTemperature(void)           { return current_temperature_; }
int16_t Thermostat::getCurrentHumidity(void)              { return current_humidity_; }
int16_t Thermostat::getFilteredTemperature(void)          { return filtered_temperature_; }
int16_t Thermostat::getFilteredHumidity(void)             { return filtered_humidity_; }
bool    Thermostat::getSensorError(void)                  { return sensor_error_; }
bool    Thermostat::getNewCalib(void)                     { return new_calib_; }
int16_t Thermostat::getSensorCalibOffset(void)            { return temperature_offset_; }
int16_t Thermostat::getSensorCalibFactor(void)            { return temperature_factor_; }
uint8_t Thermostat::getThermostatHysteresis(void)         { return thermostat_hysteresis_; }
uint8_t Thermostat::getThermostatHysteresisHigh(void)     { return thermostat_hysteresis_high_; }
uint8_t Thermostat::getThermostatHysteresisLow(void)      { return thermostat_hysteresis_low_; }
bool    Thermostat::getOutsideTemperatureReceived(void)   { return outside_temperature_received_; }

void Thermostat::setThermostatHysteresis(uint8_t hysteresis) {
  /*
  if hysteresis can not be applied symmetrically due to the internal resolution of 0.1 °C, round up (ceil) hysteresis high and round down (floor) hysteresis low
  example:
    thermostat_hysteresis_ of 0.5 would need a resultion of 0.05 °C to accomplis +/- 0.25 °C
  result:
    hysteresis high will be 0.3
    hysteresis low will be 0.2
  */
  if (hysteresis != thermostat_hysteresis_) {
    if (hysteresis > MAXIMUM_HYSTERESIS) {
      thermostat_hysteresis_ = MAXIMUM_HYSTERESIS;
    } else if (hysteresis < MINIMUM_HYSTERESIS) {
      thermostat_hysteresis_ = MINIMUM_HYSTERESIS;
    } else {
      thermostat_hysteresis_ = hysteresis;
    }
    new_data_ = true;

    if ((hysteresis % 2) == 0) {
      thermostat_hysteresis_low_  = (hysteresis / 2);
      thermostat_hysteresis_high_ = (hysteresis / 2);
    } else {
      thermostat_hysteresis_low_  = ( (uint8_t)(floorf( static_cast<float> (hysteresis) / 2) ) );
      thermostat_hysteresis_high_ = ( (uint8_t)(ceilf( static_cast<float> (hysteresis) / 2) ) );
    }
  }
}

void Thermostat::resetNewCalib() { new_calib_  = false; }

void Thermostat::setActualState(bool value) {
  if (value != actual_state_) {
    new_data_ = true;
    if (value == TH_HEAT) {
        digitalWrite(relay_gpio_, LOW); /* switch relay ON */
    } else {
        digitalWrite(relay_gpio_, HIGH); /* switch relay OFF */
    }
  }
  actual_state_  = value;
}

void Thermostat::resetNewData() {
  new_data_  = false;
}

void Thermostat::increaseTargetTemperature(uint8_t value) {
  if ((target_temperature_ + value) <= MAXIMUM_TARGET_TEMP) {
    target_temperature_ += value;
    new_data_ = true;
  }
}

void Thermostat::decreaseTargetTemperature(uint8_t value) {
  if ((target_temperature_ - value) >= MINIMUM_TARGET_TEMP) {
    target_temperature_ -= value;
    new_data_ = true;
  }
}

void Thermostat::setTargetTemperature(uint8_t value) {
  if (value != target_temperature_) {
    if (value > MAXIMUM_TARGET_TEMP) {
      target_temperature_ = MAXIMUM_TARGET_TEMP;
    } else if (value < MINIMUM_TARGET_TEMP) {
      target_temperature_ = MINIMUM_TARGET_TEMP;
    } else {
      target_temperature_  = value;
    }
    new_data_ = true;
  }
}

void Thermostat::setThermostatMode(bool value) {
  if (value != thermostat_mode_) {
    new_data_ = true;
    thermostat_mode_ = value;
  }
}

void Thermostat::toggleThermostatMode() {
  if (thermostat_mode_ == TH_OFF) {
    thermostat_mode_  = TH_HEAT;
  } else {
    thermostat_mode_ = TH_OFF;
  }
  new_data_ = true;
}

// Sensor ===============================================================

void Thermostat::setCurrentTemperature(int16_t value) {
  current_temperature_ = ((int16_t)( static_cast<float> (value) * (static_cast<float> (temperature_factor_) / 100)) + temperature_offset_);

  // add new temperature to filter
  temperature_value_queue_[temperature_value_sample_id_] = current_temperature_;
  temperature_value_sample_id_++;
  if (temperature_value_sample_id_ == CFG_TEMP_SENSOR_FILTER_QUEUE_SIZE) {
    temperature_value_sample_id_ = 0;
    if (temperature_value_queue_filled_ == false) {
      temperature_value_queue_filled_ = true;
    }
  }
  // copy array for sorting via qsort
  int16_t sorted_temperature_value_queue[CFG_TEMP_SENSOR_FILTER_QUEUE_SIZE];
  std::copy(std::begin(temperature_value_queue_), std::end(temperature_value_queue_), std::begin(sorted_temperature_value_queue));

  for (auto val : temperature_value_queue_) {
    Serial.printf("Value: %i\n", val);
  }

  // calculate new filtered temperature
  float tempValue = (int16_t) 0;
  if (temperature_value_queue_filled_ == true) {
    #if CFG_DEBUG
    uint32_t t_start = micros();
    #endif  // CFG_DEBUG
    quick_sort(sorted_temperature_value_queue, 0, CFG_TEMP_SENSOR_FILTER_QUEUE_SIZE-1);
    #if CFG_DEBUG
    uint32_t t_finish = micros();
    Serial.printf("Sorting took %i µs\n", t_finish - t_start);
    for (auto val : sorted_temperature_value_queue) {
      Serial.printf("Sorted Value: %i\n", val);
    }
    #endif  // CFG_DEBUG
    for (int16_t i=start_averaging_; i <= stop_averaging_; i++) {
      tempValue += (sorted_temperature_value_queue[i]);
      #if CFG_DEBUG
      Serial.printf("Use Value: %i from index %i \n", sorted_temperature_value_queue[i], i);
      #endif  // CFG_DEBUG
    }
    tempValue = (tempValue / (int16_t) (filter_size_));
  } else {  /* return partially filtered value until queue is filled */
    if (temperature_value_sample_id_ > 0) {
      for (int16_t i=0; i < temperature_value_sample_id_; i++) {
        tempValue += (temperature_value_queue_[i]);
      }
      tempValue = (tempValue / temperature_value_sample_id_);
    }
  }
  filtered_temperature_ = tempValue;
}

void Thermostat::setCurrentHumidity(int16_t value) {
  current_humidity_ = value;

  // add new humdity to filter
  humidity_value_queue_[humidity_value_sample_id_] = value;
  humidity_value_sample_id_++;
  if (humidity_value_sample_id_ == CFG_TEMP_SENSOR_FILTER_QUEUE_SIZE) {
    humidity_value_sample_id_ = 0;
    if (humidity_value_queue_filled_ == false) {
      humidity_value_queue_filled_ = true;
    }
  }

  // copy array for sorting via qsort
  int16_t sorted_humidity_value_queue[CFG_TEMP_SENSOR_FILTER_QUEUE_SIZE];
  std::copy(std::begin(humidity_value_queue_), std::end(humidity_value_queue_), std::begin(sorted_humidity_value_queue));

  // calculate new filtered temeprature
  float humidValue = (int16_t) 0;
  if (humidity_value_queue_filled_ == true) {
    quick_sort(sorted_humidity_value_queue, 0, CFG_TEMP_SENSOR_FILTER_QUEUE_SIZE-1);
    for (int16_t i=start_averaging_; i <= stop_averaging_; i++) {
      humidValue += (sorted_humidity_value_queue[i]);
    }
    humidValue = (humidValue / (int16_t) (filter_size_));
  } else {  /* return partially filtered value until queue is filled */
    if (humidity_value_sample_id_ > 0) {
      for (int16_t i=0; i < humidity_value_sample_id_; i++) {
        humidValue += (humidity_value_queue_[i]);
      }
      humidValue = (humidValue / humidity_value_sample_id_);
    }
  }
  filtered_humidity_ = humidValue;
}

void Thermostat::setLastSensorReadFailed(bool value) {
  /* filter sensor read failure here to avoid switching back and forth for single failure events */
  if (value == true) {
    if ( (sensor_failure_counter_ < INT8_MAX) && (sensor_failure_counter_ != SENSOR_FAILURE_COUNTER_INIT_VALUE) ) {
      sensor_failure_counter_++;
    }
  } else {
    if (sensor_failure_counter_ > 0) {
      sensor_failure_counter_--;
    } else if (sensor_failure_counter_ == SENSOR_FAILURE_COUNTER_INIT_VALUE) {
      sensor_failure_counter_ = 0;
    }
  }

  if (sensor_error_ == false) {
    if (sensor_failure_counter_ >= sensor_error_threshold_) {
      new_data_ = true;
      sensor_error_ = true;
    }
  } else {
    if (sensor_failure_counter_ == 0) {
      new_data_ = true;
      sensor_error_ = false;
    }
  }
}

void Thermostat::setOutsideTemperature(int16_t value) {
  outside_temperature_ = value;
  outside_temperature_received_ = true;
}

int16_t Thermostat::getOutsideTemperature(void) {
  return outside_temperature_;
}

void Thermostat::setSensorCalibData(int16_t factor, int16_t offset, bool calib) {
  if (temperature_offset_ != offset) {
    temperature_offset_ = offset;
    new_calib_   = calib;
  }
  if (temperature_factor_ != factor) {
    if (factor != 0) {
      temperature_factor_ = factor;
      new_calib_   = calib;
    }
  }
}

void Thermostat::quick_sort(int16_t arr[], int16_t left, int16_t right) {
  int16_t i = left, j = right;
  int16_t tmp;
  int16_t pivot = arr[(left + right) / 2];

  /* partition */
  while (i <= j) {
    while (arr[i] < pivot)
      i++;
    while (arr[j] > pivot)
      j--;
    if (i <= j) {
      tmp = arr[i];
      arr[i] = arr[j];
      arr[j] = tmp;
      i++;
      j--;
    }
  }

  /* recursion */
  if (left < j)
    Thermostat::quick_sort(arr, left, j);
  if (i < right)
    Thermostat::quick_sort(arr, i, right);
}
