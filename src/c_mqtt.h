#ifndef C_MQTT_H_
#define C_MQTT_H_

#include "Arduino.h"

#ifndef MQTT_RECONNECT_TIME
  #define MQTT_RECONNECT_TIME 5000   /* 5 s in milliseconds */
#endif

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
  kRestart,
  kUpdate
} Switch_t;

class mqttHelper {
 public:
  mqttHelper(void);
  ~mqttHelper(void);
  void   setup(String nodeId);
  void   buildBaseTopic(void);
  void   setTriggerDiscovery(bool discover);
  bool   getTriggerDiscovery(void);
  void   setTriggerUndiscover(bool undiscover);
  bool   getTriggerUndiscover(void);
  String getTopicUpdateFirmware(void);
  String getTopicUpdateFirmwareAccepted(void);
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
  String buildStateJSON(String name, String Temp, String humid, String hysteresis, String actState, String tarTemp, String sensError, String thermoMode, String calibration_factor, String calibration_offset, String ip, String firmware);
  String buildHassDiscoveryClimate(String name, String firmware, String model);
  String buildHassDiscoveryBinarySensor(String name, BinarySensor_t binarySensor);
  String buildHassDiscoverySensor(String name, Sensor_t sensor);
  String buildHassDiscoverySwitch(String name, Switch_t switches);
  String getTopicData(void);

 private:
  bool   mqttTriggerDiscovery_;
  bool   mqttTriggerUndiscover_;
  String mqttData_;
  String mqttUpdateFirmware_;
  String mqttUpdateFirmwareAccepted_;
  String mqttChangeName_;
  String mqttWill_;
  String mqttSystemRestartRequest_;
  String mqttPrefix_;
  String mqttNodeId_;
  String mqttDeviceName_;
  String mqttCompDevice_;
  String mqttCompSensor_;
  String mqttCompSwitch_;
  String mqttCompBinarySensor_;
  String mqttObjectId_;
  String mqttGeneralBaseTopic_;
  String mqttHassDiscoveryTopic_;
  String mqttChangeSensorCalib_;
  String mqttChangeHysteresis_;
  String mqttThermostatModeCmd_;
  String mqttTargetTempCmd_;
};

#endif  // C_MQTT_H_
