#include "MQTT.h"

mqttConfig::mqttConfig()
{
   mqttConfig::init();
}

mqttConfig::~mqttConfig() {}

void mqttConfig::init()
{
   mqttMainTopic = "unknown";
   mqttName = "unknown";
   nameChanged = false;
}

void mqttConfig::setup(String mainTopic, String deviceName)
{
   mqttName      = deviceName;
   mqttMainTopic = mainTopic;
   mqttConfig::buildTopics();
}

void mqttConfig::setup(String mainTopic)
{
   mqttMainTopic = mainTopic;
   mqttConfig::buildTopics();
}

void mqttConfig::buildTopics(void)
{
   mqttFirmwareVersion =         mqttMainTopic + mqttName + "/firmwareVersion";
   mqttUpdateFirmware =          mqttMainTopic + mqttName + "/updateFirmware";
   mqttUpdateFirmwareAccepted =  mqttMainTopic + mqttName + "/updateFirmwareAccepted";
   mqttChangeName =              mqttMainTopic + mqttName + "/changeName";
   mqttState =                   mqttMainTopic + mqttName + "/online";
   mqttDeviceIP          =       mqttMainTopic + mqttName + "/deviceIP";
   mqttSystemRestartRequest =    mqttMainTopic + mqttName + "/systemRestartRequest";

   #if CFG_SENSOR
   mqttTemp =                    mqttMainTopic + mqttName + "/temp";
   mqttHum =                     mqttMainTopic + mqttName + "/hum";
   mqttSensorStatus =            mqttMainTopic + mqttName + "/sensorStatus";
   mqttChangeSensorCalib =       mqttMainTopic + mqttName + "/changeSensorCalib";
   mqttSensorCalibFactor =       mqttMainTopic + mqttName + "/sensorCalibFactor";
   mqttSensorCalibOffset =       mqttMainTopic + mqttName + "/sensorCalibOffset";
   #endif

   #if CFG_HEATING_CONTROL
   mqttTargetTempCmd =           mqttMainTopic + mqttName + "/targetTempCmd";
   mqttTargetTempState =         mqttMainTopic + mqttName + "/targetTempState";
   mqttHeatingState =            mqttMainTopic + mqttName + "/heatingState";
   mqttHeatingAllowedCmd =       mqttMainTopic + mqttName + "/heatingAllowedCmd";
   mqttHeatingAllowedState =     mqttMainTopic + mqttName + "/heatingAllowedState";
   #endif

   #if CFG_DEVICE == cS20
   mqttS20State   =              mqttMainTopic + mqttName + "/state";
   mqttS20Command =              mqttMainTopic + mqttName + "/command";
   #endif
}

void mqttConfig::changeName(String value)
{
   if(value != mqttName)
   {
      mqttName = value;
      mqttConfig::buildTopics();
      nameChanged = true;
   }
}


void mqttConfig::setName(String value)
{
   if(value != mqttName)
   {
      mqttName = value;
      mqttConfig::buildTopics();
      nameChanged = false;
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
String mqttConfig::getTopicFirmwareVersion(void)
{
   return mqttFirmwareVersion;
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
