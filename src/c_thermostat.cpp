#include "c_thermostat.h"
#include "print_log.h"

#include "print_log.h"  /* last include: renames the identifier Serial to the console capture tee, see print_log.h */

/* target_temperature_ : initial value if setup() is not called; resolution is 0.1 C */
/* new_data_ : flag to indicate new data to be displayed and transmitted vai MQTT */
/* relay_gpio_ : relay GPIO if setup() is not called */
/* thermostat_hysteresis_ : hysteresis initialized to 0.2 C */
/* sensor_error_ : filtered sensor error */
/* current_temperature_ : current sensor temperature */
/* current_humidity_ : current sensor humidity */
/* filtered_temperature_ : filtered sensor temperature; mean value of CFG_TEMP_SENSOR_FILTER_QUEUE_SIZE samples */
/* filtered_humidity_ : filtered sensor humidity; mean value of CFG_TEMP_SENSOR_FILTER_QUEUE_SIZE samples */
/* sensor_failure_counter_ : count sensor failures during read */
/* temperature_offset_ : offset in 0.1 C */
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
    current_temperature_(THERMOSTAT_SENSOR_INIT_VALUE), \
    current_humidity_(THERMOSTAT_SENSOR_INIT_VALUE), \
    filtered_temperature_(THERMOSTAT_SENSOR_INIT_VALUE), \
    filtered_humidity_(THERMOSTAT_SENSOR_INIT_VALUE), \
    start_averaging_(2), \
    stop_averaging_(CFG_TEMP_SENSOR_FILTER_QUEUE_SIZE - 2), \
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
    temperature_value_queue_[i] = THERMOSTAT_SENSOR_INIT_VALUE;
  }

  for (int16_t i=0; i < CFG_TEMP_SENSOR_FILTER_QUEUE_SIZE; i++) {
    humidity_value_queue_[i] = THERMOSTAT_SENSOR_INIT_VALUE;
  }
}

Thermostat::~Thermostat() {
  /* do nothing */
}

