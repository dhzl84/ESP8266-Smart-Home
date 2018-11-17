#ifndef cMQTT_H_
#define cMQTT_H_

#include "Arduino.h"

#ifndef MQTT_MAIN_TOPIC
  #define MQTT_MAIN_TOPIC "homeassistant/climate/"
#endif

#ifndef MQTT_RECONNECT_TIME
  #define MQTT_RECONNECT_TIME 10000   /* 10s in milliseconds */
#endif

class mqttHelper
{
public:
   mqttHelper(void);
   ~mqttHelper(void);
   void   init(void);
   void   setup(String mainTopic, String deviceName);
   void   setup(String mainTopic);
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
   String getTopicTargetTempCmd(void);
   String getTopicThermostatModeCmd(void);
   String getTopicHassDiscovery(void);
   String buildStateJSON(String Temp, String humid, String actState, String tarTemp, String sensError, String thermoMode, String calibF, String calibO, String ip, String firmware);
   String buildHassDiscovery(void);
   String getTopicData(void);

private:
   String mqttName;
   String loweredMqttName;
   String mqttData;
   String mqttUpdateFirmware;
   String mqttUpdateFirmwareAccepted;
   String mqttChangeName;
   String mqttWill;
   String mqttSystemRestartRequest;
   String mqttMainTopic;
   String mqttHassDiscoveryTopic;
   bool   nameChanged;
   String mqttChangeSensorCalib;
   String mqttThermostatModeCmd;
   String mqttTargetTempCmd;
};

#endif /* cMQTT_H_ */
