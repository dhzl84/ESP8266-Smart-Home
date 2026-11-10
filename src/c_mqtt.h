#ifndef C_MQTT_H_
#define C_MQTT_H_

#include "Arduino.h"

typedef enum {
  kTemp,
  kHum,
  kIP,  /* DEPRECATED */
  kCalibF,  /* DEPRECATED */
  kCalibO,  /* DEPRECATED */
  kFW,  /* DEPRECATED */
  kHysteresis,  /* DEPRECATED */
  kSensorState  /* DEPRECATED */
} Sensor_t;

typedef enum {
  kSensFail,  /* DEPRECATED */
  kThermostatState
} BinarySensor_t;

typedef enum {
  kRestart_switch,
  kUpdate_switch
} Switch_t;

typedef enum {
  kRestart_button,
  kUpdate_button
} Button_t;

class mqttHelper {
 public:
  mqttHelper(void);
  ~mqttHelper(void);
  void   setup(String nodeId);
  void   setTriggerDiscovery(bool discover);
  bool   getTriggerDiscovery(void);
  void   setTriggerRemoveDiscovered(bool remove);
  bool   getTriggerRemoveDiscovered(void);
  /* topic getters return cached topic strings built once in setup(), so that the
     MQTT callback and the publish path do not allocate a new String per call */
  String getTopicUpdateFirmware(void);
  String getTopicChangeName(void);
  String getTopicLastWill(void);
  String getTopicSystemRestartRequest(void);
  String getTopicChangeSensorCalib(void);
  String getTopicChangeHysteresis(void);
  String getTopicTargetTempCmd(void);
  String getTopicThermostatModeCmd(void);
  String getTopicHassDiscoveryClimate(void);
  String getTopicHassDiscoveryBinarySensor(BinarySensor_t binarySensor);
  String getTopicHassDiscoverySensor(Sensor_t sensor);
  String getTopicHassDiscoverySwitch(Switch_t switches);
  String getTopicHassDiscoveryButton(Button_t buttons);
  String getTopicData(void);
  String getTopicOutsideTemperature(void);
  /* parameters are passed by const reference: 12 String copies per publish avoided */
  /* state JSON is formatted into the caller's buffer (no heap String at all on the
     publish path); returns the number of bytes written, -1 if the buffer was too small */
  int buildStateJSON(char* buf, size_t size, const char* name, const char* temp, const char* humid, const char* hysteresis, const char* actState, const char* tarTemp, const char* sensError, const char* thermoMode, int16_t calibration_factor, const char* calibration_offset, const char* ip, const char* firmware);
  String buildHassDiscoveryClimate(String name, String firmware, String model);
  String buildHassDiscoveryBinarySensor(BinarySensor_t binarySensor);
  String buildHassDiscoverySensor(String name, Sensor_t sensor);
  String buildHassDiscoveryButton(String name, Button_t buttons);
  String mqttLastErrorToString(int8_t last_error);

 private:
  void   buildBaseTopic(void);
  void   buildTopics(void);

  bool   mqttTriggerDiscovery_;
  bool   mqttTriggerRemoveDiscovered_;
  String mqttData_;
  String mqttUpdateFirmware_;
  String mqttChangeName_;
  String mqttWill_;
  String mqttSystemRestartRequest_;
  String mqttPrefix_;
  String mqttNodeId_;
  String mqttDeviceName_;
  String mqttCompDevice_;
  String mqttCompSensor_;
  String mqttCompSwitch_;
  String mqttCompButton_;
  String mqttCompBinarySensor_;
  String mqttObjectId_;
  String mqttGeneralBaseTopic_;
  String mqttHassDiscoveryTopic_;
  String mqttChangeSensorCalib_;
  String mqttChangeHysteresis_;
  String mqttThermostatModeCmd_;
  String mqttTargetTempCmd_;
  String mqttOutsideTemperature_;
  /* fully qualified topics, built once per connection in buildTopics() */
  String mqttTopicData_;
  String mqttTopicUpdateFirmware_;
  String mqttTopicChangeName_;
  String mqttTopicLastWill_;
  String mqttTopicSystemRestartRequest_;
  String mqttTopicChangeSensorCalib_;
  String mqttTopicChangeHysteresis_;
  String mqttTopicTargetTempCmd_;
  String mqttTopicThermostatModeCmd_;
  String mqttTopicOutsideTemperature_;
  String mqttTopicDiscoveryClimate_;
  String mqttTopicDiscoveryBinarySensorState_;
  String mqttTopicDiscoverySensorTemp_;
  String mqttTopicDiscoverySensorHum_;
  String mqttTopicDiscoveryButtonUpdateButton_;
  String mqttTopicDiscoveryButtonRestartButton_;
};

#endif  // C_MQTT_H_
