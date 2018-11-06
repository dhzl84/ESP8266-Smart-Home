#ifndef cMQTT_H_
#define cMQTT_H_

#include "main.h"

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

   #if CFG_SENSOR
   String getTopicTemp(void);
   String getTopicHum(void);
   String getTopicSensorStatus(void);
   String getTopicChangeSensorCalib(void);
   String getTopicSensorCalibFactor(void);
   String getTopicSensorCalibOffset(void);
   #endif

   #if CFG_HEATING_CONTROL
   String getTopicTargetTempCmd(void);
   String getTopicTargetTempState(void);
   String getTopicHeatingState(void);
   String getTopicHeatingAllowedCmd(void);
   String getTopicHeatingAllowedState(void);
   #endif

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

   #if CFG_SENSOR
   String mqttTemp;
   String mqttHum;
   String mqttSensorStatus;
   String mqttChangeSensorCalib;
   String mqttSensorCalibFactor;
   String mqttSensorCalibOffset;
   #endif

   #if CFG_HEATING_CONTROL
   String mqttHeatingState;
   String mqttHeatingAllowedCmd;
   String mqttHeatingAllowedState;
   String mqttTargetTempCmd;
   String mqttTargetTempState;
   #endif
};

#endif /* cMQTT_H_ */
