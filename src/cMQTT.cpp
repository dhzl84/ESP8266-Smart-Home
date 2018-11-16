#include "cMQTT.h"

mqttHelper::mqttHelper()  { mqttHelper::init(); }
mqttHelper::~mqttHelper() {}

void mqttHelper::init()
{
   mqttMainTopic = "unknown";
   mqttName = "unknown";
   nameChanged = false;
}

void mqttHelper::setup(String mainTopic, String deviceName)
{
   mqttName      = deviceName;
   mqttMainTopic = mainTopic;
   mqttHelper::buildTopics();
}

void mqttHelper::setup(String mainTopic)
{
   mqttMainTopic = mainTopic;
   mqttHelper::buildTopics();
}

void mqttHelper::buildTopics(void)
{
   mqttHassDiscoveryTopic =      "homeassistant/climate/" + mqttName + "/config";
   mqttData =                    mqttMainTopic + mqttName + "/data";
// sub
   mqttUpdateFirmware =          mqttMainTopic + mqttName + "/updateFirmware";
   mqttChangeName =              mqttMainTopic + mqttName + "/changeName";
   mqttSystemRestartRequest =    mqttMainTopic + mqttName + "/systemRestartRequest";
   mqttChangeSensorCalib =       mqttMainTopic + mqttName + "/changeSensorCalib";
   mqttTargetTempCmd =           mqttMainTopic + mqttName + "/targetTempCmd";
   mqttThermostatModeCmd =       mqttMainTopic + mqttName + "/thermostatModeCmd";

//pub
   mqttUpdateFirmwareAccepted =  mqttMainTopic + mqttName + "/updateFirmwareAccepted";
   mqttWill =                    mqttMainTopic + mqttName + "/online";

}

String mqttHelper::buildJSON(String temp, String humid, String actState, String tarTemp, String sensError, String thermoMode, String calibF, String calibO, String ip, String firmware)
{
   String JSON = \
   "{\n" \
   "  \"name\":\"" + mqttName + "\",\n" \
   "  \"device_class\":\"climate\",\n" \
   "  \"mode\":\"" + thermoMode + "\",\n" \
   "  \"state\":\"" + actState + "\",\n" \
   "  \"target_temperature\":\"" + tarTemp + "\",\n" \
   "  \"current_temperature\":\"" + temp + "\",\n" \
   "  \"current_humidity\":\"" + humid + "\",\n" \
   "  \"sensor_status\":\"" + sensError + "\",\n" \
   "  \"sensor_scaling\":\"" + calibF + "\",\n" \
   "  \"sensor_offset\":\"" + calibO + "\",\n" \
   "  \"firmware_version\":\"" + firmware + "\",\n" \
   "  \"ip\":\"" + ip + "\"\n "\
   "}";

   return (JSON);
}

String mqttHelper::buildHassDiscovery(void)
{
   String JSON = \
   "{\n" \
   "  \"name\":\"" + mqttName + "\",\n" \
   "  \"device_class\":\"climate\",\n" \
   "  \"mode_command_topic\":\"" + mqttThermostatModeCmd + "\",\n" \
   "  \"mode_state_topic\":\"" + mqttData + "\",\n" \
   "  \"mode_state_template\":\"{{value_json.mode}}\",\n" \
   "  \"availability_topic\":\"" + mqttWill + "\",\n" \
   "  \"payload_available\":\"online\",\n" \
   "  \"payload_not_available\":\"offline\",\n" \
   "  \"temperature_command_topic\":\"" + mqttTargetTempCmd + "\",\n" \
   "  \"temperature_state_topic\":\"" + mqttData + "\",\n" \
   "  \"temperature_state_template\":\"{{value_json.target_temperature}}\",\n" \
   "  \"current_temperature_topic\":\"" + mqttData + "\",\n" \
   "  \"current_temperature_template\":\"{{value_json.current_temperature}}\",\n" \
   "  \"min_temp\":\"15\",\n" \
   "  \"max_temp\":\"25\",\n" \
   "  \"temp_step\":\"0.5\",\n" \
   "  \"modes\":[\"off\", \"heat\"]\n" \
   "}";

   return (JSON);
}

void mqttHelper::changeName(String value)
{
   if(value != mqttName)
   {
      mqttName = value;
      mqttHelper::buildTopics();
      nameChanged = true;
   }
}


void mqttHelper::setName(String value)
{
   if(value != mqttName)
   {
      mqttName = value;
      mqttHelper::buildTopics();
      nameChanged = false;
   }
}

void mqttHelper::resetNameChanged(void)                  { nameChanged = false; }
bool mqttHelper::getNameChanged(void)                    { return nameChanged; }
String mqttHelper::getName(void)                         { return mqttName; }
String mqttHelper::getTopicUpdateFirmware(void)          { return mqttUpdateFirmware;  }
String mqttHelper::getTopicUpdateFirmwareAccepted(void)  { return mqttUpdateFirmwareAccepted; }
String mqttHelper::getTopicChangeName(void)              { return mqttChangeName; }
String mqttHelper::getTopicState(void)                   { return mqttWill; }
String mqttHelper::getTopicSystemRestartRequest(void)    { return mqttSystemRestartRequest; }
String mqttHelper::getTopicChangeSensorCalib(void)       { return mqttChangeSensorCalib; }
String mqttHelper::getTopicTargetTempCmd(void)           { return mqttTargetTempCmd; }
String mqttHelper::getTopicThermostatModeCmd(void)       { return mqttThermostatModeCmd; }
String mqttHelper::getTopicHassDiscovery(void)           { return mqttHassDiscoveryTopic; }
String mqttHelper::getTopicData(void)                    { return mqttData; }
