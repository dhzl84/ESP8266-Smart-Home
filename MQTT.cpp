#include "MQTT.h"

#ifndef MQTT_MAIN_TOPIC
#if CFG_MQTT_CLIENT
#warning "MQTT topic not defined"
#endif
#define MQTT_MAIN_TOPIC "none"
#endif

mqttConfig::mqttConfig()
{
   mqttConfig::init();
}

mqttConfig::~mqttConfig()
{

}

void mqttConfig::init()
{
   mqttConfig::changeName("unknown");
   nameChanged = false;
}

void mqttConfig::changeName(String deviceID)
{
   mqttName =                    deviceID;
   mqttUpdateFirmware =          MQTT_MAIN_TOPIC + deviceID + "/updateFirmware";
   mqttUpdateFirmwareAccepted =  MQTT_MAIN_TOPIC + deviceID + "/updateFirmwareAccepted";
   mqttChangeName =              MQTT_MAIN_TOPIC + deviceID + "/changeName";
   mqttState =                   MQTT_MAIN_TOPIC + deviceID + "/online";
   mqttDeviceIP          =       MQTT_MAIN_TOPIC + deviceID + "/deviceIP";
   mqttSystemRestartRequest =    MQTT_MAIN_TOPIC + deviceID + "/systemRestartRequest";

   #if CFG_SENSOR
   mqttTemp =                    MQTT_MAIN_TOPIC + deviceID + "/temp";
   mqttHum =                     MQTT_MAIN_TOPIC + deviceID + "/hum";
   mqttSensorStatus =            MQTT_MAIN_TOPIC + deviceID + "/sensorStatus";
   mqttChangeSensorCalib =       MQTT_MAIN_TOPIC + deviceID + "/changeSensorCalib";
   mqttSensorCalibFactor =       MQTT_MAIN_TOPIC + deviceID + "/sensorCalibFactor";
   mqttSensorCalibOffset =       MQTT_MAIN_TOPIC + deviceID + "/sensorCalibOffset";
   #endif

   #if CFG_HEATING_CONTROL
   mqttTargetTempCmd =           MQTT_MAIN_TOPIC + deviceID + "/targetTempCmd";
   mqttTargetTempState =         MQTT_MAIN_TOPIC + deviceID + "/targetTempState";
   mqttHeatingState =            MQTT_MAIN_TOPIC + deviceID + "/heatingState";
   mqttHeatingAllowedCmd =       MQTT_MAIN_TOPIC + deviceID + "/heatingAllowedCmd";
   mqttHeatingAllowedState =     MQTT_MAIN_TOPIC + deviceID + "/heatingAllowedState";
   #endif

   #if CFG_DEVICE == cS20
   mqttS20State   =              MQTT_MAIN_TOPIC + deviceID + "/state";
   mqttS20Command =              MQTT_MAIN_TOPIC + deviceID + "/command";
   #endif
}

void mqttConfig::setName(String value)
{
   if(value != mqttName)
   {
      mqttConfig::changeName(value);
      nameChanged = true;
   }
}

String mqttConfig::getName(void)
{
   return mqttName;
}

void mqttConfig::resetNameChanged(void)
{
   nameChanged = false;
}

bool mqttConfig::getNameChanged(void)
{
   return nameChanged;
}
String mqttConfig::getTopicUpdateFirmware(void)
{
   return mqttUpdateFirmware;
}
String mqttConfig::getTopicUpdateFirmwareAccepted(void)
{
   return mqttUpdateFirmwareAccepted;
}
String mqttConfig::getTopicChangeName(void)
{
   return mqttChangeName;
}
String mqttConfig::getTopicState(void)
{
   return mqttState;
}
String mqttConfig::getTopicDeviceIP(void)
{
   return mqttDeviceIP;
}
String mqttConfig::getTopicSystemRestartRequest(void)
{
   return mqttSystemRestartRequest;
}
#if CFG_SENSOR
String mqttConfig::getTopicTemp(void)
{
   return mqttTemp;
}
String mqttConfig::getTopicHum(void)
{
   return mqttHum;
}
String mqttConfig::getTopicSensorStatus(void)
{
   return mqttSensorStatus;
}
String mqttConfig::getTopicChangeSensorCalib(void)
{
   return mqttChangeSensorCalib;
}
String mqttConfig::getTopicSensorCalibFactor(void)
{
   return mqttSensorCalibFactor;
}
String mqttConfig::getTopicSensorCalibOffset(void)
{
   return mqttSensorCalibOffset;
}
#endif /* CFG_SENSOR */

#if CFG_HEATING_CONTROL
String mqttConfig::getTopicTargetTempCmd(void)
{
   return mqttTargetTempCmd;
}
String mqttConfig::getTopicTargetTempState(void)
{
   return mqttTargetTempState;
}
String mqttConfig::getTopicHeatingState(void)
{
   return mqttHeatingState;
}
String mqttConfig::getTopicHeatingAllowedCmd(void)
{
   return mqttHeatingAllowedCmd;
}
String mqttConfig::getTopicHeatingAllowedState(void)
{
   return mqttHeatingAllowedState;
}
#endif /* CFG_HEATING_CONTROL */

#if CFG_DEVICE == cS20
String mqttConfig::getTopicS20State(void)
{
   return mqttS20State;
}
String mqttConfig::getTopicS20Command(void)
{
   return mqttS20Command;
}
#endif
