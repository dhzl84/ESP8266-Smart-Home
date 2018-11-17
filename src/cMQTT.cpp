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
   mqttMainTopic   = mainTopic;
   mqttName        = deviceName;
   loweredMqttName = deviceName;
   loweredMqttName.toLowerCase();
   mqttHelper::buildTopics();
}

void mqttHelper::setup(String mainTopic)
{
   mqttMainTopic = mainTopic;
   mqttHelper::buildTopics();
}

void mqttHelper::buildTopics(void)
{
   mqttHassDiscoveryTopic =      mqttMainTopic + loweredMqttName + "/config";
   mqttData =                    mqttMainTopic + loweredMqttName + "/state";
// sub
   mqttUpdateFirmware =          mqttMainTopic + loweredMqttName + "/updateFirmware";
   mqttChangeName =              mqttMainTopic + loweredMqttName + "/changeName";
   mqttSystemRestartRequest =    mqttMainTopic + loweredMqttName + "/systemRestartRequest";
   mqttChangeSensorCalib =       mqttMainTopic + loweredMqttName + "/changeSensorCalib";
   mqttTargetTempCmd =           mqttMainTopic + loweredMqttName + "/targetTempCmd";
   mqttThermostatModeCmd =       mqttMainTopic + loweredMqttName + "/thermostatModeCmd";
//pub
   mqttUpdateFirmwareAccepted =  mqttMainTopic + loweredMqttName + "/updateFirmwareAccepted";
   mqttWill =                    mqttMainTopic + loweredMqttName + "/LWT";

}

String mqttHelper::buildStateJSON(String temp, String humid, String actState, String tarTemp, String sensError, String thermoMode, String calibF, String calibO, String ip, String firmware)
{
   String JSON = \
   "{\n" \
   "  \"name\":\"" + mqttName + "\",\n" \
   "  \"dev_cla\":\"climate\",\n" \
   "  \"mode\":\"" + thermoMode + "\",\n" \
   "  \"state\":\"" + actState + "\",\n" \
   "  \"target_temp\":\"" + tarTemp + "\",\n" \
   "  \"current_temp\":\"" + temp + "\",\n" \
   "  \"humidity\":\"" + humid + "\",\n" \
   "  \"sens_status\":\"" + sensError + "\",\n" \
   "  \"sens_scale\":\"" + calibF + "\",\n" \
   "  \"sens_offs\":\"" + calibO + "\",\n" \
   "  \"fw\":\"" + firmware + "\",\n" \
   "  \"ip\":\"" + ip + "\"\n "\
   "}";

   return (JSON);
}

String mqttHelper::buildHassDiscovery(void)
{
   String JSON = \
   "{\n" \
   "  \"name\":\"" + mqttName + "\",\n" \
   "  \"dev_cla\":\"climate\",\n" \
   "  \"mode_cmd_t\":\"" + mqttThermostatModeCmd + "\",\n" \
   "  \"mode_stat_t\":\"" + mqttData + "\",\n" \
   "  \"mode_stat_tpl\":\"{{value_json.mode}}\",\n" \
   "  \"avty_t\":\"" + mqttWill + "\",\n" \
   "  \"pl_avail\":\"online\",\n" \
   "  \"pl_not_avail\":\"offline\",\n" \
   "  \"temp_cmd_t\":\"" + mqttTargetTempCmd + "\",\n" \
   "  \"temp_stat_t\":\"" + mqttData + "\",\n" \
   "  \"temp_stat_tpl\":\"{{value_json.target_temp}}\",\n" \
   "  \"curr_temp_t\":\"" + mqttData + "\",\n" \
   "  \"current_temperature_template\":\"{{value_json.current_temp}}\",\n" \
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
      loweredMqttName = value;
      loweredMqttName.toLowerCase();
      mqttHelper::buildTopics();
      nameChanged = true;
   }
}

void mqttHelper::setName(String value)
{
   if(value != mqttName)
   {
      mqttName = value;
      loweredMqttName = value;
      loweredMqttName.toLowerCase();
      mqttHelper::buildTopics();
      nameChanged = false;
   }
}

void   mqttHelper::resetNameChanged(void)                { nameChanged = false; }
bool   mqttHelper::getNameChanged(void)                  { return nameChanged; }
String mqttHelper::getName(void)                         { return mqttName; }
String mqttHelper::getLoweredName(void)                  { return loweredMqttName; }
String mqttHelper::getTopicUpdateFirmware(void)          { return mqttUpdateFirmware;  }
String mqttHelper::getTopicUpdateFirmwareAccepted(void)  { return mqttUpdateFirmwareAccepted; }
String mqttHelper::getTopicChangeName(void)              { return mqttChangeName; }
String mqttHelper::getTopicLastWill(void)                { return mqttWill; }
String mqttHelper::getTopicSystemRestartRequest(void)    { return mqttSystemRestartRequest; }
String mqttHelper::getTopicChangeSensorCalib(void)       { return mqttChangeSensorCalib; }
String mqttHelper::getTopicTargetTempCmd(void)           { return mqttTargetTempCmd; }
String mqttHelper::getTopicThermostatModeCmd(void)       { return mqttThermostatModeCmd; }
String mqttHelper::getTopicHassDiscovery(void)           { return mqttHassDiscoveryTopic; }
String mqttHelper::getTopicData(void)                    { return mqttData; }
