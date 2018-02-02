#ifndef MQTT_H_
#define MQTT_H_

#include "Arduino.h"

#include "ESP8266.h" /* needed to resolve MQTT_MAIN_TOPIC and device config */

class mqttConfig
{
public:
   mqttConfig(void);
   ~mqttConfig(void);
   void   init(void);
   void   setName(String value);
   void   changeName(String value);
   bool   getNameChanged();
   void   resetNameChanged();
   String getName(void);
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

   #if CFG_DEVICE == cS20
   String getTopicS20State(void);
   String getTopicS20Command(void);
   #endif

private:
   String mqttName;
   String mqttUpdateFirmware;
   String mqttUpdateFirmwareAccepted;
   String mqttChangeName;
   String mqttState;
   String mqttDeviceIP;
   String mqttSystemRestartRequest;
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

   #if CFG_DEVICE == cS20
   String mqttS20State;
   String mqttS20Command;
   #endif

};

#endif /* MQTT_H_ */