void Thermostat::setup(uint8_t gpio, int16_t tarTemp, int16_t calibFactor, int16_t calibOffset, uint8_t temperature_hysteresis, bool thermostat_mode) {
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
          logSetLevel(0);   /* level 0: the relay state the room feels, in every build */
          Serial.println("[Thermostat] heating");
        }
      } else if (filtered_temperature_ >= static_cast<int16_t>(target_temperature_ + thermostat_hysteresis_high_)) {  /* check if measured temperature is higher than heating target */
        if (actual_state_ == TH_HEAT) {  /* switch off heating if target temperature is lower than measured temperature */
          setActualState(TH_OFF);
          logSetLevel(0);   /* level 0: the relay state the room feels, in every build */
          Serial.println("[Thermostat] not heating");
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
  if hysteresis can not be applied symmetrically due to the internal resolution of 0.1 C, round up (ceil) hysteresis high and round down (floor) hysteresis low
  example:
    thermostat_hysteresis_ of 0.5 would need a resultion of 0.05 C to accomplis +/- 0.25 C
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

    /* derive the halves from the CLAMPED total, not the raw argument: with the raw value,
       h=0 kept halves of 0/0 (heating churns at the setpoint) and h=30 stored halves of
       15/15 while reporting 20 */
    if ((thermostat_hysteresis_ % 2) == 0) {
      thermostat_hysteresis_low_  = (thermostat_hysteresis_ / 2);
      thermostat_hysteresis_high_ = (thermostat_hysteresis_ / 2);
    } else {
      thermostat_hysteresis_low_  = (thermostat_hysteresis_ / 2);
      thermostat_hysteresis_high_ = (thermostat_hysteresis_ - thermostat_hysteresis_low_);
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

void Thermostat::setTargetTemperature(int16_t value) {
  if ((uint8_t)value != target_temperature_) {
    /* clamp the full int16 range BEFORE the assignment: an MQTT payload of "300" is
       int16 3000; truncating to uint8_t first turned it into 184 and silently set 18.4 °C */
    if (value > (int16_t)MAXIMUM_TARGET_TEMP) {
      target_temperature_ = MAXIMUM_TARGET_TEMP;
    } else if (value < (int16_t)MINIMUM_TARGET_TEMP) {
      target_temperature_ = MINIMUM_TARGET_TEMP;
    } else {
      target_temperature_  = (uint8_t)value;
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

/* sort the samples ascending (insertion sort). The queue holds only a few samples, so this is fast,
   needs no extra memory and no recursion, and the trimmed mean below relies on a fully sorted array */
void Thermostat::sort_samples(int16_t samples[], int16_t count) {
  for (int16_t i = 1; i < count; i++) {
    const int16_t value = samples[i];
    int16_t j = i - 1;
    while ((j >= 0) && (samples[j] > value)) {
      samples[j + 1] = samples[j];
      j--;
    }
    samples[j + 1] = value;
  }
}

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

  // copy array for sorting, the queue itself must keep the order of arrival
  int16_t sorted_temperature_value_queue[CFG_TEMP_SENSOR_FILTER_QUEUE_SIZE];
  std::copy(std::begin(temperature_value_queue_), std::end(temperature_value_queue_), std::begin(sorted_temperature_value_queue));

#if CFG_DEBUG_SELECTION_SORT
  for (auto val : temperature_value_queue_) {
    Serial.printf("[Thermostat] Value: %i\n", val);
  }
#endif  // CFG_DEBUG_SELECTION_SORT

  sort_samples(sorted_temperature_value_queue, CFG_TEMP_SENSOR_FILTER_QUEUE_SIZE);

#if CFG_DEBUG_SELECTION_SORT
  for (int16_t i = 0; i < CFG_TEMP_SENSOR_FILTER_QUEUE_SIZE; i++) {
    Serial.printf("[Thermostat] Sorted Value: %i\n", sorted_temperature_value_queue[i]);
  }
#endif  // CFG_DEBUG_SELECTION_SORT

  // calculate new filtered temperature using trimmed mean
  float tempValue = static_cast<float>(0);
  if (temperature_value_queue_filled_ == true) {
    for (int16_t i = start_averaging_; i < stop_averaging_; i++) {
      tempValue += static_cast<float>(sorted_temperature_value_queue[i]);
#if CFG_DEBUG_SELECTION_SORT
      Serial.printf("[Thermostat] Use Value: %i from index %i\n", sorted_temperature_value_queue[i], i);
#endif  // CFG_DEBUG_SELECTION_SORT
    }
    tempValue = (tempValue / static_cast<float>(filter_size_));
  } else {  /* return partially filtered value until queue is filled */
    if (temperature_value_sample_id_ > 0) {
      for (int16_t i = 0; i < temperature_value_sample_id_; i++) {
        tempValue += static_cast<float>(temperature_value_queue_[i]);
      }
      tempValue = (tempValue / static_cast<float>(temperature_value_sample_id_));
    }
  }
  filtered_temperature_ = tempValue;
}

void Thermostat::setCurrentHumidity(int16_t value) {
  current_humidity_ = value;

  // add new humidity to filter
  humidity_value_queue_[humidity_value_sample_id_] = value;
  humidity_value_sample_id_++;
  if (humidity_value_sample_id_ == CFG_TEMP_SENSOR_FILTER_QUEUE_SIZE) {
    humidity_value_sample_id_ = 0;
    if (humidity_value_queue_filled_ == false) {
      humidity_value_queue_filled_ = true;
    }
  }

  // copy array for sorting, the queue itself must keep the order of arrival
  int16_t sorted_humidity_value_queue[CFG_TEMP_SENSOR_FILTER_QUEUE_SIZE];
  std::copy(std::begin(humidity_value_queue_), std::end(humidity_value_queue_), std::begin(sorted_humidity_value_queue));

#if CFG_DEBUG_SELECTION_SORT
  for (auto val : humidity_value_queue_) {
    Serial.printf("[Thermostat] Humidity Value: %i\n", val);
  }
#endif  // CFG_DEBUG_SELECTION_SORT

  sort_samples(sorted_humidity_value_queue, CFG_TEMP_SENSOR_FILTER_QUEUE_SIZE);

#if CFG_DEBUG_SELECTION_SORT
  for (int16_t i = 0; i < CFG_TEMP_SENSOR_FILTER_QUEUE_SIZE; i++) {
    Serial.printf("[Thermostat] Sorted Humidity Value: %i\n", sorted_humidity_value_queue[i]);
  }
#endif  // CFG_DEBUG_SELECTION_SORT

  // calculate new filtered humidity using trimmed mean
  float humidValue = static_cast<float>(0);
  if (humidity_value_queue_filled_ == true) {
    for (int16_t i = start_averaging_; i < stop_averaging_; i++) {
      humidValue += static_cast<float>(sorted_humidity_value_queue[i]);
#if CFG_DEBUG_SELECTION_SORT
      Serial.printf("[Thermostat] Use Humidity Value: %i from index %i\n", sorted_humidity_value_queue[i], i);
#endif  // CFG_DEBUG_SELECTION_SORT
    }
    humidValue = (humidValue / static_cast<float>(filter_size_));
  } else {  /* return partially filtered value until queue is filled */
    if (humidity_value_sample_id_ > 0) {
      for (int16_t i = 0; i < humidity_value_sample_id_; i++) {
        humidValue += static_cast<float>(humidity_value_queue_[i]);
      }
      humidValue = (humidValue / static_cast<float>(humidity_value_sample_id_));
    }
  }
  filtered_humidity_ = humidValue;
}

void Thermostat::setLastSensorReadFailed(bool value) {
  /* filter sensor read failure here to avoid switching back and forth for single failure events */
  if (value == true) {
    if (sensor_failure_counter_ < INT8_MAX) {
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
      /* set (filtered) temperature and humidity to init value */
      current_temperature_ = THERMOSTAT_SENSOR_INIT_VALUE;
      current_humidity_ = THERMOSTAT_SENSOR_INIT_VALUE;
      filtered_temperature_ = THERMOSTAT_SENSOR_INIT_VALUE;
      filtered_humidity_ = THERMOSTAT_SENSOR_INIT_VALUE;
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

