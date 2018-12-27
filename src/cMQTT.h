#ifndef cMQTT_H_
#define cMQTT_H_

#include "Arduino.h"

#ifndef MQTT_RECONNECT_TIME
  #define MQTT_RECONNECT_TIME 5000   /* 5 s in milliseconds */
#endif

typedef enum {
  sTemp,
  sHum,
  sIP,
  sCalibF,
  sCalibO,
  sFW,
  sHysteresis,
  sState
}sensor_t;

typedef enum {
  bsSensFail,
  bsState
}binarySensor_t;

typedef enum {
  swReset,
  swUpdate
}switch_t;

class mqttHelper {
public:
  mqttHelper(void);
  ~mqttHelper(void);
  void   init(void);
  void   setup(String name);
  void   changeName(String value);
  void   setName(String value);
  void   buildTopics(void);
  bool   getNameChanged();
  void   resetNameChanged();
  String getName(void);
  String getLoweredName(void);
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
  String buildStateJSON(String Temp, String humid, String hysteresis, String actState, String tarTemp, String sensError, String thermoMode, String calibF, String calibO, String ip, String firmware);
  String buildHassDiscoveryClimate(void);
  String buildHassDiscoveryBinarySensor(binarySensor_t binarySensor);
  String buildHassDiscoverySensor(sensor_t sensor);
  String buildHassDiscoverySwitch(switch_t switches);
  String getTopicData(void);

private:
  bool   nameChanged;
  String mqttData;
  String mqttUpdateFirmware;
  String mqttUpdateFirmwareAccepted;
  String mqttChangeName;
  String mqttWill;
  String mqttSystemRestartRequest;
  String mqttSystemRestartResponse;
  String mqttPrefix;
  String mqttNodeId;
  String loweredMqttNodeId;
  String mqttCompClimate;
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

#endif /* cMQTT_H_ */
