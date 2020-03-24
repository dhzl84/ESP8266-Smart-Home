#ifndef CMQTT_H_
#define CMQTT_H_

#include "Arduino.h"

#ifndef MQTT_RECONNECT_TIME
  #define MQTT_RECONNECT_TIME 5000   /* 5 s in milliseconds */
#endif

typedef enum {
  sTemp,
  sHum,
  sIP,  /* DEPRECATED */
  sCalibF,  /* DEPRECATED */
  sCalibO,  /* DEPRECATED */
  sFW,  /* DEPRECATED */
  sHysteresis,  /* DEPRECATED */
  sState  /* DEPRECATED */
}sensor_t;

/* DEPRECATED */
typedef enum {
  bsSensFail,
  bsState
}binarySensor_t;

typedef enum {
  swRestart,
  swUpdate
}switch_t;

class mqttHelper {
 public:
  mqttHelper(void);
  ~mqttHelper(void);
  void   setup(void);
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
  String getTopicHassDiscoveryBinarySensor(binarySensor_t binarySensor);
  String getTopicHassDiscoverySensor(sensor_t sensor);
  String getTopicHassDiscoverySwitch(switch_t switches);
  String buildStateJSON(String name, String Temp, String humid, String hysteresis, String actState, String tarTemp, String sensError, String thermoMode, String calibF, String calibO, String ip, String firmware);
  String buildHassDiscoveryClimate(String name, String firmware);
  String buildHassDiscoveryBinarySensor(String name, binarySensor_t binarySensor);
  String buildHassDiscoverySensor(String name, sensor_t sensor);
  String buildHassDiscoverySwitch(String name, switch_t switches);
  String getTopicData(void);

 private:
  bool   mqttTriggerDiscovery;
  bool   mqttTriggerUndiscover;
  String mqttData;
  String mqttUpdateFirmware;
  String mqttUpdateFirmwareAccepted;
  String mqttChangeName;
  String mqttWill;
  String mqttSystemRestartRequest;
  String mqttSystemRestartResponse;
  String mqttPrefix;
  String mqttNodeId;
  String mqttDeviceName;
  String mqttcompDevice;
  String mqttCompSensor;
  String mqttCompSwitch;
  String mqttCompBinarySensor;
  String mqttObjectId;
  String mqttGeneralBaseTopic;
  String mqttHassDiscoveryTopic;
  String mqttChangeSensorCalib;
  String mqttChangeHysteresis;
  String mqttThermostatModeCmd;
  String mqttTargetTempCmd;
};

#endif  // CMQTT_H_
