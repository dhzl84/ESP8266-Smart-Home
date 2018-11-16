#include "cMQTT.h"

mqttConfig::mqttConfig()  { mqttConfig::init(); }
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
   mqttTemp =                    mqttMainTopic + mqttName + "/temp";
   mqttHum =                     mqttMainTopic + mqttName + "/hum";
   mqttSensorStatus =            mqttMainTopic + mqttName + "/sensorStatus";
   mqttChangeSensorCalib =       mqttMainTopic + mqttName + "/changeSensorCalib";
   mqttSensorCalibFactor =       mqttMainTopic + mqttName + "/sensorCalibFactor";
   mqttSensorCalibOffset =       mqttMainTopic + mqttName + "/sensorCalibOffset";
   mqttTargetTempCmd =           mqttMainTopic + mqttName + "/targetTempCmd";
   mqttTargetTempState =         mqttMainTopic + mqttName + "/targetTempState";
   mqttActualState =             mqttMainTopic + mqttName + "/actualState";
   mqttThermostatModeCmd =       mqttMainTopic + mqttName + "/thermostatModeCmd";
   mqttThermostatModeState =     mqttMainTopic + mqttName + "/thermostatModeState";

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

void mqttConfig::resetNameChanged(void)                  { nameChanged = false; }
bool mqttConfig::getNameChanged(void)                    { return nameChanged; }
String mqttConfig::getName(void)                         { return mqttName; }
String mqttConfig::getTopicFirmwareVersion(void)         { return mqttFirmwareVersion; }
String mqttConfig::getTopicUpdateFirmware(void)          { return mqttUpdateFirmware;  }
String mqttConfig::getTopicUpdateFirmwareAccepted(void)  { return mqttUpdateFirmwareAccepted; }
String mqttConfig::getTopicChangeName(void)              { return mqttChangeName; }
String mqttConfig::getTopicState(void)                   { return mqttState; }
String mqttConfig::getTopicDeviceIP(void)                { return mqttDeviceIP;}
String mqttConfig::getTopicSystemRestartRequest(void)    { return mqttSystemRestartRequest; }
String mqttConfig::getTopicTemp(void)                    { return mqttTemp; }
String mqttConfig::getTopicHum(void)                     { return mqttHum; }
String mqttConfig::getTopicSensorStatus(void)            { return mqttSensorStatus; }
String mqttConfig::getTopicChangeSensorCalib(void)       { return mqttChangeSensorCalib; }
String mqttConfig::getTopicSensorCalibFactor(void)       { return mqttSensorCalibFactor; }
String mqttConfig::getTopicSensorCalibOffset(void)       { return mqttSensorCalibOffset; }
String mqttConfig::getTopicTargetTempCmd(void)           { return mqttTargetTempCmd; }
String mqttConfig::getTopicTargetTempState(void)         { return mqttTargetTempState; }
String mqttConfig::getTopicActualState(void)             { return mqttActualState; }
String mqttConfig::getTopicThermostatModeCmd(void)       { return mqttThermostatModeCmd; }
String mqttConfig::getTopicThermostatModeState(void)     { return mqttThermostatModeState; }