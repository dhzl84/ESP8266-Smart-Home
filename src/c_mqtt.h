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
  void   buildBaseTopic(void);
  void   setTriggerDiscovery(bool discover);
  bool   getTriggerDiscovery(void);
  void   setTriggerRemoveDiscovered(bool remove);
  bool   getTriggerRemoveDiscovered(void);
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
  String buildStateJSON(String name, String temp, String humid, String hysteresis, String actState, String tarTemp, String sensError, String thermoMode, String calibration_factor, String calibration_offset, String ip, String firmware);
  String buildHassDiscoveryClimate(String name, String firmware, String model);
  String buildHassDiscoveryBinarySensor(String name, BinarySensor_t binarySensor);
  String buildHassDiscoverySensor(String name, Sensor_t sensor);
  String buildHassDiscoveryButton(String name, Button_t buttons);
  String buildHassDiscoverySwitch(String name, Switch_t switches);
  String getTopicData(void);
  String getTopicOutsideTemperature(void);

 private:
  bool   mqttTriggerDiscovery_;
  bool   mqttTriggerRemoveDiscovered_;
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
};

#endif  // C_MQTT_H_
