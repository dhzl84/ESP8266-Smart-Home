#ifndef CMQTT_H_
#define CMQTT_H_

#include "Arduino.h"

#ifndef MQTT_RECONNECT_TIME
  #define MQTT_RECONNECT_TIME 5000   /* 5 s in milliseconds */
#endif

typedef enum {
  sIP,
  sFW
}sensor_t;

typedef enum {
  none
}binarySensor_t;

typedef enum {
  swRestart,
  swUpdate
}switch_t;

class mqttHelper {
 public:
  mqttHelper(void);
  ~mqttHelper(void);
  void   init(void);
  void   setup(void);
  void   buildTopics(void);
  void   setTriggerDiscovery(bool discover);
  bool   getTriggerDiscovery(void);
  String getTopicUpdateFirmware(void);
  String getTopicUpdateFirmwareAccepted(void);
  String getTopicChangeName(void);
  String getTopicLastWill(void);
  String getTopicSystemRestartRequest(void);
  String getTopicRfCommand(void);
  String getTopicHassDiscoveryDevice(void);
  String getTopicHassDiscoveryBinarySensor(binarySensor_t binarySensor);
  String getTopicHassDiscoverySensor(sensor_t sensor);
  String getTopicHassDiscoverySwitch(switch_t switches);
  String buildStateJSON(String name, String rfCode, String ip, String firmware);
  String buildHassDiscoveryDevice(String name, String firmware);
  String buildHassDiscoveryBinarySensor(String name, binarySensor_t binarySensor);
  String buildHassDiscoverySensor(String name, sensor_t sensor);
  String buildHassDiscoverySwitch(String name, switch_t switches);
  String getTopicData(void);

 private:
  bool   nameChanged;
  bool   triggerDiscovery;
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
  String mqttCompDevice;
  String mqttCompSensor;
  String mqttCompSwitch;
  String mqttCompBinarySensor;
  String mqttObjectId;
  String mqttGeneralBaseTopic;
  String mqttHassDiscoveryTopic;
  String mqttRfCommand;
};

#endif  // CMQTT_H_
