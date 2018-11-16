#ifndef cMQTT_H_
#define cMQTT_H_

#include "Arduino.h"

#ifndef MQTT_MAIN_TOPIC
  #define MQTT_MAIN_TOPIC             "/heating/"
#endif

#ifndef MQTT_RECONNECT_TIME
  #define MQTT_RECONNECT_TIME 10000   /* 10s in milliseconds */
#endif

class mqttConfig
{
public:
   mqttConfig(void);
   ~mqttConfig(void);
   void   init(void);
   void   setup(String mainTopic, String deviceName);
   void   setup(String mainTopic);
   void   changeName(String value);
   void   setName(String value);
   void   buildTopics(void);
   bool   getNameChanged();
   void   resetNameChanged();
   String getName(void);
   String getTopicFirmwareVersion(void);
   String getTopicUpdateFirmware(void);
   String getTopicUpdateFirmwareAccepted(void);
   String getTopicChangeName(void);
   String getTopicState(void);
   String getTopicDeviceIP(void);
   String getTopicSystemRestartRequest(void);
   String getTopicTemp(void);
   String getTopicHum(void);
   String getTopicSensorStatus(void);
   String getTopicChangeSensorCalib(void);
   String getTopicSensorCalibFactor(void);
   String getTopicSensorCalibOffset(void);
   String getTopicTargetTempCmd(void);
   String getTopicTargetTempState(void);
   String getTopicActualState(void);
   String getTopicThermostatModeCmd(void);
   String getTopicThermostatModeState(void);

private:
   String mqttName;
   String mqttFirmwareVersion;
   String mqttUpdateFirmware;
   String mqttUpdateFirmwareAccepted;
   String mqttChangeName;
   String mqttState;
   String mqttDeviceIP;
   String mqttSystemRestartRequest;
   String mqttMainTopic;
   bool   nameChanged;
   String mqttTemp;
   String mqttHum;
   String mqttSensorStatus;
   String mqttChangeSensorCalib;
   String mqttSensorCalibFactor;
   String mqttSensorCalibOffset;
   String mqttActualState;
   String mqttThermostatModeCmd;
   String mqttThermostatModeState;
   String mqttTargetTempCmd;
   String mqttTargetTempState;
};

#endif /* cMQTT_H_ */
